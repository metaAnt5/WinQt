#pragma once
#include "dataprovider.h"

class CryptoDataProvider : public RemoteDataProvider {
    Q_OBJECT
public:
    explicit CryptoDataProvider(const QString &endpoint, QObject *parent=nullptr);
    explicit CryptoDataProvider(const QString &endpoint, const QString &filenamePattern, QObject *parent=nullptr);
};