#pragma once

#include <QWidget>
#include <QVector>
#include "klinewidget.h"

class IndicatorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorWidget(QWidget *parent = nullptr);
    void setData(const QVector<Candle> &data);

public slots:
    void setViewport(int startIndex, int count);
    void setCrosshairIndex(int index);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Candle> m_data;
    int m_viewStart = 0;
    int m_viewCount = 0;
    int m_crosshairIndex = -1;
    QVector<double> m_k, m_d, m_j;
    void computeKDJ(int period = 9, double kInit = 50.0, double dInit = 50.0);
};
