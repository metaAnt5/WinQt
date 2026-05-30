#include "dataloader.h"
#include "dataprovider.h"
#include "providerfactory.h"
#include "klinewidget.h"
#include "csvloader.h"

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
            if (m_destroying) return;
            QTextEdit *log = m_parent->getLogWidget();
            if (log) log->append(QString::fromStdString(msg));
        };

        // 连接状态回调
        m_rpc->on_connection_changed = [this](bool connected) {
            if (m_destroying) return;
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
            if (m_destroying) return;
            KBarManager::instance().add_kbar(kbar);

            emit m_parent->pushDataReady(
                QString::fromStdString(kbar.symbol),
                kbar.timeFrame,
                kbar.time,
                kbar.open, kbar.high, kbar.low, kbar.close,
                static_cast<double>(kbar.volume));
        };

        // 错误回调
        m_rpc->on_error = [this](const std::string &err) {
            if (m_destroying) return;
            QTextEdit *log = m_parent->getLogWidget();
            if (log) log->append(QStringLiteral("[TCP] error: %1").arg(QString::fromStdString(err)));
        };
    }

    ~Impl() {
        m_destroying = true;
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

    // 全量获取（用于首次加载时获取全部数据）
    bool fetchBars(const std::string &symbol, int timeFrame,
                   std::vector<KBar> &out)
    {
        if (!m_rpc || !m_rpc->is_running()) return false;
        return m_rpc->fetch_kbars(symbol, timeFrame, out);
    }

    // 增量获取（从 startTime 开始）
    bool fetchBarsSince(const std::string &symbol, int timeFrame,
                        uint64_t startTime, std::vector<KBar> &out)
    {
        if (!m_rpc || !m_rpc->is_running()) return false;
        return m_rpc->fetch_kbars_since(symbol, timeFrame, startTime, out);
    }

private:
    DataLoader *m_parent;
    std::shared_ptr<KBarRpcService> m_rpc;
    bool m_destroying = false;

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

    if (candles.size() > 6000) {
        candles = candles.mid(candles.size() - 6000);
    }

    if (m_k) {
        m_k->setData(candles, tf);
    }
}

