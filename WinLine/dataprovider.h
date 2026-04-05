#pragma once

#include <QObject>
#include <QVector>
#include <QString>

struct Candle;

class DataProvider : public QObject {
    Q_OBJECT
public:
    explicit DataProvider(QObject *parent=nullptr) : QObject(parent) {}
    virtual ~DataProvider() {}

    virtual bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) = 0;
    // load only the most recent `rows` entries
    virtual bool loadRecent(const QString &symbol, int timeframeMinutes, int rows, QVector<Candle> &out, const QString &csvPath = QString()) { Q_UNUSED(symbol); Q_UNUSED(timeframeMinutes); Q_UNUSED(rows); Q_UNUSED(out); Q_UNUSED(csvPath); return false; }
    // load a block of 'count' rows skipping the last 'skip' rows (older data)
    virtual bool loadRange(const QString &symbol, int skip, int count, QVector<Candle> &out, const QString &csvPath = QString()) { Q_UNUSED(symbol); Q_UNUSED(skip); Q_UNUSED(count); Q_UNUSED(out); Q_UNUSED(csvPath); return false; }

    virtual bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) = 0;
};

class FileDataProvider : public DataProvider {
    Q_OBJECT
public:
    explicit FileDataProvider(const QString &dataDir, QObject *parent=nullptr);
    explicit FileDataProvider(const QString &dataDir, const QString &filenamePattern, QObject *parent=nullptr);

    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool loadRecent(const QString &symbol, int timeframeMinutes, int rows, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool loadRange(const QString &symbol, int skip, int count, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) override { return false;}

    // build index file for given csv path (creates csvPath.idx). Thread-safe; can be called asynchronously.
    static bool buildIndexFile(const QString &csvPath, int stride = 100);

private:
    QString m_dataDir;
protected:
    QString m_filenamePattern; // pattern e.g. "%{symbol}_%{tf}.csv" or "%{symbol}%{tf}.csv"

    // index helpers: create/load an index file storing offsets every `stride` lines
    bool ensureIndexExists(const QString &csvPath, int stride = 100);
    bool loadIndex(const QString &idxPath, QVector<qint64> &outOffsets);
};

class RemoteDataProvider : public DataProvider {
    Q_OBJECT
public:
    explicit RemoteDataProvider(const QString &endpoint, QObject *parent=nullptr);
    explicit RemoteDataProvider(const QString &endpoint, const QString &filenamePattern, QObject *parent=nullptr);

    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) override;

private:
    QString m_endpoint;
    QString m_filenamePattern;
};
