#include "dataloader.h"
#include "dataprovider.h"
#include "providerfactory.h"
#include "klinewidget.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QTextEdit>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDateTime>
#include <memory>

// NetCore RPC 客户端和数据管理器
#include "mt4rpc/KBarRpcService.h"
#include "mt4rpc/KBarManager.h"

// ============================================================
// 工具函数：KBar -> Candle 转换
// ============================================================
static Candle kbarToCandle(const KBar &kb)
{
    Candle c;
    c.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(kb.time));
    c.open = kb.open;
    c.high = kb.high;
    c.low = kb.low;
    c.close = kb.close;
    c.volume = static_cast<double>(kb.volume);
    return c;
}

static QVector<Candle> kbarVectorToCandles(const std::vector<KBar> &bars)
{
    QVector<Candle> result;
    result.reserve(static_cast<int>(bars.size()));
    for (const auto &kb : bars) {
        result.append(kbarToCandle(kb));
    }
    return result;
}

// ============================================================
// DataLoader::Impl - 封装 RPC 客户端逻辑
// ============================================================
class DataLoader::Impl {
public:
    explicit Impl(DataLoader *parent)
        : m_parent(parent)
    {
        KBarRpcService::Config cfg = loadConfig();
        m_rpc = std::make_shared<KBarRpcService>(cfg);

        // RPC 日志 -> 输出到 log widget
        m_rpc->on_log_message = [this](const std::string &msg) {
            QTextEdit *log = m_parent->getLogWidget();
            if (log) log->append(QString::fromStdString(msg));
        };

        // 连接状态回调
        m_rpc->on_connection_changed = [this](bool connected) {
            QTextEdit *log = m_parent->getLogWidget();
            if (connected) {
                if (log) log->append(QStringLiteral("[TCP] connected to %1:%2")
                    .arg(QString::fromStdString(m_rpc->config().host))
                    .arg(m_rpc->config().port));
            } else {
                if (log) log->append(QStringLiteral("[TCP] disconnected"));
            }
            emit m_parent->connectionStatusChanged(connected);
        };

        // 推送回调：收到新 K 线数据 -> 写入 KBarManager + 通知 UI
        m_rpc->on_kbar_pushed = [this](const KBar &kbar) {
            // 1) 写入 KBarManager（线程安全）
            KBarManager::instance().add_kbar(kbar);

            // 2) 通过信号通知主线程更新 UI
            emit m_parent->pushDataReady(
                QString::fromStdString(kbar.symbol),
                kbar.timeFrame,
                kbar.time,
                kbar.open, kbar.high, kbar.low, kbar.close,
                static_cast<double>(kbar.volume));
        };

        // 错误回调
        m_rpc->on_error = [this](const std::string &err) {
            QTextEdit *log = m_parent->getLogWidget();
            if (log) log->append(QStringLiteral("[TCP] error: %1").arg(QString::fromStdString(err)));
        };
    }

    ~Impl() {
        if (m_rpc) {
            m_rpc->stop();
        }
    }

    bool start() {
        if (!m_rpc) return false;
        QTextEdit *log = m_parent->getLogWidget();
        if (log) log->append(QStringLiteral("KBarRPC client initializing..."));
        return m_rpc->start();
    }

    void stop() {
        if (m_rpc) {
            m_rpc->stop();
        }
    }

    bool isRunning() const {
        return m_rpc && m_rpc->is_running();
    }

    // 通过 RPC 获取数据（同步调用，应在后台线程执行）
    bool fetchBars(const std::string &symbol, int timeFrame,
                   std::vector<KBar> &out)
    {
        if (!m_rpc || !m_rpc->is_running()) return false;
        return m_rpc->fetch_kbars(symbol, timeFrame, out);
    }

private:
    DataLoader *m_parent;
    std::shared_ptr<KBarRpcService> m_rpc;

    static KBarRpcService::Config loadConfig() {
        KBarRpcService::Config cfg;
        QFile f(QCoreApplication::applicationDirPath() + "/config/server.json");
        if (!f.exists()) {
            f.setFileName(QCoreApplication::applicationDirPath() + "/../config/server.json");
        }
        if (!f.exists()) {
            f.setFileName("config/server.json");
        }
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                cfg.host = obj.value("host").toString("127.0.0.1").toStdString();
                cfg.port = static_cast<uint16_t>(obj.value("port").toInt(9200));
                cfg.reconnect_interval_ms = obj.value("reconnect_interval_ms").toInt(3000);
                cfg.request_timeout_ms = obj.value("request_timeout_ms").toInt(10000);
                cfg.auto_reconnect = obj.value("auto_reconnect").toBool(true);
            }
            f.close();
        }
        return cfg;
    }
};

