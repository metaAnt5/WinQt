#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QSet>
#include <QPair>

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
//
// 加载流程：
//   ① 检查 (品种,周期) 是否已初始化 → 是则直接从管理器显示
//   ② 加载本地 CSV → 写入 KBarManager → 结束 loading → 显示
//   ③ 取本地最后一条时间戳 → 增量 RPC 请求（startTime=最后时间）
//   ④ RPC 返回 → 合并到管理器 → 刷新显示
//   ⑤ 新数据追加写入本地 CSV → 标记已初始化
// ============================================================
class DataLoader : public QObject {
    Q_OBJECT
public:
    explicit DataLoader(KLineWidget *k, QObject *parent=nullptr);
    ~DataLoader();

    // 请求加载某个品种/周期的数据
    void requestLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem);

    // 启动/停止 RPC 客户端
    bool startRpcClient();
    void stopRpcClient();
    bool isRpcRunning() const;

    QString currentSymbol() const { return m_symbol; }
    int currentTimeframe() const { return m_timeframe; }
    KLineWidget *klineWidget() const { return m_k; }
    QTextEdit *getLogWidget() const;
    // Check if (symbol,tf) is ready to accept push data
    bool canAcceptPush(const QString &symbol, int timeframe) const {
        return m_initialized.contains(QPair<QString,int>(symbol, timeframe));
    }
    // Preload all symbols that have associated scripts
    void preloadAllScriptSymbols();


Q_SIGNALS:
    void loadStarted(const QString &symbol, int timeframe);
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

    // 已初始化的 (品种, 周期) 集合
    QSet<QPair<QString,int>> m_initialized;

    // 是否为全量加载（跳过本地 CSV，直接全量 RPC 拉取）
    bool m_isFullLoad = false;


    void loadFromManagerAndDisplay(const QString &symbol, int tf);
    void loadLocalToManager(const QString &symbol, int tf, QTreeWidgetItem *symItem);

    // 本地加载完成后的回调：结束 loading、显示、触发增量 RPC
    void onLocalLoadDone(const QString &symbol, int tf, QTreeWidgetItem *symItem);

    // 增量 RPC 请求（带 startTime 参数）
    void fetchRpcIncremental(const QString &symbol, int tf, uint64_t startTime);

    // RPC 增量加载完成后的回调：合并、刷新、回写、标记初始化
    void onRpcIncrementalDone(const QString &symbol, int tf, QTreeWidgetItem *symItem,
                              const std::vector<struct KBar> &rpcBars);

    // 本地加载完成后调用：结束 loading + 发射信号 + 处理空数据显示
    void finishLocalLoad(const QString &symbol, int tf);

    // 将新 K 线数据追加写入本地 CSV
    void appendToLocalFile(const QString &symbol, int tf,
                           QTreeWidgetItem *symItem,
                           const std::vector<struct KBar> &newBars);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};
