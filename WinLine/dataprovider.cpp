#include "dataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>
#include <QDebug>

FileDataProvider::FileDataProvider(const QString &dataDir, QObject *parent)
    : DataProvider(parent), m_dataDir(dataDir) {}

FileDataProvider::FileDataProvider(const QString &dataDir, const QString &filenamePattern, QObject *parent)
    : DataProvider(parent), m_dataDir(dataDir), m_filenamePattern(filenamePattern) {}

bool FileDataProvider::loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDir);
    qDebug() << "Resolved baseDir:" << baseDir << "for configured dataDir:" << m_dataDir;

    QString candidate = QString("%1_%2.csv").arg(symbol).arg(timeframeMinutes);
    QString path = QDir(baseDir).filePath(candidate);
    if (path.isEmpty() || !QFile::exists(path)) {
        qDebug() << "Data file not found for" << symbol << "in" << baseDir;
        return false;
    }
    QString sym; int baseMin=1;
    qDebug() << "Loading CSV from" << path;
    return loadCsvFile(path, out, sym, baseMin);
}

bool FileDataProvider::fetchRemoteData(const QString &/*symbol*/, int /*timeframeMinutes*/, QVector<Candle> &/*out*/)
{
    // not implemented
    return false;
}

RemoteDataProvider::RemoteDataProvider(const QString &endpoint, QObject *parent)
    : DataProvider(parent), m_endpoint(endpoint) {}

RemoteDataProvider::RemoteDataProvider(const QString &endpoint, const QString &filenamePattern, QObject *parent)
    : DataProvider(parent), m_endpoint(endpoint), m_filenamePattern(filenamePattern) {}

bool RemoteDataProvider::loadLocalData(const QString &/*symbol*/, int /*timeframeMinutes*/, QVector<Candle> &/*out*/, const QString &/*csvPath*/)
{
    return false;
}

bool RemoteDataProvider::fetchRemoteData(const QString &/*symbol*/, int /*timeframeMinutes*/, QVector<Candle> &/*out*/)
{
    // TODO: implement network fetching
    return false;
}
