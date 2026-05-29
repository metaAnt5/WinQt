#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

struct Candle;
class KLineWidget;
class QTreeWidgetItem;
class QTextEdit;

// ============================================================
// DataLoader - 数据加载器
//
// 职责：
//  - 所有数据读写都通过 DataCache 全局缓存
//  - 用户请求品种/周期时，先查缓存，有则直接显示
//  - 无缓存数据时，读本地 CSV 写入缓存，同时发起 RPC 请求
//  - 推送数据写入缓存，如果正在显示则更新视图
//  - 启动时自动批量加载所有品种的本地 CSV
// ============================================================
class DataLoader : public QObject {
    Q_OBJECT
public:
    explicit DataLoader(KLineWidget *k, QObject *parent=nullptr);
    ~DataLoader();

    // 请求加载某个品种/周期的数据
    // 流程：查缓存 -> 有则直接显示 -> 无则本地+RPC 加载
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

    // 地址时批量加载所有品种的可用本地数据
    void startBatchLoad(const QStringList &symbols, const QVector<int> &timeframes);

    // 获取日志控件
    QTextEdit *getLogWidget() const;

Q_SIGNALS:
    // 加载状态信号
    void loadStarted(const QString &symbol, int timeframe);
    void loadFinished(const QString &symbol, int timeframe, bool success);
    void loadFailed(const QString &symbol, int timeframe, const QString &reason);

    // 视图更新信号：数据就绪，通知显示
    void dataReady(const QString &symbol, int timeframe, const QVector<Candle> &candles);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<KLineWidget> m_k;
    QString m_symbol;
    int m_timeframe = 1;
    QTreeWidgetItem *m_symItem = nullptr;
    bool m_loading = false;

    // 从缓存加载并显示
    void loadFromCacheAndDisplay(const QString &symbol, int tf);

    // 后台线程：读取本地 CSV 并写入缓存
    void loadLocalToCache(const QString &symbol, int tf, QTreeWidgetItem *symItem);

    // 后台线程：通过 RPC 获取数据并写入缓存
    void fetchRpcToCache(const QString &symbol, int tf);

    // RPC 客户端（前向声明）
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
