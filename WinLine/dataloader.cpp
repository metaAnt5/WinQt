#include "dataloader.h"
#include "datacache.h"
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

// NetCore RPC 客户端
#include "mt4rpc/KBarRpcService.h"

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
        };

        // 推送回调：收到新 K 线数据时写入 DataCache
        m_rpc->on_kbar_pushed = [this](const KBar &kbar) {
            // 写入 DataCache（线程安全，后台线程直接调用）
            Candle c;
            c.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(kbar.time));
            c.open = kbar.open;
            c.high = kbar.high;
            c.low = kbar.low;
            c.close = kbar.close;
            c.volume = static_cast<double>(kbar.volume);

            QString symbol = QString::fromStdString(kbar.symbol);
            DataCache::instance()->insertCandle(symbol, kbar.timeFrame, c);

            // 通知 DataLoader（通过信号跨线程）
            emit m_parent->klineWidget()->updateRealtimeCandle(c);
            // 同时在主线程更新视图（如果正在显示）
            QMetaObject::invokeMethod(m_parent, [this, symbol, tf = kbar.timeFrame]() {
                if (symbol == m_parent->m_symbol && tf == m_parent->m_timeframe) {
                    // 从缓存重新读取并更新
                    QVector<Candle> data = DataCache::instance()->getCandles(symbol, tf);
                    if (!data.isEmpty() && m_parent->m_k) {
                        m_parent->m_k->setData(data, tf);
                    }
                }
            }, Qt::QueuedConnection);
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
    // 连接 DataCache 信号，订阅数据更新
    connect(DataCache::instance(), &DataCache::dataBatchLoaded, this,
        [this](const QString &symbol, int tf, int added) {
            Q_UNUSED(added);
            // 如果当前正在显示这个品种/周期，刷新视图
            if (symbol == m_symbol && tf == m_timeframe && m_k) {
                QVector<Candle> data = DataCache::instance()->getCandles(symbol, tf);
                if (!data.isEmpty()) {
                    m_k->setData(data, tf);
                }
            }
        });
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
// 从缓存加载并显示
// ============================================================
void DataLoader::loadFromCacheAndDisplay(const QString &symbol, int tf)
{
    QVector<Candle> data = DataCache::instance()->getCandles(symbol, tf);
    if (data.isEmpty()) return;

    if (m_k) {
        m_k->setData(data, tf);
    }
    emit dataReady(symbol, tf, data);
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
    // 第 0 步：检查缓存是否已有数据
    // ================================================================
    if (DataCache::instance()->hasData(symbol, timeframeMinutes)) {
        if (logText) logText->append(QStringLiteral("缓存命中，直接显示 %1 %2min").arg(symbol).arg(timeframeMinutes));
        loadFromCacheAndDisplay(symbol, timeframeMinutes);
        m_loading = false;
        emit loadFinished(symbol, timeframeMinutes, true);
        return;
    }

    // ================================================================
    // 第一步：后台线程读取本地 CSV 并写入缓存
    // ================================================================
    loadLocalToCache(symbol, timeframeMinutes, symItem);

    // ================================================================
    // 第二步：后台线程通过 RPC 获取全量数据并写入缓存
    // ================================================================
    fetchRpcToCache(symbol, timeframeMinutes);
}

