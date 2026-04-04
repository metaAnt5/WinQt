#pragma once

#include <QString>
#include <QVector>
#include "klinewidget.h"

// load CSV file; returns true on success. symbol and minutes (base timeframe) are extracted from filename if possible.
bool loadCsvFile(const QString &path, QVector<Candle> &outData, QString &symbol, int &baseMinutes);
