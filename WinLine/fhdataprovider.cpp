#include "fhdataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>

FhDataProvider::FhDataProvider(const QString &dataDir, QObject *parent)
    : FileDataProvider(dataDir, QStringLiteral("%{symbol}%{tf}.csv"), parent), m_dataDirFh(dataDir) {}

bool FhDataProvider::loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath)
{
    if (FileDataProvider::loadLocalData(symbol, timeframeMinutes, out, csvPath)) return true;
    QString baseDir = AppPaths::resolveDataDir(m_dataDirFh);
    QStringList candidates;
    if (timeframeMinutes > 0) {
        candidates << QString("%1%2.csv").arg(symbol).arg(timeframeMinutes);
        candidates << QString("%1-%2.csv").arg(symbol).arg(timeframeMinutes);
        candidates << QString("%1_%2.csv").arg(symbol).arg(timeframeMinutes);
    }
    candidates << QString("%1.csv").arg(symbol);
    for (const QString &c : candidates) {
        QString path = QDir(baseDir).filePath(c);
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        QString sym; int baseMin=1;
        bool ok = loadCsvFile(path, out, sym, baseMin);
        if (ok && !out.isEmpty()) return true;
    }
    return false;
}