// ============================================================
// 本地加载完成：结束 loading + 发射信号 + 空数据显示
// ============================================================
void DataLoader::finishLocalLoad(const QString &symbol, int tf)
{
    m_loading = false;

    if (m_symbol == symbol && m_timeframe == tf) {
        bool hasData = KBarManager::instance().kbar_count(symbol.toStdString(), tf) > 0;
        if (!hasData && m_k) {
            // 无数据：显示"暂无数据"覆盖层
            QVector<Candle> empty;
            m_k->setData(empty, tf);
        }
        emit loadFinished(symbol, tf, hasData);
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

    // ============================================================
    // 第 0 步：检查 (品种,周期) 是否已初始化过
    // ============================================================
    QPair<QString,int> key(symbol, timeframeMinutes);
    if (m_initialized.contains(key)) {
        if (logText) logText->append(QStringLiteral("%1 %2min 已初始化，直接显示").arg(symbol).arg(timeframeMinutes));
        loadFromManagerAndDisplay(symbol, timeframeMinutes);
        finishLocalLoad(symbol, timeframeMinutes);
        return;
    }

    // ============================================================
    // 第一步：本地加载（后台线程，完成后回调 onLocalLoadDone）
    // ============================================================
    loadLocalToManager(symbol, timeframeMinutes, symItem);
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
    QObject::connect(w, &QFutureWatcher<QVector<Candle>>::finished, [this, w, prov, symbol, tf, symItem]() {
        QVector<Candle> localCandles = w->future().result();
        QTextEdit *logText = getLogWidget();

        if (!localCandles.isEmpty()) {
            if (localCandles.size() > 6000) {
                localCandles = localCandles.mid(localCandles.size() - 6000);
            }

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
        } else {
            if (logText) logText->append(QStringLiteral("本地无数据"));
        }

        // 本地加载完成后的处理
        onLocalLoadDone(symbol, tf, symItem);

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
// 本地加载完成回调：显示、结束 loading、触发增量 RPC
// ============================================================
void DataLoader::onLocalLoadDone(const QString &symbol, int tf, QTreeWidgetItem *symItem)
{
    // 先显示现有数据（可能不全）
    loadFromManagerAndDisplay(symbol, tf);

    // 结束 loading 状态
    finishLocalLoad(symbol, tf);

    // 取本地最后一条 K 线的时间戳作为增量 RPC 的起点
    std::vector<KBar> allBars = KBarManager::instance().get_kbars(
        symbol.toStdString(), tf);
    uint64_t startTime = 0;
    if (!allBars.empty()) {
        startTime = allBars.back().time;
    }

    // 触发增量 RPC 请求（后台）
    fetchRpcIncremental(symbol, tf, startTime);
}

// ============================================================
// 增量 RPC 请求（后台线程）
// ============================================================
void DataLoader::fetchRpcIncremental(const QString &symbol, int tf, uint64_t startTime)
{
    if (!m_impl || !m_impl->isRunning()) {
        QTextEdit *logText = getLogWidget();
        if (logText) logText->append(QStringLiteral("RPC 未连接，跳过增量数据获取"));

        // RPC 不可用时也标记已初始化（只有本地数据）
        m_initialized.insert(QPair<QString,int>(symbol, tf));
        return;
    }

    QTextEdit *logText = getLogWidget();
    if (startTime > 0) {
        if (logText) logText->append(QStringLiteral("正在从服务器获取增量数据（起始时间: %1）...")
            .arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(startTime)).toString("yyyy.MM.dd HH:mm")));
    } else {
        if (logText) logText->append(QStringLiteral("正在从服务器获取全量数据..."));
    }

    // 后台线程执行增量 RPC 请求
    QFutureWatcher<std::vector<KBar>> *rpcWatcher = new QFutureWatcher<std::vector<KBar>>(this);
    QObject::connect(rpcWatcher, &QFutureWatcher<std::vector<KBar>>::finished, [this, rpcWatcher, symbol, tf]() {
        std::vector<KBar> rpcBars = rpcWatcher->future().result();
        onRpcIncrementalDone(symbol, tf, m_symItem, rpcBars);
        rpcWatcher->deleteLater();
    });

    QFuture<std::vector<KBar>> rpcFuture = QtConcurrent::run(
        [this, symbol, tf, startTime]() -> std::vector<KBar> {
        std::vector<KBar> out;
        if (m_impl) {
            if (startTime > 0) {
                m_impl->fetchBarsSince(symbol.toStdString(), tf, startTime, out);
            } else {
                m_impl->fetchBars(symbol.toStdString(), tf, out);
            }
        }
        return out;
    });
    rpcWatcher->setFuture(rpcFuture);
}

// ============================================================
// RPC 增量加载完成回调：合并、刷新、回写、标记初始化
// ============================================================
void DataLoader::onRpcIncrementalDone(const QString &symbol, int tf,
                                       QTreeWidgetItem *symItem,
                                       const std::vector<KBar> &rpcBars)
{
    QTextEdit *logText = getLogWidget();

    if (!rpcBars.empty()) {
        if (logText) logText->append(QStringLiteral("服务器增量数据加载完成，共 %1 条").arg(rpcBars.size()));

        // 合并到 KBarManager
        KBarManager::instance().add_kbars(symbol.toStdString(), tf, rpcBars);

        // 刷新显示
        std::vector<KBar> allBars = KBarManager::instance().latest_kbars(
            symbol.toStdString(), tf,
            std::min<size_t>(KBarManager::instance().kbar_count(symbol.toStdString(), tf), 6000));

        if (!allBars.empty()) {
            QVector<Candle> candles = kbarVectorToCandles(allBars);
            if (m_k && symbol == m_symbol && tf == m_timeframe) {
                m_k->setData(candles, tf);
            }
        }

        // 追加写入本地 CSV
        appendToLocalFile(symbol, tf, symItem, rpcBars);

        if (logText) logText->append(QStringLiteral("%1 %2min 加载完成").arg(symbol).arg(tf));
    } else {
        if (logText) logText->append(QStringLiteral("服务器返回空数据"));
    }

    // 标记已初始化
    m_initialized.insert(QPair<QString,int>(symbol, tf));
}

// ============================================================
// 将新 K 线数据追加写入本地 CSV
// ============================================================
void DataLoader::appendToLocalFile(const QString &symbol, int tf,
                                    QTreeWidgetItem *symItem,
                                    const std::vector<KBar> &newBars)
{
    if (!symItem || newBars.empty()) return;

    // 构造 CSV 文件路径（与读取时保持一致）
    QString dataDir = symItem->data(0, Qt::UserRole + 1).toString();
    QString filenamePattern = symItem->data(0, Qt::UserRole + 6).toString();

    if (dataDir.isEmpty()) {
        dataDir = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    }

    // 文件名：symbol+timeframeMinutes.csv，例如 XAUUSD5.csv
    QString filename = symbol + QString::number(tf) + ".csv";
    QString filePath = QDir(dataDir).filePath(filename);

    // 如果有 filenamePattern，用它来生成文件名
    if (!filenamePattern.isEmpty()) {
        filePath = QDir(dataDir).filePath(
            filenamePattern
                .replace("{Symbol}", symbol)
                .replace("{Timeframe}", QString::number(tf)));
    }

    // 转换 KBar -> Candle
    QVector<Candle> candles;
    candles.reserve(static_cast<int>(newBars.size()));
    for (const auto &kb : newBars) {
        candles.append(kbarToCandle(kb));
    }

    // 调用 CSV 追加函数
    if (appendCsvFile(filePath, candles)) {
        QTextEdit *log = getLogWidget();
        if (log) log->append(QStringLiteral("已将 %1 条新数据追加到 %2").arg(candles.size()).arg(filePath));
    }
}

bool DataLoader::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event);
}
