#include "indicatorcalc.h"
#include "klinewidget.h"
#include <QtMath>

IndicatorCalculator& IndicatorCalculator::instance() { static IndicatorCalculator inst; return inst; }

void IndicatorCalculator::calcMA(const QVector<Candle> &candles, int period, QVector<double> &out)
{
    out.clear(); if (candles.isEmpty()) return;
    out.resize(candles.size(), 0.0); double sum = 0;
    for (int i = 0; i < candles.size(); ++i) {
        sum += candles[i].close;
        if (i < period) out[i] = sum / (i + 1);
        else { sum -= candles[i - period].close; out[i] = sum / period; }
    }
}

void IndicatorCalculator::calcRSI(const QVector<Candle> &candles, int period, QVector<double> &out)
{
    out.clear(); if (candles.size() < period + 1) { out.resize(candles.size(), 50.0); return; }
    out.resize(candles.size(), 50.0);
    double avgGain = 0, avgLoss = 0;
    for (int i = 1; i <= period; ++i) {
        double ch = candles[i].close - candles[i-1].close;
        if (ch > 0) avgGain += ch; else avgLoss -= ch;
    }
    avgGain /= period; avgLoss /= period;
    out[period] = (avgLoss == 0) ? 100.0 : 100.0 - 100.0 / (1.0 + avgGain / avgLoss);
    for (int i = period + 1; i < candles.size(); ++i) {
        double ch = candles[i].close - candles[i-1].close;
        double gain = (ch > 0) ? ch : 0, loss = (ch > 0) ? 0 : -ch;
        avgGain = (avgGain * (period-1) + gain) / period;
        avgLoss = (avgLoss * (period-1) + loss) / period;
        out[i] = (avgLoss == 0) ? 100.0 : 100.0 - 100.0 / (1.0 + avgGain / avgLoss);
    }
}

void IndicatorCalculator::calcKDJ(const QVector<Candle> &candles, int p, int kf, int df,
                                  QVector<double> &outK, QVector<double> &outD, QVector<double> &outJ)
{
    outK.clear(); outD.clear(); outJ.clear(); int n = candles.size(); if (n == 0) return;
    outK.resize(n, 50.0); outD.resize(n, 50.0); outJ.resize(n, 50.0);
    for (int i = 0; i < n; ++i) {
        int s = qMax(0, i - p + 1); double lo = candles[s].low, hi = candles[s].high;
        for (int j = s + 1; j <= i; ++j) { lo = qMin(lo, candles[j].low); hi = qMax(hi, candles[j].high); }
        double rsv = (hi == lo) ? 50.0 : (candles[i].close - lo) / (hi - lo) * 100.0;
        if (i == 0) { outK[i] = rsv; outD[i] = rsv; }
        else { outK[i] = (kf-1.0)/kf * outK[i-1] + 1.0/kf * rsv; outD[i] = (df-1.0)/df * outD[i-1] + 1.0/df * outK[i]; }
        outJ[i] = 3.0 * outK[i] - 2.0 * outD[i];
    }
}

void IndicatorCalculator::calcMACD(const QVector<Candle> &candles,
                                   QVector<double> &outMacd, QVector<double> &outSignal, QVector<double> &outHist)
{
    outMacd.clear(); outSignal.clear(); outHist.clear(); if (candles.size() < 26) return;
    auto ema = [](const QVector<Candle> &c, int p, QVector<double> &r) {
        r.resize(c.size(), 0.0); double m = 2.0/(p+1), s = 0;
        for (int i = 0; i < p; ++i) s += c[i].close; r[p-1] = s/p;
        for (int i = p; i < c.size(); ++i) r[i] = (c[i].close - r[i-1]) * m + r[i-1];
    };
    QVector<double> e12, e26; ema(candles, 12, e12); ema(candles, 26, e26);
    outMacd.resize(candles.size(), 0.0);
    for (int i = 0; i < candles.size(); ++i) if (e12[i] != 0 && e26[i] != 0) outMacd[i] = e12[i] - e26[i];
    auto emaS = [](const QVector<double> &src, int p, QVector<double> &r) {
        r.resize(src.size(), 0.0); double m = 2.0/(p+1), s = 0;
        for (int i = 0; i < p; ++i) s += src[i]; r[p-1] = s/p;
        for (int i = p; i < src.size(); ++i) r[i] = (src[i] - r[i-1]) * m + r[i-1];
    };
    emaS(outMacd, 9, outSignal); outHist.resize(candles.size(), 0.0);
    for (int i = 0; i < candles.size(); ++i) if (outMacd[i] != 0 && outSignal[i] != 0) outHist[i] = outMacd[i] - outSignal[i];
}

