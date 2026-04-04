#pragma once
#include <QWidget>
#include <QVector>
#include "klinewidget.h"

class MacdWidget : public QWidget {
    Q_OBJECT
public:
    explicit MacdWidget(QWidget *parent = nullptr);
    void setData(const QVector<Candle> &data);
    void setViewport(int startIndex, int count);
    void setCrosshairIndex(int index);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QVector<Candle> m_data;
    QVector<double> m_dif, m_dea, m_hist;
    int m_viewStart = 0;
    int m_viewCount = 0;
    int m_crossIdx = -1;
    void computeMacd();
};
