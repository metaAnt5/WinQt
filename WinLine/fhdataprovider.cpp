#include "fhdataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>
#include <QTemporaryFile>

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

bool FhDataProvider::loadRecent(const QString &symbol, int timeframeMinutes, int rows, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDirFh);
    QString path;
    if (!csvPath.isEmpty()) {
        if (QFileInfo(csvPath).isAbsolute()) path = csvPath;
        else path = QDir(baseDir).filePath(csvPath);
    } else {
        if (!m_filenamePattern.isEmpty() && timeframeMinutes>0) {
            QString p = m_filenamePattern;
            p.replace("%{symbol}", symbol);
            p.replace("%{tf}", QString::number(timeframeMinutes));
            QString candidate = QDir(baseDir).filePath(p);
            if (QFile::exists(candidate)) path = candidate;
        }
        if (path.isEmpty() && timeframeMinutes>0) {
            QString candidate = QDir(baseDir).filePath(QString("%1%2.csv").arg(symbol).arg(timeframeMinutes));
            if (QFile::exists(candidate)) path = candidate;
            if (path.isEmpty()) {
                QString candidate2 = QDir(baseDir).filePath(QString("%1_%2.csv").arg(symbol).arg(timeframeMinutes));
                if (QFile::exists(candidate2)) path = candidate2;
            }
        }
        if (path.isEmpty()) {
            QString candidate = QDir(baseDir).filePath(symbol + ".csv");
            if (QFile::exists(candidate)) path = candidate;
        }
    }
    if (path.isEmpty() || !QFile::exists(path)) return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    qint64 fileSize = f.size();
    const qint64 bufSize = 8192;
    QByteArray buffer;
    qint64 pos = fileSize;
    QList<QByteArray> lines;
    QByteArray partial;
    while (pos > 0 && lines.size() < rows + 1) {
        qint64 toRead = qMin(bufSize, pos);
        pos -= toRead;
        f.seek(pos);
        buffer = f.read(toRead);
        for (int i = buffer.size()-1; i>=0; --i) {
            char c = buffer.at(i);
            partial.prepend(c);
            if (c == '\n') {
                lines.prepend(partial);
                partial.clear();
                if (lines.size() >= rows + 1) break;
            }
        }
    }
    if (!partial.isEmpty()) lines.prepend(partial);
    QTemporaryFile tmp;
    if (!tmp.open()) return false;
    f.seek(0);
    QByteArray headerLine = f.readLine();
    tmp.write(headerLine);
    int start = qMax(0, lines.size() - rows);
    for (int i = start; i < lines.size(); ++i) tmp.write(lines.at(i));
    tmp.flush(); tmp.seek(0);
    QString pathTmp = tmp.fileName();
    QString sym; int baseMin=1;
    bool ok = loadCsvFile(pathTmp, out, sym, baseMin);
    tmp.close();
    return ok;
}

bool FhDataProvider::loadRange(const QString &symbol, int skip, int count, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDirFh);
    QString path;
    if (!csvPath.isEmpty()) {
        if (QFileInfo(csvPath).isAbsolute()) path = csvPath;
        else path = QDir(baseDir).filePath(csvPath);
    } else {
        QString p = m_filenamePattern;
        if (!p.isEmpty()) {
            p.replace("%{symbol}", symbol);
            p.replace("%{tf}", QString::number(0)); // timeframe unknown here
            if (QFile::exists(QDir(baseDir).filePath(p))) path = QDir(baseDir).filePath(p);
        }
        if (path.isEmpty()) {
            QString fallback = QDir(baseDir).filePath(symbol + ".csv");
            if (QFile::exists(fallback)) path = fallback;
        }
    }
    if (path.isEmpty() || !QFile::exists(path)) return false;
    bool ok = FileDataProvider::loadRange(symbol, skip, count, out, path);
    return ok;
}
