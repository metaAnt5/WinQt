#include "cryptodataprovider.h"

CryptoDataProvider::CryptoDataProvider(const QString &endpoint, QObject *parent)
    : RemoteDataProvider(endpoint, parent) {}

CryptoDataProvider::CryptoDataProvider(const QString &endpoint, const QString &filenamePattern, QObject *parent)
    : RemoteDataProvider(endpoint, filenamePattern, parent) {}
