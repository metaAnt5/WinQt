#include "indicatorwidget.h"
#include "chartconfig.h"
#include <QPainter>
#include <QtMath>
#include <QPainterPath>

IndicatorWidget::IndicatorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
}

void IndicatorWidget::setData(const QVector<Candle> &data)
{
    m_data = data;
    computeKDJ();
    update();
}

void IndicatorWidget::setViewport(int startIndex, int count)
{
    m_viewStart = qBound(0, startIndex, qMax(0, m_data.size()-1));
    m_viewCount = qBound(0, count, qMax(0, m_data.size()-m_viewStart));
    update();
}

void IndicatorWidget::setCrosshairIndex(int index)
{
    m_crosshairIndex = index;
    update();
}

void IndicatorWidget::computeKDJ(int period, double kInit, double dInit)
{
    m_k.clear(); m_d.clear(); m_j.clear();
    int n = m_data.size();
    if (n == 0) return;
    m_k.resize(n); m_d.resize(n); m_j.resize(n);
    QVector<double> rsv(n);
    for (int i = 0; i < n; ++i) {
        int start = qMax(0, i - period + 1);
        double low = m_data[start].low;
        double high = m_data[start].high;
        for (int j = start+1; j <= i; ++j) { low = qMin(low, m_data[j].low); high = qMax(high, m_data[j].high); }
        double close = m_data[i].close;
        if (high == low) rsv[i] = 50.0;
        else rsv[i] = (close - low) / (high - low) * 100.0;
        if (i == 0) {
            m_k[i] = kInit;
            m_d[i] = dInit;
        } else {
            m_k[i] = (2.0/3.0) * m_k[i-1] + (1.0/3.0) * rsv[i];
            m_d[i] = (2.0/3.0) * m_d[i-1] + (1.0/3.0) * m_k[i];
        }
        m_j[i] = 3.0 * m_k[i] - 2.0 * m_d[i];
    }
}

void IndicatorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::Antialiasing, true);

    // compute drawing rect with margins and additional padding so content isn't flush to edges
    int marginLeft = 40;
    int marginRight = 10;
    int padLeft = 10;
    int padRight = 12;
    int padTop = 8;
    int padBottom = 8;
    QRect rr = rect().adjusted(marginLeft + padLeft, padTop, -marginRight - padRight, -padBottom);
    p.setPen(Qt::gray);
    p.drawLine(rr.left(), rr.center().y(), rr.right(), rr.center().y());

    if (m_data.isEmpty()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("无指标数据"));
        return;
    }

    int start = m_viewStart;
    int count = m_viewCount > 0 ? m_viewCount : m_data.size();
    if (start + count > m_data.size()) count = m_data.size() - start;
    if (count <= 0) return;

    // scale KDJ to 0..100 using padded rect
    auto valToY = [&](double v){ double rratio = (v - 0.0) / 100.0; return rr.bottom() - rratio * rr.height(); };

    // 使用主图的 totalPer 和 mainRect.left() 确保 K 棒中心对齐
    double totalPer = ChartConfig::totalPer();
    int mainLeft = ChartConfig::mainRect().left();
    if (totalPer <= 0) totalPer = qMax(2.0, (rr.width() / (double)qMax(1, count)));
    if (mainLeft <= 0) mainLeft = rr.left();

    // draw horizontal lines at 20/80
    p.setPen(QPen(Qt::lightGray));
    int y20 = int(valToY(20)); int y80 = int(valToY(80));
    p.drawLine(rr.left(), y20, rr.right(), y20);
    p.drawLine(rr.left(), y80, rr.right(), y80);

    // draw K/D/J for visible range
    QPainterPath pk, pd, pj;
    for (int i = 0; i < count; ++i) {
        int idx = start + i;
        double x = mainLeft + (idx - ChartConfig::startIndex()) * totalPer + totalPer / 2.0;
        double yk = valToY(m_k[idx]);
        double yd = valToY(m_d[idx]);
        double yj = valToY(m_j[idx]);
        if (i == 0) { pk.moveTo(x, yk); pd.moveTo(x, yd); pj.moveTo(x, yj); }
        else { pk.lineTo(x, yk); pd.lineTo(x, yd); pj.lineTo(x, yj); }
    }
    QPen penK(Qt::red, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen penD(Qt::yellow, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen penJ(Qt::cyan, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(penK); p.drawPath(pk);
    p.setPen(penD); p.drawPath(pd);
    p.setPen(penJ); p.drawPath(pj);

    // draw crosshair vertical line — 使用主图的 candleCenterX 确保与主图精确对齐
    if (m_crosshairIndex >= start && m_crosshairIndex < start + count) {
        double x = static_cast<double>(ChartConfig::candleCenterX(m_crosshairIndex));
        p.setPen(QPen(Qt::magenta, 1.5, Qt::DashLine));
        p.drawLine(int(x), rr.top(), int(x), rr.bottom());
        // draw marker dot
        p.setBrush(Qt::magenta);
        p.drawEllipse(QPointF(x, rr.center().y()), 4, 4);
    }
}
