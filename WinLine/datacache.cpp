#include "datacache.h"
#include "klinewidget.h" // for Candle struct

#include <QMutexLocker>
#include <algorithm>

// ============================================================
// 单例
// ============================================================
DataCache* DataCache::instance()
{
    static DataCache s_instance;
    return &s_instance;
}

DataCache::DataCache(QObject *parent)
    : QObject(parent)
{
}

// ============================================================
// 工具函数
// ============================================================
QString DataCache::makeKey(const QString &symbol, int tf)
{
    return symbol + "|" + QString::number(tf);
}

QString DataCache::extractSymbol(const QString &key)
{
    int pos = key.indexOf('|');
    return pos < 0 ? key : key.left(pos);
}

int DataCache::extractTf(const QString &key)
{
    int pos = key.indexOf('|');
    return pos < 0 ? 0 : key.mid(pos + 1).toInt();
}

// ============================================================
// 内部插入（已加写锁时调用）
// 返回插入位置的索引，以及是否更新了已有数据
// ============================================================
static int insertOneIntoSorted(QVector<Candle> &vec, const Candle &c, bool &wasUpdate)
{
    // 二分查找时间位置，vec 始终按 date 升序排列
    auto it = std::lower_bound(vec.begin(), vec.end(), c.date,
        [](const Candle &a, const QDateTime &t) { return a.date < t; });

    int idx = static_cast<int>(std::distance(vec.begin(), it));

    if (it != vec.end() && it->date == c.date) {
        // 同时间 -> 更新（Bar Update）
        *it = c;
        wasUpdate = true;
        return idx;
    } else {
        // 新时间 -> 插入（New Bar）
        vec.insert(it, c);
        wasUpdate = false;
        return idx;
    }
}

// ============================================================
// 数据写入
// ============================================================
int DataCache::insertCandle(const QString &symbol, int tf, const Candle &c)
{
    QString key = makeKey(symbol, tf);
    int idx = -1;
    bool wasUpdate = false;

    {
        QWriteLocker lock(&m_rwLock);
        idx = insertOneIntoSorted(m_cache[key], c, wasUpdate);
    }

    // 在锁外发送信号，防止死锁
    if (wasUpdate)
        emit candleUpdated(symbol, tf, idx);
    else
        emit candleInserted(symbol, tf, idx);

    return idx;
}

void DataCache::insertCandles(const QString &symbol, int tf, const QVector<Candle> &candles)
{
    if (candles.isEmpty()) return;

    int addedCount = 0;

    {
        QWriteLocker lock(&m_rwLock);
        QString key = makeKey(symbol, tf);
        auto &vec = m_cache[key];
        int oldSize = vec.size();

        for (const auto &c : candles) {
            bool dummy;
            insertOneIntoSorted(vec, c, dummy);
        }

        addedCount = vec.size() - oldSize;
    }

    emit dataBatchLoaded(symbol, tf, addedCount);
}

// ============================================================
// 数据查询（返回副本，线程安全）
// ============================================================
QVector<Candle> DataCache::getCandles(const QString &symbol, int tf) const
{
    QReadLocker lock(&m_rwLock);
    QString key = makeKey(symbol, tf);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it.value();
    }
    return {};
}

int DataCache::candleCount(const QString &symbol, int tf) const
{
    QReadLocker lock(&m_rwLock);
    QString key = makeKey(symbol, tf);
    auto it = m_cache.find(key);
    return it != m_cache.end() ? it.value().size() : 0;
}

bool DataCache::hasData(const QString &symbol, int tf) const
{
    return candleCount(symbol, tf) > 0;
}

// ============================================================
// 清除
// ============================================================
void DataCache::clearCache(const QString &symbol, int tf)
{
    {
        QWriteLocker lock(&m_rwLock);
        QString key = makeKey(symbol, tf);
        m_cache.remove(key);
    }
    emit cacheCleared(symbol, tf);
}

void DataCache::clearAll()
{
    {
        QWriteLocker lock(&m_rwLock);
        m_cache.clear();
    }
}

// ============================================================
// 查询
// ============================================================
QStringList DataCache::allKeys() const
{
    QReadLocker lock(&m_rwLock);
    return m_cache.keys();
}

int DataCache::totalCacheEntries() const
{
    QReadLocker lock(&m_rwLock);
    return m_cache.size();
}

int DataCache::totalCandles() const
{
    QReadLocker lock(&m_rwLock);
    int total = 0;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        total += it.value().size();
    }
    return total;
}
