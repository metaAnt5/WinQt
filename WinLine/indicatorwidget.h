#pragma once

#include <QWidget>
#include <QVector>
#include <QRect>
#include "klinewidget.h"

class IndicatorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorWidget(QWidget *parent = nullptr);
    void setData(const QVector<Candle> &data);
    void setSymbol(const QString &s) { m_symbol = s; }
    void setTimeframe(int tf) { m_timeframe = tf; }
    void loadKDJ() { computeKDJ(); }


public slots:
    void setViewport(int startIndex, int count);
    void setCrosshairIndex(int index);
    void setLayout(int startIndex, int visibleCount, double totalPer, double candleBodyWidth, QRect mainChartRect);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Candle> m_data;
    QString m_symbol;
    int m_timeframe = 0;

    int m_viewStart = 0;
    int m_viewCount = 0;
    int m_layoutStart = 0;
    double m_totalPer = 1.0;
    int m_mainLeft = 0;
    int m_crosshairIndex = -1;
    QVector<double> m_k, m_d, m_j;
    void computeKDJ(int period = 9, double kInit = 50.0, double dInit = 50.0);
};
