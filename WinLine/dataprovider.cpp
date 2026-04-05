#include "dataprovider.h"
#include "csvloader.h"
#include "apppaths.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryFile>

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

bool FileDataProvider::loadRecent(const QString &symbol, int timeframeMinutes, int rows, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDir);
    QString path;
    if (!csvPath.isEmpty()) {
        if (QFileInfo(csvPath).isAbsolute()) path = csvPath;
        else path = QDir(baseDir).filePath(csvPath);
    } else {
        if (!m_filenamePattern.isEmpty() && timeframeMinutes>0) {
            QString fname = m_filenamePattern;
            fname.replace("%{symbol}", symbol);
            fname.replace("%{tf}", QString::number(timeframeMinutes));
            QString candidate = QDir(baseDir).filePath(fname);
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
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    qint64 fileSize = f.size();
    const qint64 bufSize = 8192;
    QByteArray buffer; QByteArray partial;
    qint64 pos = fileSize;
    QList<QByteArray> lines;
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
    QByteArray header = f.readLine();
    tmp.write(header);
    int start = qMax(0, lines.size() - rows);
    for (int i = start; i < lines.size(); ++i) tmp.write(lines.at(i));
    tmp.flush(); tmp.seek(0);
    QString tmpPath = tmp.fileName();
    QString sym; int baseMin=1;
    bool ok = loadCsvFile(tmpPath, out, sym, baseMin);
    tmp.close();
    return ok;
}

bool FileDataProvider::buildIndexFile(const QString &csvPath, int stride)
{
    QString path = csvPath;
    if (!QFile::exists(path)) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QVector<qint64> offsets;
    qint64 pos = 0; qint64 lineNo = 0;
    while (!f.atEnd()) {
        qint64 start = pos;
        QByteArray line = f.readLine();
        pos = f.pos();
        if (lineNo % stride == 0) offsets.append(start);
        lineNo++;
    }
    QString idxPath = path + ".idx";
    QFile idx(idxPath);
    if (!idx.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&idx);
    ds << offsets;
    idx.close();
    return true;
}

bool FileDataProvider::ensureIndexExists(const QString &csvPath, int stride)
{
    QString idxPath = csvPath + ".idx";
    if (QFile::exists(idxPath)) return true;
    return buildIndexFile(csvPath, stride);
}

bool FileDataProvider::loadIndex(const QString &idxPath, QVector<qint64> &outOffsets)
{
    QFile idx(idxPath);
    if (!idx.open(QIODevice::ReadOnly)) return false;
    QDataStream ds(&idx);
    ds >> outOffsets;
    idx.close();
    return true;
}

bool FileDataProvider::loadRange(const QString &symbol, int skip, int count, QVector<Candle> &out, const QString &csvPath)
{
    QString baseDir = AppPaths::resolveDataDir(m_dataDir);
    QString path;
    if (!csvPath.isEmpty()) {
        if (QFileInfo(csvPath).isAbsolute()) path = csvPath;
        else path = QDir(baseDir).filePath(csvPath);
    } else {
        QString p = QDir(baseDir).filePath(symbol + ".csv");
        if (QFile::exists(p)) path = p;
    }
    if (path.isEmpty()) {
        qDebug() << "loadRange: no path for" << symbol;
        return false;
    }

    QString idxPath = path + ".idx";
    if (QFile::exists(idxPath)) {
        QVector<qint64> offsets;
        if (loadIndex(idxPath, offsets)) {
            int stride = 100; // must match index creation
            int block = skip / stride;
            int startLine = block * stride;
            qint64 startPos = (block < offsets.size()) ? offsets.at(block) : 0;
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) {
                f.seek(startPos);
                int curLine = startLine;
                while (curLine < skip && !f.atEnd()) { f.readLine(); curLine++; }
                QTemporaryFile tmp;
                if (tmp.open()) {
                    // write header
                    f.seek(0);
                    QByteArray h = f.readLine(); tmp.write(h);
                    int wrote = 0;
                    for (int i = 0; i < count && !f.atEnd(); ++i) {
                        QByteArray line = f.readLine(); if (line.isEmpty()) break; tmp.write(line); wrote++; }
                    tmp.flush(); tmp.seek(0);
                    QString tmpPath = tmp.fileName(); QString sym; int baseMin=1;
                    bool ok = loadCsvFile(tmpPath, out, sym, baseMin);
                    tmp.close(); f.close();
                    if (ok && !out.isEmpty()) {
                        return true;
                    }
                } else {
                    f.close();
                }
            } else {
            }
        } else {
        }
    } else {
    }

    // fallback: scan from end to collect last skip+count lines
    QFile f2(path);
    if (!f2.open(QIODevice::ReadOnly)) { qDebug() << "loadRange: cannot open file for tail scan"; return false; }
    qint64 fileSize = f2.size();
    const qint64 bufSize2 = 8192;
    QByteArray buffer2; QByteArray partial2;
    qint64 pos2 = fileSize;
    QList<QByteArray> lines2;
    while (pos2 > 0 && lines2.size() < skip + count + 1) {
        qint64 toRead = qMin(bufSize2, pos2);
        pos2 -= toRead;
        f2.seek(pos2);
        buffer2 = f2.read(toRead);
        for (int i = buffer2.size()-1; i>=0; --i) {
            char c = buffer2.at(i);
            partial2.prepend(c);
            if (c == '\n') {
                lines2.prepend(partial2);
                partial2.clear();
                if (lines2.size() >= skip + count + 1) break;
            }
        }
    }
    if (!partial2.isEmpty()) lines2.prepend(partial2);
    int total = lines2.size();
    QTemporaryFile tmp2;
    if (!tmp2.open()) { f2.close(); qDebug() << "loadRange: failed to open tmp2"; return false; }
    f2.seek(0);
    QByteArray header2 = f2.readLine(); tmp2.write(header2);
    int startIdx = qMax(0, total - (skip + count));
    int endIdx = qMax(0, total - skip);
    for (int i = startIdx; i < endIdx; ++i) tmp2.write(lines2.at(i));
    tmp2.flush(); tmp2.seek(0);
    QString tmpPath2 = tmp2.fileName(); QString sym2; int baseMin2=1;
    bool ok2 = loadCsvFile(tmpPath2, out, sym2, baseMin2);
    tmp2.close(); f2.close();
    return ok2;
}

// RemoteDataProvider placeholders
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
    return false;
}
