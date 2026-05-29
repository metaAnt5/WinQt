#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

struct Candle;
class KLineWidget;
class QTreeWidgetItem;
class QTextEdit;


class DataLoader : public QObject {
    Q_OBJECT
public:
    explicit DataLoader(KLineWidget *k, QObject *parent=nullptr);
    ~DataLoader();

    void requestInitialLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem);

    // 启动/停止 RPC 客户端
    bool startRpcClient();
    void stopRpcClient();
    bool isRpcRunning() const;

    // 获取日志控件（供内部跨线程使用）
    QTextEdit *getLogWidget() const;

    // 获取当前品种和周期
    QString currentSymbol() const { return m_symbol; }
    int currentTimeframe() const { return m_timeframe; }

    // 获取 KLineWidget
    KLineWidget *klineWidget() const { return m_k; }

Q_SIGNALS:
    // 加载状态信号
    void loadStarted(const QString &symbol, int timeframe);
    void localDataLoaded(const QString &symbol, int timeframe, int count);
    void rpcDataLoaded(const QString &symbol, int timeframe, int count);
    void loadFinished(const QString &symbol, int timeframe, bool success);
    void loadFailed(const QString &symbol, int timeframe, const QString &reason);

    // 推送数据到达（跨线程安全）
    void pushDataReady(const QString &symbol, int timeframe, uint64_t time,
                       double open, double high, double low, double close, double volume);

    // RPC 连接状态变化
    void connectionStatusChanged(bool connected);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<KLineWidget> m_k;
    QString m_symbol;
    int m_timeframe = 1;
    QTreeWidgetItem *m_symItem = nullptr;
    bool m_loading = false;

    // RPC 客户端（前向声明）
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
