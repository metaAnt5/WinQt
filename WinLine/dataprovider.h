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
    virtual bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) = 0;
};

class FileDataProvider : public DataProvider {
    Q_OBJECT
public:
    explicit FileDataProvider(const QString &dataDir, QObject *parent=nullptr);
    explicit FileDataProvider(const QString &dataDir, const QString &filenamePattern, QObject *parent=nullptr);

    bool loadLocalData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out, const QString &csvPath = QString()) override;
    bool fetchRemoteData(const QString &symbol, int timeframeMinutes, QVector<Candle> &out) override { return false; }

private:
    QString m_dataDir;
protected:
    QString m_filenamePattern;
};
