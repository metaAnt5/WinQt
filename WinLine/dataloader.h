#pragma once

#include <QObject>
#include <QPointer>

struct Candle;
class KLineWidget;
class QTreeWidgetItem;

class DataLoader : public QObject {
    Q_OBJECT
public:
    explicit DataLoader(KLineWidget *k, QObject *parent=nullptr);
    void requestInitialLoad(const QString &symbol, int timeframeMinutes, QTreeWidgetItem *symItem);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<KLineWidget> m_k;
    QString m_symbol;
    int m_timeframe = 1;
    QTreeWidgetItem *m_symItem = nullptr;
    bool m_loading = false;
};