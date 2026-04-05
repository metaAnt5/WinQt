#pragma once
#include <QString>
#include "dataprovider.h"

class ProviderFactory {
public:
    static DataProvider* createProvider(const QString &apiType, const QString &dataDir, const QString &filenamePattern = QString(), const QString &readerType = QString(), QObject *parent=nullptr);
};