// ============================================================
// DataLoader 公开接口
// ============================================================
DataLoader::DataLoader(KLineWidget *k, QObject *parent)
    : QObject(parent), m_k(k)
    , m_impl(std::make_unique<Impl>(this))
{
}

DataLoader::~DataLoader() = default;

bool DataLoader::startRpcClient()
{
    return m_impl->start();
}

void DataLoader::stopRpcClient()
{
    m_impl->stop();
}

bool DataLoader::isRpcRunning() const
{
    return m_impl->isRunning();
}

QTextEdit *DataLoader::getLogWidget() const
{
    if (!m_k) return nullptr;
    QWidget *w = m_k->window();
    return w ? w->findChild<QTextEdit*>() : nullptr;
}

// ============================================================
// 从 KBarManager 加载并显示
// ============================================================
void DataLoader::loadFromManagerAndDisplay(const QString &symbol, int tf)
{
    std::vector<KBar> bars = KBarManager::instance().get_kbars(
        symbol.toStdString(), tf);
    if (bars.empty()) return;

    QVector<Candle> candles = kbarVectorToCandles(bars);

    // 限制最大显示条数
    if (candles.size() > 6000) {
        candles = candles.mid(candles.size() - 6000);
    }

    if (m_k) {
        m_k->setData(candles, tf);
    }
}

// ============================================================
// 核心加载流程
// ============================================================
void DataLoader::requestLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem)
{
    if (m_loading) {
        QTextEdit *log = getLogWidget();
        if (log) log->append(QStringLiteral("正在加载 %1 %2min, 请等待...").arg(symbol).arg(timeframeMinutes));
        return;
    }

    // 验证 symItem 有效性
    bool invalidItem = false;
    if (!symItem) invalidItem = true;
    else if (!symItem->treeWidget()) invalidItem = true;
    else if (symItem->childCount() > 0) invalidItem = true;
    else if (symbol.trimmed().isEmpty()) invalidItem = true;

    if (invalidItem) {
        m_loading = false;
        m_symbol.clear(); m_timeframe = timeframeMinutes; m_symItem = nullptr;
        if (m_k) {
            QVector<Candle> empty;
            m_k->setData(empty, timeframeMinutes);
        }
        QTextEdit *logText = getLogWidget();
        if (logText) logText->append(QStringLiteral("无数据：未选择品种或品种无效"));
        return;
    }

    m_loading = true;
    m_symbol = symbol;
    m_timeframe = timeframeMinutes;
    m_symItem = symItem;

    emit loadStarted(symbol, timeframeMinutes);

    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("正在加载 %1 %2min...").arg(symbol).arg(timeframeMinutes));

    // ================================================================
    // 第 0 步：检查 KBarManager 是否已有缓存
    // ================================================================
    if (KBarManager::instance().kbar_count(symbol.toStdString(), timeframeMinutes) > 0) {
        if (logText) logText->append(QStringLiteral("缓存命中，直接显示 %1 %2min").arg(symbol).arg(timeframeMinutes));
        loadFromManagerAndDisplay(symbol, timeframeMinutes);
        m_loading = false;
        emit loadFinished(symbol, timeframeMinutes, true);
        return;
    }

    // ================================================================
    // 第一步：后台线程读取本地 CSV 并写入 KBarManager
    // ================================================================
    loadLocalToManager(symbol, timeframeMinutes, symItem);

    // ================================================================
    // 第二步：后台线程通过 RPC 获取全量数据并写入 KBarManager
    // ================================================================
    fetchRpcToManager(symbol, timeframeMinutes);
}

