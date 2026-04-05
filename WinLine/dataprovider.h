#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct Candle; // forward

class DataProvider : public QObject {
    Q_OBJECT
public:
    explicit DataProvider(QObject *parent=nullptr) : QObject(parent) {}
    virtual ~DataProvider() {}
    virtual bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) = 0;
    virtual bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) = 0;
};

class FileDataProvider : public DataProvider {
    Q_OBJECT
public:
    explicit FileDataProvider(const QString &dataDir, QObject *parent=nullptr);
    explicit FileDataProvider(const QString &dataDir, const QString &filenamePattern, QObject *parent=nullptr);
    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) override;
private:
    QString m_dataDir;
    QString m_filenamePattern; // pattern e.g. "%{symbol}_%{tf}.csv" or "%{symbol}%{tf}.csv"
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