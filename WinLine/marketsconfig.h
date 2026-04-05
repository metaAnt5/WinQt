#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>

class QTreeWidget;

struct TimeframeInfo {
    int minutes = 0;
    QString csvPath;
};
struct SymbolInfo {
    QString name;
    QString csvPath; // default csv for symbol
    QVector<TimeframeInfo> tfs;
};
struct MarketInfo {
    QString name;
    QString dataDir;
    QString apiType;
    QString filenamePattern; // optional pattern from XML
    QString readerType; // optional explicit reader type
    QVector<SymbolInfo> symbols;
};

class MarketsConfig : public QObject {
    Q_OBJECT
public:
    explicit MarketsConfig(QObject *parent = nullptr);
    bool loadFromFile(const QString &filePath);
    void populateTree(QTreeWidget *tree) const;
    bool isEmpty() const { return m_markets.isEmpty(); }
private:
    QVector<MarketInfo> m_markets;
};
