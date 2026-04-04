#pragma once

#include "klinewidget.h"
#include <QRandomGenerator>

static inline double randDoubleRange(double min, double max)
{
    return min + (max - min) * QRandomGenerator::global()->generateDouble();
}

static QVector<Candle> sampleKLineData()
{
    QVector<Candle> d;
    d.reserve(2000);
    double price = 10.0;
    QDateTime startDate(QDate(2025, 1, 1), QTime(9, 30));
    for (int i = 0; i < 2000; ++i) {
        double open = price;
        // random walk
        double change = randDoubleRange(-0.01, 0.01) * price; // -1% .. +1% per minute
        double close = qMax(0.01, open + change);
        double high = qMax(open, close) + randDoubleRange(0.0, 0.002) * price;
        double low = qMin(open, close) - randDoubleRange(0.0, 0.002) * price;
        double vol = QRandomGenerator::global()->bounded(100, 2000);
        Candle c{ startDate.addSecs(i * 60), open, high, low, close, vol };
        d.append(c);
        price = close; // next minute starts from close
    }
    return d;
}
