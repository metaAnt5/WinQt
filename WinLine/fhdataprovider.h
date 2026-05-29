#pragma once
#include "dataprovider.h"

class FhDataProvider : public FileDataProvider {
    Q_OBJECT
public:
    explicit FhDataProvider(const QString &dataDir, QObject *parent=nullptr);
    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
private:
    QString m_dataDirFh;
};
