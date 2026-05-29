#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

struct Candle;
class KLineWidget;
class QTreeWidgetItem;
class QTextEdit;

// ============================================================
// DataLoader - 数据加载器
//
// 数据流：
//   所有数据统一通过 KBarManager（全局单例）管理
//   本地 CSV 和 RPC 数据都先写入 KBarManager，再从管理器读取显示
//   推送数据也写入 KBarManager，如果正在显示则更新视图
// ============================================================
class DataLoader : public QObject {
    Q_OBJECT
public:
    explicit DataLoader(KLineWidget *k, QObject *parent=nullptr);
    ~DataLoader();

    // 请求加载某个品种/周期的数据
    // 流程：查 KBarManager 缓存 -> 有则直接显示 -> 无则本地+RPC 加载
    void requestLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem);

    // 启动/停止 RPC 客户端
    bool startRpcClient();
    void stopRpcClient();
    bool isRpcRunning() const;

    // 获取当前品种和周期
    QString currentSymbol() const { return m_symbol; }
    int currentTimeframe() const { return m_timeframe; }

    // 获取 KLineWidget
    KLineWidget *klineWidget() const { return m_k; }

    // 获取日志控件
    QTextEdit *getLogWidget() const;

Q_SIGNALS:
    void loadStarted(const QString &symbol, int timeframe);
    void loadFinished(const QString &symbol, int timeframe, bool success);
    void loadFailed(const QString &symbol, int timeframe, const QString &reason);

    // 推送数据到达（跨线程安全，给 UI 更新实时 K 线用）
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

    // 从 KBarManager 加载并显示
    void loadFromManagerAndDisplay(const QString &symbol, int tf);

    // 后台线程：读取本地 CSV 并写入 KBarManager
    void loadLocalToManager(const QString &symbol, int tf, QTreeWidgetItem *symItem);

    // 后台线程：通过 RPC 获取数据并写入 KBarManager
    void fetchRpcToManager(const QString &symbol, int tf);

    // RPC 客户端（前向声明）
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
