#include "fhdataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>
#include <QDebug>

FhDataProvider::FhDataProvider(const QString &dataDir, QObject *parent)
    : FileDataProvider(dataDir, QStringLiteral("%{symbol}%{tf}.csv"), parent), m_dataDirFh(dataDir) {}

bool FhDataProvider::loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath)
{
    // First try base class behavior (respects csvPath and filenamePattern)
    if (FileDataProvider::loadLocalData(symbol, timeframeMinutes, out, csvPath)) return true;

    // Otherwise try common FH-style name candidates
    QString baseDir = AppPaths::resolveDataDir(m_dataDirFh);
    QString candidates = QString("%1%2.csv").arg(symbol).arg(timeframeMinutes);

    QString path = QDir(baseDir).filePath(candidates);
    qDebug() << "FhDataProvider trying" << path;
    if (QFile::exists(path)) {
        QString sym; int baseMin=1;
        if (loadCsvFile(path, out, sym, baseMin)) return true;
    }

    return false;
}
