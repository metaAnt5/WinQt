#include "dataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

FileDataProvider::FileDataProvider(const QString &dataDir, QObject *parent)
    : DataProvider(parent), m_dataDir(dataDir), m_filenamePattern() {}

FileDataProvider::FileDataProvider(const QString &dataDir, const QString &filenamePattern, QObject *parent)
    : DataProvider(parent), m_dataDir(dataDir), m_filenamePattern(filenamePattern) {}

bool FileDataProvider::loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDir);

    QString path;
    if (!csvPath.isEmpty()) {
        if (QFileInfo(csvPath).isAbsolute()) path = csvPath;
        else path = QDir(baseDir).filePath(csvPath);
    } else {
        if (!m_filenamePattern.isEmpty() && timeframeMinutes > 0) {
            QString fname = m_filenamePattern;
            fname.replace("%{symbol}", symbol);
            fname.replace("%{tf}", QString::number(timeframeMinutes));
            QString p = QDir(baseDir).filePath(fname);
            if (QFile::exists(p)) path = p;
        }
        if (path.isEmpty() && timeframeMinutes > 0) {
            QString p = QDir(baseDir).filePath(QString("%1_%2.csv").arg(symbol).arg(timeframeMinutes));
            if (QFile::exists(p)) path = p;
        }
        if (path.isEmpty()) {
            QString p = QDir(baseDir).filePath(symbol + ".csv");
            if (QFile::exists(p)) path = p;
        }
    }
    if (path.isEmpty()) return false;
    QString sym; int baseMin=1;
    return loadCsvFile(path, out, sym, baseMin);
}
