#include "marketsconfig.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QTreeWidget>
#include <QTreeWidgetItem>

MarketsConfig::MarketsConfig(QObject *parent) : QObject(parent)
{
}

bool MarketsConfig::loadFromFile(const QString &filePath)
{
    m_markets.clear();
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QXmlStreamReader xml(&f);

    // Expect a root <Markets>
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("Markets")) {
        f.close();
        return false;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("Market")) {
            MarketInfo market;
            market.name = xml.attributes().value("name").toString();
            market.dataDir = xml.attributes().value("dataDir").toString();
            market.apiType = xml.attributes().value("apiType").toString();
            market.filenamePattern = xml.attributes().value("filenamePattern").toString();
            market.readerType = xml.attributes().value("readerType").toString();

            // parse Symbol children
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("Symbol")) {
                    SymbolInfo sym;
                    sym.csvPath = xml.attributes().value("csvPath").toString();
                    QString text = xml.readElementText();
                    sym.name = text.trimmed();
                    market.symbols.append(sym);
                } else {
                    xml.skipCurrentElement();
                }
            }

            m_markets.append(market);
        } else {
            xml.skipCurrentElement();
        }
    }

    f.close();
    if (xml.hasError()) {
        m_markets.clear();
        return false;
    }
    return !m_markets.isEmpty();
}

void MarketsConfig::populateTree(QTreeWidget *tree) const
{
    if (!tree) return;
    tree->clear();
    for (const MarketInfo &m : m_markets) {
        QTreeWidgetItem *mi = new QTreeWidgetItem(tree);
        mi->setText(0, m.name);
        mi->setData(0, Qt::UserRole + 1, m.dataDir);
        mi->setData(0, Qt::UserRole + 2, m.apiType);
        for (const SymbolInfo &s : m.symbols) {
            QTreeWidgetItem *si = new QTreeWidgetItem(mi);
            si->setText(0, s.name);
            si->setData(0, Qt::UserRole + 3, s.csvPath);
            si->setData(0, Qt::UserRole + 1, m.dataDir);
            si->setData(0, Qt::UserRole + 2, m.apiType);
            si->setData(0, Qt::UserRole + 5, m.name);
            si->setData(0, Qt::UserRole + 6, m.filenamePattern);
            si->setData(0, Qt::UserRole + 7, m.readerType);
            // timeframes are handled globally; do not add timeframe children here
        }
    }
    tree->expandAll();
}
