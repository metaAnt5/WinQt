#pragma once
#include "dataprovider.h"

class FhDataProvider : public FileDataProvider {
    Q_OBJECT
public:
    explicit FhDataProvider(const QString &dataDir, QObject *parent=nullptr);
    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool loadRecent(const QString &symbol, int timeframeMinutes, int rows, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool loadRange(const QString &symbol, int skip, int count, QVector<Candle> &out, const QString &csvPath = QString()) override;
    // can override parsing behavior if FH files have special format
private:
    QString m_dataDirFh;
};
