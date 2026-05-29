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
        // 加载服务器配置
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
            emit m_parent->connectionStatusChanged(connected);
        };

        // 推送回调：收到新 K 线数据时通过信号通知主线程
        m_rpc->on_kbar_pushed = [this](const KBar &kbar) {
            // 通过信号安全跨线程通知
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

    // 尝试通过 RPC 获取数据（同步调用，应在后台线程执行）
    bool fetchBars(const std::string &symbol, int timeFrame,
                   std::vector<KBar> &out)
    {
        if (!m_rpc || !m_rpc->is_running()) return false;

        bool ok = m_rpc->fetch_kbars(symbol, timeFrame, out);
        return ok;
    }

private:
    DataLoader *m_parent;
    std::shared_ptr<KBarRpcService> m_rpc;

    // 加载 server.json 配置文件
    static KBarRpcService::Config loadConfig() {
        KBarRpcService::Config cfg;
        QFile f(QCoreApplication::applicationDirPath() + "/config/server.json");
        // 也试试相对路径
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
// 核心加载流程：两阶段加载
// ============================================================
void DataLoader::requestInitialLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem)
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

    // 发射加载开始信号
    emit loadStarted(symbol, timeframeMinutes);

    QTextEdit *logText = getLogWidget();
    if (logText) logText->append(QStringLiteral("正在加载 %1 %2min...").arg(symbol).arg(timeframeMinutes));

    // --------------------------------------------------------
    // 第一阶段：读取本地数据
    // --------------------------------------------------------
    QString dataDir = symItem->data(0, Qt::UserRole + 1).toString();
    QString apiType = symItem->data(0, Qt::UserRole + 2).toString();
    QString filenamePattern = symItem->data(0, Qt::UserRole + 6).toString();
    QString readerType = symItem->data(0, Qt::UserRole + 7).toString();

    DataProvider *prov = ProviderFactory::createProvider(
        apiType.isEmpty() ? QStringLiteral("file") : apiType,
        dataDir.isEmpty() ? QDir(QCoreApplication::applicationDirPath()).filePath("data") : dataDir,
        filenamePattern, readerType, nullptr);

    if (logText) logText->append(QStringLiteral("正在读取本地数据..."));

    QFutureWatcher<QVector<Candle>> *w = new QFutureWatcher<QVector<Candle>>(this);
    QObject::connect(w, &QFutureWatcher<QVector<Candle>>::finished, [this, w, prov, symbol, timeframeMinutes]() {
        QVector<Candle> localData = w->future().result();
        QTextEdit *logText = getLogWidget();

        bool hasLocal = !localData.isEmpty();

        if (hasLocal) {
            // 本地数据加载成功，立即显示
            if (localData.size() > 6000) {
                localData = localData.mid(localData.size() - 6000);
            }
            if (m_k) {
                m_k->setData(localData, m_timeframe);
            }

            // 获取 CSV 路径用于日志
            QString csvPath = m_symItem ? m_symItem->data(0, Qt::UserRole + 3).toString() : QString();
            if (csvPath.isEmpty() && m_symItem) {
                QString dDir = m_symItem->data(0, Qt::UserRole + 1).toString();
                QString fnPattern = m_symItem->data(0, Qt::UserRole + 6).toString();
                QString baseDir = dDir.isEmpty() ? QDir(QCoreApplication::applicationDirPath()).filePath("data") : dDir;
                if (!fnPattern.isEmpty()) {
                    QString p = fnPattern; p.replace("%{symbol}", m_symbol); p.replace("%{tf}", QString::number(m_timeframe));
                    csvPath = QDir(baseDir).filePath(p);
                } else csvPath = QDir(baseDir).filePath(m_symbol + ".csv");
            }

            if (logText) logText->append(QStringLiteral("本地数据加载完成，共 %1 条 (%2)").arg(localData.size()).arg(csvPath));

            // 发射信号
            emit localDataLoaded(m_symbol, m_timeframe, localData.size());
        } else {
            if (logText) logText->append(QStringLiteral("本地无数据"));
        }

        // --------------------------------------------------------
        // 第二阶段：通过 RPC 获取全量数据（后台线程）
        // --------------------------------------------------------
        bool rpcAvailable = m_impl && m_impl->isRunning();

        if (rpcAvailable) {
            if (logText) logText->append(QStringLiteral("正在从服务器获取全量数据..."));

            // 在后台线程执行 RPC 请求
            QFutureWatcher<std::vector<KBar>> *rpcWatcher = new QFutureWatcher<std::vector<KBar>>(this);
            QObject::connect(rpcWatcher, &QFutureWatcher<std::vector<KBar>>::finished, [this, rpcWatcher, hasLocal, localData, symbol, timeframeMinutes]() {
                std::vector<KBar> rpcBars = rpcWatcher->future().result();
                QTextEdit *logText = getLogWidget();

                if (!rpcBars.empty()) {
                    // 将 KBar 转换为 Candle
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
                    emit rpcDataLoaded(m_symbol, m_timeframe, rpcCandles.size());

                    // 合并本地和 RPC 数据（按时间戳去重）
                    QVector<Candle> merged;
                    if (hasLocal) {
                        // 合并去重
                        merged.reserve(localData.size() + rpcCandles.size());
                        int li = 0, ri = 0;
                        while (li < localData.size() && ri < rpcCandles.size()) {
                            if (localData[li].date < rpcCandles[ri].date) {
                                merged.append(localData[li++]);
                            } else if (localData[li].date > rpcCandles[ri].date) {
                                merged.append(rpcCandles[ri++]);
                            } else {
                                // 时间相同，用 RPC 数据（更新更准确）
                                merged.append(rpcCandles[ri++]);
                                li++;
                            }
                        }
                        while (li < localData.size()) merged.append(localData[li++]);
                        while (ri < rpcCandles.size()) merged.append(rpcCandles[ri++]);

                        if (logText) logText->append(QStringLiteral("合并后共 %1 条 K 线").arg(merged.size()));
                    } else {
                        // 只有 RPC 数据
                        merged = rpcCandles;
                    }

                    // 显示合并后的数据
                    if (m_k) {
                        if (merged.size() > 6000) {
                            merged = merged.mid(merged.size() - 6000);
                        }
                        m_k->setData(merged, m_timeframe);
                    }

                    if (logText) logText->append(QStringLiteral("%1 %2min 加载完成").arg(m_symbol).arg(m_timeframe));
                    emit loadFinished(m_symbol, m_timeframe, true);
                } else {
                    // RPC 返回空数据（连接正常但服务器无数据）
                    if (logText) logText->append(QStringLiteral("服务器无数据"));

                    if (!hasLocal) {
                        QString reason = QStringLiteral("RPC 返回数据为空");
                        if (logText) logText->append(QStringLiteral("加载 %1 %2min 失败: 本地无数据且 %3")
                            .arg(m_symbol).arg(m_timeframe).arg(reason));
                        emit loadFailed(m_symbol, m_timeframe, reason);
                    } else {
                        emit loadFinished(m_symbol, m_timeframe, true);
                    }
                }


                // 清理
                m_loading = false;
                rpcWatcher->deleteLater();
            });

            // 启动后台 RPC 请求
            QFuture<std::vector<KBar>> rpcFuture = QtConcurrent::run([this, symbol, timeframeMinutes]() -> std::vector<KBar> {
                std::vector<KBar> out;
                // 从 Impl 获取 RPC 数据
                if (m_impl) {
                    // 日志输出（不能在后台线程直接操作 GUI，但 logText 追加可以）
                    m_impl->fetchBars(symbol.toStdString(), timeframeMinutes, out);
                }
                return out;
            });
            rpcWatcher->setFuture(rpcFuture);
        } else {
            // RPC 不可用
            if (logText) {
                if (!hasLocal) {
                    logText->append(QStringLiteral("加载 %1 %2min 失败: 本地无数据且 RPC 未连接")
                        .arg(m_symbol).arg(m_timeframe));
                } else {
                    logText->append(QStringLiteral("%1 %2min 加载完成 (仅本地数据)").arg(m_symbol).arg(m_timeframe));
                }
            }

            if (!hasLocal) {
                emit loadFailed(m_symbol, m_timeframe, QStringLiteral("RPC 未连接"));
            } else {
                emit loadFinished(m_symbol, m_timeframe, true);
            }

            // 清理
            m_loading = false;
        }

        prov->deleteLater();
        w->deleteLater();
    });

    // 启动后台本地数据读取
    QFuture<QVector<Candle>> f = QtConcurrent::run([prov, symbol, timeframeMinutes]() -> QVector<Candle> {
        QVector<Candle> out;
        if (!prov->loadLocalData(symbol, timeframeMinutes, out)) out.clear();
        return out;
    });
    w->setFuture(f);
}

bool DataLoader::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event);
}
