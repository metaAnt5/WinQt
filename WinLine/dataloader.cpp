#include "dataloader.h"
#include "dataprovider.h"
#include "providerfactory.h"
#include "klinewidget.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QTextEdit>
#include <QPointer>

DataLoader::DataLoader(KLineWidget *k, QObject *parent)
    : QObject(parent), m_k(k)
{
}

void DataLoader::requestInitialLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem)
{
    if (m_loading) return;
    
    // guard symItem validity: null, not in a tree, has children (market node), or empty symbol -> treat as invalid
    bool invalidItem = false;
    if (!symItem) invalidItem = true;
    else if (!symItem->treeWidget()) invalidItem = true;
    else if (symItem->childCount() > 0) invalidItem = true;
    else if (symbol.trimmed().isEmpty()) invalidItem = true;

    if (invalidItem) {
        m_loading = false;
        m_symbol.clear(); m_timeframe = timeframeMinutes; m_symItem = nullptr;
        if (m_k) {
            QVector<Candle> empty;
            m_k->setData(empty, timeframeMinutes);
        }
        QTextEdit *logText = nullptr;
        if (m_k) {
            QWidget *w = m_k->window(); if (w) logText = w->findChild<QTextEdit*>();
        }
        if (logText) logText->append(QStringLiteral("无数据：未选择品种或品种无效"));
        return;
    }

    m_loading = true;
    m_symbol = symbol; m_timeframe = timeframeMinutes; m_symItem = symItem;

    QString dataDir = symItem->data(0, Qt::UserRole + 1).toString();
    QString apiType = symItem->data(0, Qt::UserRole + 2).toString();
    QString filenamePattern = symItem->data(0, Qt::UserRole + 6).toString();
    QString readerType = symItem->data(0, Qt::UserRole + 7).toString();

    DataProvider *prov = ProviderFactory::createProvider(apiType.isEmpty() ? QStringLiteral("file") : apiType,
                                                        dataDir.isEmpty() ? QDir(QCoreApplication::applicationDirPath()).filePath("data") : dataDir,
                                                        filenamePattern, readerType, nullptr);

    QFutureWatcher<QVector<Candle>> *w = new QFutureWatcher<QVector<Candle>>(this);
    QObject::connect(w, &QFutureWatcher<QVector<Candle>>::finished, [this, w, prov]() {
        QVector<Candle> out = w->future().result();
        QTextEdit *logText = nullptr;
        if (m_k) { QWidget *win = m_k->window(); if (win) logText = win->findChild<QTextEdit*>(); }
        
        if (!out.isEmpty() && m_k) {
            // Limit to 6000 candles
            if (out.size() > 6000) {
                out = out.mid(out.size() - 6000);
            }
            m_k->setData(out, m_timeframe);
            if (logText) {
                QString csvPath = m_symItem ? m_symItem->data(0, Qt::UserRole + 3).toString() : QString();
                if (csvPath.isEmpty() && m_symItem) {
                    QString dataDir = m_symItem->data(0, Qt::UserRole + 1).toString();
                    QString filenamePattern = m_symItem->data(0, Qt::UserRole + 6).toString();
                    QString baseDir = dataDir.isEmpty() ? QDir(QCoreApplication::applicationDirPath()).filePath("data") : dataDir;
                    if (!filenamePattern.isEmpty()) {
                        QString p = filenamePattern; p.replace("%{symbol}", m_symbol); p.replace("%{tf}", QString::number(m_timeframe));
                        csvPath = QDir(baseDir).filePath(p);
                    } else csvPath = QDir(baseDir).filePath(m_symbol + ".csv");
                }
                logText->append(QStringLiteral("Loaded %1 %2min rows %3 from %4").arg(m_symbol).arg(m_timeframe).arg(out.size()).arg(csvPath));
            }
        } else {
            if (logText) logText->append(QStringLiteral("Failed to load data for %1 %2min").arg(m_symbol).arg(m_timeframe));
        }
        prov->deleteLater();
        m_loading = false;
        w->deleteLater();
    });

    QFuture<QVector<Candle>> f = QtConcurrent::run([prov, symbol, timeframeMinutes]() -> QVector<Candle> {
        QVector<Candle> out;
        if (!prov->loadLocalData(symbol, timeframeMinutes, out)) out.clear();
        return out;
    });
    w->setFuture(f);
}

bool DataLoader::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event);
}