// ============================================================
// 后台：读取本地 CSV 并写入 KBarManager
// ============================================================
void DataLoader::loadLocalToManager(const QString &symbol, int tf, QTreeWidgetItem *symItem)
{
    if (!symItem) {
        m_loading = false;
        return;
    }

    QString dataDir = symItem->data(0, Qt::UserRole + 1).toString();
    QString apiType = symItem->data(0, Qt::UserRole + 2).toString();
    QString filenamePattern = symItem->data(0, Qt::UserRole + 6).toString();
    QString readerType = symItem->data(0, Qt::UserRole + 7).toString();

    DataProvider *prov = ProviderFactory::createProvider(
        apiType.isEmpty() ? QStringLiteral("file") : apiType,
        dataDir.isEmpty() ? QDir(QCoreApplication::applicationDirPath()).filePath("data") : dataDir,
        filenamePattern, readerType, nullptr);

    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("正在读取本地数据..."));

    // 后台线程读取
    QFutureWatcher<QVector<Candle>> *w = new QFutureWatcher<QVector<Candle>>(this);
    QObject::connect(w, &QFutureWatcher<QVector<Candle>>::finished, [this, w, prov, symbol, tf]() {
        QVector<Candle> localCandles = w->future().result();
        QTextEdit *logText = getLogWidget();

        if (!localCandles.isEmpty()) {
            // 限制最大 6000 条
            if (localCandles.size() > 6000) {
                localCandles = localCandles.mid(localCandles.size() - 6000);
            }

            // 转换为 KBar 并批量写入 KBarManager（一次锁）
            std::vector<KBar> bars;
            bars.reserve(localCandles.size());
            std::string symStd = symbol.toStdString();
            for (const auto &c : localCandles) {
                KBar kb;
                kb.symbol = symStd;
                kb.timeFrame = tf;
                kb.time = static_cast<uint64_t>(c.date.toSecsSinceEpoch());
                kb.open = c.open;
                kb.high = c.high;
                kb.low = c.low;
                kb.close = c.close;
                kb.volume = static_cast<uint64_t>(c.volume);
                bars.push_back(kb);
            }
            KBarManager::instance().add_kbars(symStd, tf, bars);

            if (logText) logText->append(QStringLiteral("本地数据加载完成，共 %1 条").arg(localCandles.size()));

            // 从 KBarManager 读取并显示
            loadFromManagerAndDisplay(symbol, tf);

            emit loadFinished(symbol, tf, true);
        } else {
            if (logText) logText->append(QStringLiteral("本地无数据"));
        }

        m_loading = false;
        prov->deleteLater();
        w->deleteLater();
    });

    QFuture<QVector<Candle>> f = QtConcurrent::run([prov, symbol, tf]() -> QVector<Candle> {
        QVector<Candle> out;
        if (!prov->loadLocalData(symbol, tf, out)) out.clear();
        return out;
    });
    w->setFuture(f);
}

// ============================================================
// 后台：通过 RPC 获取数据并写入 KBarManager
// ============================================================
void DataLoader::fetchRpcToManager(const QString &symbol, int tf)
{
    if (!m_impl || !m_impl->isRunning()) {
        QTextEdit *logText = getLogWidget();
        if (logText) logText->append(QStringLiteral("RPC 未连接，跳过远程数据获取"));
        return;
    }

    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("正在从服务器获取全量数据..."));

    // 后台线程执行 RPC 请求
    QFutureWatcher<std::vector<KBar>> *rpcWatcher = new QFutureWatcher<std::vector<KBar>>(this);
    QObject::connect(rpcWatcher, &QFutureWatcher<std::vector<KBar>>::finished, [this, rpcWatcher, symbol, tf]() {
        std::vector<KBar> rpcBars = rpcWatcher->future().result();
        QTextEdit *logText = getLogWidget();

        if (!rpcBars.empty()) {
            if (logText) logText->append(QStringLiteral("服务器数据加载完成，共 %1 条").arg(rpcBars.size()));

            // 写入 KBarManager（自动去重合并）
            KBarManager::instance().add_kbars(symbol.toStdString(), tf, rpcBars);

            // 限制最大 6000 条后刷新显示
            std::vector<KBar> allBars = KBarManager::instance().latest_kbars(
                symbol.toStdString(), tf,
                std::min<size_t>(KBarManager::instance().kbar_count(symbol.toStdString(), tf), 6000));

            if (!allBars.empty()) {
                QVector<Candle> candles = kbarVectorToCandles(allBars);
                if (m_k && symbol == m_symbol && tf == m_timeframe) {
                    m_k->setData(candles, tf);
                }
            }

            if (logText) logText->append(QStringLiteral("%1 %2min 加载完成").arg(symbol).arg(tf));
        } else {
            if (logText) logText->append(QStringLiteral("服务器返回空数据"));
        }

        rpcWatcher->deleteLater();
    });

    QFuture<std::vector<KBar>> rpcFuture = QtConcurrent::run([this, symbol, tf]() -> std::vector<KBar> {
        std::vector<KBar> out;
        if (m_impl) {
            m_impl->fetchBars(symbol.toStdString(), tf, out);
        }
        return out;
    });
    rpcWatcher->setFuture(rpcFuture);
}

bool DataLoader::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event);
}