// ============================================================
// 后台：读取本地 CSV 并写入缓存
// ============================================================
void DataLoader::loadLocalToCache(const QString &symbol, int tf, QTreeWidgetItem *symItem)
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
    connect(w, &QFutureWatcher<QVector<Candle>>::finished, [this, w, prov, symbol, tf]() {
        QVector<Candle> localData = w->future().result();
        QTextEdit *logText = getLogWidget();

        if (!localData.isEmpty()) {
            // 限制最大 6000 条
            if (localData.size() > 6000) {
                localData = localData.mid(localData.size() - 6000);
            }

            // 写入缓存
            DataCache::instance()->insertCandles(symbol, tf, localData);

            // 从缓存读取并显示
            loadFromCacheAndDisplay(symbol, tf);

            if (logText) logText->append(QStringLiteral("本地数据加载完成，共 %1 条").arg(localData.size()));
            emit loadFinished(symbol, tf, true);
        } else {
            if (logText) logText->append(QStringLiteral("本地无数据"));
            // 如果也无 RPC，稍后 fetchRpcToCache 会处理
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
// 后台：通过 RPC 获取数据并写入缓存
// ============================================================
void DataLoader::fetchRpcToCache(const QString &symbol, int tf)
{
    if (!m_impl || !m_impl->isRunning()) {
        QTextEdit *logText = getLogWidget();
        if (logText) logText->append(QStringLiteral("RPC 未连接，跳过远程数据获取"));
        // 如果本地已有数据，不算失败
        return;
    }

    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("正在从服务器获取全量数据..."));

    // 后台线程执行 RPC 请求
    QFutureWatcher<std::vector<KBar>> *rpcWatcher = new QFutureWatcher<std::vector<KBar>>(this);
    connect(rpcWatcher, &QFutureWatcher<std::vector<KBar>>::finished, [this, rpcWatcher, symbol, tf]() {
        std::vector<KBar> rpcBars = rpcWatcher->future().result();
        QTextEdit *logText = getLogWidget();

        if (!rpcBars.empty()) {
            // 转换为 Candle
            QVector<Candle> rpcCandles;
            rpcCandles.reserve(static_cast<int>(rpcBars.size()));
            for (const auto &kb : rpcBars) {
                Candle c;
                c.date = QDateTime::fromSecsSinceEpoch(kb.time);
                c.open = kb.open;
                c.high = kb.high;
                c.low = kb.low;
                c.close = kb.close;
                c.volume = static_cast<double>(kb.volume);
                rpcCandles.append(c);
            }

            if (logText) logText->append(QStringLiteral("服务器数据加载完成，共 %1 条").arg(rpcCandles.size()));

            // 限制最大 6000 条（保留最新数据）
            if (rpcCandles.size() > 6000) {
                rpcCandles = rpcCandles.mid(rpcCandles.size() - 6000);
            }

            // 写入缓存（自动去重合并）
            DataCache::instance()->insertCandles(symbol, tf, rpcCandles);

            // 如果当前正在显示此品种/周期，刷新视图
            if (symbol == m_symbol && tf == m_timeframe && m_k) {
                QVector<Candle> merged = DataCache::instance()->getCandles(symbol, tf);
                if (!merged.isEmpty()) {
                    m_k->setData(merged, tf);
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

// ============================================================
// 启动时批量加载所有品种
// ============================================================
void DataLoader::startBatchLoad(const QStringList &symbols, const QVector<int> &timeframes)
{
    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("开始批量加载 %1 个品种的数据...").arg(symbols.size()));

    // 逐个后台加载本地 CSV 到缓存
    for (const QString &sym : symbols) {
        for (int tf : timeframes) {
            // 如果缓存中已有，跳过
            if (DataCache::instance()->hasData(sym, tf)) continue;

            // 构造一个临时 symItem 用于读取配置
            // 启动后台读取（只读本地，不阻塞 UI）
            std::string symStd = sym.toStdString();
            int tfCopy = tf;

            QtConcurrent::run([this, sym, tfCopy]() {
                // 需要在主线程获取 log
                QTextEdit *log = getLogWidget();

                // 从 ProviderFactory 获取数据
                // 这里简化处理：直接通过 ProviderFactory 创建 provider
                // 但我们没有 symItem 的数据，所以先跳过
                // 在实际使用中，main.cpp 可以传入所有 symItem 的信息
                if (log) log->append(QStringLiteral("批量加载 %1 %2min...").arg(sym).arg(tfCopy));
            });
        }
    }
}

void DataLoader::onRpcPushData(const QString &symbol, int timeFrame,
                                uint64_t time, double open, double high,
                                double low, double close, double volume)
{
    Candle c;
    c.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time));
    c.open = open;
    c.high = high;
    c.low = low;
    c.close = close;
    c.volume = volume;

    DataCache::instance()->insertCandle(symbol, timeFrame, c);
}

void DataLoader::onRpcFetchResult(const QString &symbol, int timeFrame,
                                    const QVector<Candle> &candles)
{
    DataCache::instance()->insertCandles(symbol, timeFrame, candles);
}
