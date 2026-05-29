#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QReadWriteLock>
#include <QThread>
#include <algorithm>

struct Candle;

// ============================================================
// DataCache - 全局数据缓存（线程安全单例）
// 所有 K 线数据统一经过此缓存，保证时序性和数据一致性
// ============================================================
class DataCache : public QObject
{
    Q_OBJECT
public:
    static DataCache* instance();

    // ---- 数据查询 ----
    QVector<Candle> getCandles(const QString &symbol, int tf) const;
    int candleCount(const QString &symbol, int tf) const;
    bool hasData(const QString &symbol, int tf) const;

    // ---- 数据写入（带时序保证） ----
    // 插入单根 Candle，自动按时间排序/去重
    // 返回插入位置的索引
    int insertCandle(const QString &symbol, int tf, const Candle &c);

    // 批量插入 Candle vector（已按时间升序排列）
    // 用于 CSV 加载或 RPC 全量获取
    void insertCandles(const QString &symbol, int tf, const QVector<Candle> &candles);

    // 清除指定品种/周期的缓存
    void clearCache(const QString &symbol, int tf);
    void clearAll();

    // ---- 获取所有缓存的 key ----
    QStringList allKeys() const;

    // ---- 缓存统计 ----
    int totalCacheEntries() const;
    int totalCandles() const;

signals:
    // 数据变更通知
    void candleInserted(const QString &symbol, int tf, int index);
    void candleUpdated(const QString &symbol, int tf, int index);
    void dataBatchLoaded(const QString &symbol, int tf, int count);
    void cacheCleared(const QString &symbol, int tf);

private:
    DataCache(QObject *parent = nullptr);
    ~DataCache() = default;
    DataCache(const DataCache&) = delete;
    DataCache& operator=(const DataCache&) = delete;

    static QString makeKey(const QString &symbol, int tf);
    static QString extractSymbol(const QString &key);
    static int extractTf(const QString &key);

    // 内部插入（已加锁），返回索引
    int insertOneLocked(const QString &key, const Candle &c);

    QMap<QString, QVector<Candle>> m_cache;
    mutable QReadWriteLock m_rwLock;
};