void IndicatorCalculator::calcBollinger(const QVector<Candle> &candles, int period, double mult,
                                        QVector<double> &outMid, QVector<double> &outUpper, QVector<double> &outLower)
{
    calcMA(candles, period, outMid);
    outUpper.resize(candles.size(), 0.0); outLower.resize(candles.size(), 0.0);
    for (int i = period - 1; i < candles.size(); ++i) {
        double sumSq = 0;
        for (int j = i - period + 1; j <= i; ++j) { double dev = candles[j].close - outMid[i]; sumSq += dev * dev; }
        double stddev = qSqrt(sumSq / period);
        outUpper[i] = outMid[i] + mult * stddev; outLower[i] = outMid[i] - mult * stddev;
    }
}

void IndicatorCalculator::updateIndicators(const QString &symbol, int tf, const QVector<Candle> &c)
{
    QMutexLocker lock(&m_mutex); IndicatorCache cache;
    calcMA(c, 5, cache.ma5); calcMA(c, 10, cache.ma10); calcMA(c, 20, cache.ma20); calcMA(c, 60, cache.ma60);
    calcRSI(c, 14, cache.rsi14); calcKDJ(c, 9, 3, 3, cache.kdjK, cache.kdjD, cache.kdjJ);
    calcMACD(c, cache.macd, cache.macdSignal, cache.macdHist);
    calcBollinger(c, 20, 2.0, cache.bollMid, cache.bollUpper, cache.bollLower);
    m_caches[symbol + "@" + QString::number(tf)] = cache;
}

void IndicatorCalculator::appendCandle(const QString &symbol, int tf, const Candle &)
    { QMutexLocker lock(&m_mutex); m_caches.remove(symbol + "@" + QString::number(tf)); }

void IndicatorCalculator::updateLastCandle(const QString &symbol, int tf, const Candle &)
    { QMutexLocker lock(&m_mutex); m_caches.remove(symbol + "@" + QString::number(tf)); }

IndicatorCache IndicatorCalculator::getCache(const QString &symbol, int tf) const
    { QMutexLocker lock(&m_mutex); return m_caches.value(symbol + "@" + QString::number(tf)); }

double IndicatorCalculator::getMA(const QString &symbol, int tf, int period, int index) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_caches.find(symbol + "@" + QString::number(tf));
    if (it == m_caches.end()) return 0.0;
    const auto &c = it.value();
    if (period == 5 && index >= 0 && index < c.ma5.size()) return c.ma5[index];
    if (period == 10 && index >= 0 && index < c.ma10.size()) return c.ma10[index];
    if (period == 20 && index >= 0 && index < c.ma20.size()) return c.ma20[index];
    if (period == 60 && index >= 0 && index < c.ma60.size()) return c.ma60[index];
    return 0.0;
}

double IndicatorCalculator::getRSI(const QString &symbol, int tf, int, int idx) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_caches.find(symbol + "@" + QString::number(tf));
    if (it == m_caches.end()) return 0.0;
    return (idx >= 0 && idx < it.value().rsi14.size()) ? it.value().rsi14[idx] : 0.0;
}

void IndicatorCalculator::getKDJ(const QString &symbol, int tf, QVector<double> &k, QVector<double> &d, QVector<double> &j) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_caches.find(symbol + "@" + QString::number(tf));
    if (it == m_caches.end()) return;
    k = it.value().kdjK; d = it.value().kdjD; j = it.value().kdjJ;
}

void IndicatorCalculator::getMACD(const QString &symbol, int tf, QVector<double> &dif, QVector<double> &dea, QVector<double> &macd) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_caches.find(symbol + "@" + QString::number(tf));
    if (it == m_caches.end()) return;
    dif = it.value().macd; dea = it.value().macdSignal; macd = it.value().macdHist;
}

void IndicatorCalculator::clearCache(const QString &symbol, int tf)
    { QMutexLocker lock(&m_mutex); m_caches.remove(symbol + "@" + QString::number(tf)); }

void IndicatorCalculator::clearAll()
    { QMutexLocker lock(&m_mutex); m_caches.clear(); }



