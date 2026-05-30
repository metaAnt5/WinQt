#pragma once

#include <QString>
#include <QVector>
#include "klinewidget.h"

// load CSV file; returns true on success. symbol and minutes (base timeframe) are extracted from filename if possible.
bool loadCsvFile(const QString &path, QVector<Candle> &outData, QString &symbol, int &baseMinutes);

// append candles to CSV file (sorted by time, skip duplicates by time)
// returns true on success
bool appendCsvFile(const QString &path, const QVector<Candle> &candles);
