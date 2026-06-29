#pragma once

#include <QVector>
#include <QString>
#include <QHash>
#include <QMutex>
#include <memory>

struct Candle;

// ============================================================
// IndicatorCache - 单个 (品种,周期) 的指标缓存
// ============================================================
struct IndicatorCache {
    // Moving Averages
    QVector<double> ma5;
    QVector<double> ma10;
    QVector<double> ma20;
    QVector<double> ma60;

    // RSI (14 period)
    QVector<double> rsi14;

    // KDJ (9,3,3)
    QVector<double> kdjK;
    QVector<double> kdjD;
    QVector<double> kdjJ;

    // MACD (12,26,9)
    QVector<double> macd;
    QVector<double> macdSignal;
    QVector<double> macdHist;

    // Bollinger Bands (20,2)
    QVector<double> bollMid;
    QVector<double> bollUpper;
    QVector<double> bollLower;

    bool isValid() const { return !ma5.isEmpty(); }
    void clear() {
        ma5.clear(); ma10.clear(); ma20.clear(); ma60.clear();
        rsi14.clear();
        kdjK.clear(); kdjD.clear(); kdjJ.clear();
        macd.clear(); macdSignal.clear(); macdHist.clear();
        bollMid.clear(); bollUpper.clear(); bollLower.clear();
    }
};

// ============================================================
// IndicatorCalculator - 全局指标计算器
// 与 KBarManager 平级，独立维护所有 (品种,周期) 的指标缓存
// ============================================================
class IndicatorCalculator {
public:
    static IndicatorCalculator& instance();

    // 更新某个品种/周期的全部 K 线数据，重新计算所有指标
    void updateIndicators(const QString &symbol, int timeframe,
                          const QVector<Candle> &candles);

    // 追加单根 K 线并增量更新指标
    void appendCandle(const QString &symbol, int timeframe,
                      const Candle &candle);

    // 替换最后一根 K 线并增量更新指标
    void updateLastCandle(const QString &symbol, int timeframe,
                          const Candle &candle);

    // 获取指标缓存（线程安全）
    IndicatorCache getCache(const QString &symbol, int timeframe) const;

    // 获取单个指标值
    double getMA(const QString &symbol, int timeframe, int period, int index) const;
    double getRSI(const QString &symbol, int timeframe, int period, int index) const;
    // 获取 KDJ 值（通过输出参数返回 K、D、J 向量）
    void getKDJ(const QString &symbol, int timeframe,
                QVector<double> &outK, QVector<double> &outD, QVector<double> &outJ) const;

    // 获取 MACD 值（通过输出参数返回 dif, dea, macd 向量）
    void getMACD(const QString &symbol, int timeframe,
                 QVector<double> &outDif, QVector<double> &outDea, QVector<double> &outMacd) const;

    // 清除缓存
    void clearCache(const QString &symbol, int timeframe);
    void clearAll();

private:
    IndicatorCalculator() = default;
    ~IndicatorCalculator() = default;
    IndicatorCalculator(const IndicatorCalculator&) = delete;
    IndicatorCalculator& operator=(const IndicatorCalculator&) = delete;

    // 内部计算函数
    static void calcMA(const QVector<Candle> &candles, int period, QVector<double> &out);
    static void calcRSI(const QVector<Candle> &candles, int period, QVector<double> &out);
    static void calcKDJ(const QVector<Candle> &candles, int period, int kFactor, int dFactor,
                        QVector<double> &outK, QVector<double> &outD, QVector<double> &outJ);
    static void calcMACD(const QVector<Candle> &candles,
                         QVector<double> &outMacd,
                         QVector<double> &outSignal,
                         QVector<double> &outHist);
    static void calcBollinger(const QVector<Candle> &candles, int period, double multiplier,
                              QVector<double> &outMid,
                              QVector<double> &outUpper,
                              QVector<double> &outLower);

    // 缓存 key = symbol + "@" + timeframe
    struct Key {
        QString symbol;
        int timeframe;
        bool operator==(const Key &o) const {
            return symbol == o.symbol && timeframe == o.timeframe;
        }
    };
    struct KeyHash {
        size_t operator()(const Key &k) const {
            return qHash(k.symbol) ^ qHash(k.timeframe);
        }
    };

    mutable QMutex m_mutex;
    QHash<QString, IndicatorCache> m_caches; // key = "symbol@tf"
};
