#include "macdwidget.h"
#include "chartconfig.h"
#include <QPainter>
#include <QRect>
#include <QPainterPath>

MacdWidget::MacdWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(100);
}

void MacdWidget::setData(const QVector<Candle> &data) {
    m_data = data; computeMacd(); update();
}

void MacdWidget::setViewport(int startIndex, int count) {
    m_viewStart = qBound(0, startIndex, qMax(0, m_data.size()-1));
    m_viewCount = qBound(0, count, qMax(0, m_data.size()-m_viewStart));
    update();
}

void MacdWidget::setLayout(int startIndex, int /*visibleCount*/, double totalPer, double /*candleBodyWidth*/, QRect mainChartRect) {
    m_layoutStart = startIndex;
    m_totalPer = totalPer > 0 ? totalPer : 1.0;
    m_mainLeft = mainChartRect.left();
}

void MacdWidget::setCrosshairIndex(int index) { m_crossIdx = index; update(); }

static void ema(const QVector<double> &src, QVector<double> &out, int period) {
    out.clear(); int n = src.size(); if (n==0) return; out.resize(n);
    double a = 2.0/(period+1);
    out[0] = src[0];
    for (int i=1;i<n;++i) out[i] = out[i-1] * (1-a) + src[i] * a;
}

void MacdWidget::computeMacd() {
    int n = m_data.size(); m_dif.clear(); m_dea.clear(); m_hist.clear();
    if (n==0) return;
    QVector<double> closes(n);
    for (int i=0;i<n;++i) closes[i]=m_data[i].close;
    QVector<double> ema12, ema26;
    ema(closes, ema12, 12);
    ema(closes, ema26, 26);
    m_dif.resize(n); m_dea.resize(n); m_hist.resize(n);
    for (int i=0;i<n;++i) m_dif[i]=ema12[i]-ema26[i];
    // dea is 9-period ema of dif
    ema(m_dif, m_dea, 9);
    for (int i=0;i<n;++i) m_hist[i]=m_dif[i]-m_dea[i];
}

void MacdWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_data.isEmpty()) return;
    int start = m_viewStart;
    int count = m_viewCount>0? m_viewCount : m_data.size();
    if (start+count > m_data.size()) count = m_data.size()-start;
    if (count<=0) return;
    QRect r = rect().adjusted(40,4,-10,-4);
    double maxV = 0; for (int i=start;i<start+count;++i) maxV = qMax(maxV, qAbs(m_hist[i]));
    // 使用主图的 layout 参数确保 K 棒中心对齐
    double totalPer = m_totalPer > 0 ? m_totalPer : qMax(2.0, (r.width() / (double)qMax(1, count)));
    int mainLeft = m_mainLeft > 0 ? m_mainLeft : r.left();
    int mainStart = m_layoutStart;
    double barW = totalPer;
    double drawLeft = static_cast<double>(mainLeft);

    for (int i=0;i<count;++i) {
        int idx = start+i;
        double candleCenter = drawLeft + (idx - mainStart) * totalPer + totalPer / 2.0;
        double x = candleCenter - barW * 0.4;  // hist bar centered on candle center
        double h = (m_hist[idx]/maxV) * (r.height()/2.0);
        QRectF br(x, r.center().y() - (h>0?h:0), barW*0.8, qAbs(h));
        QColor col = (m_hist[idx] >= 0) ? QColor(220,20,60) : QColor(0,180,0);
        p.fillRect(br, col);
    }
    // draw DIF/DEA
    QPainterPath pdif, pdea;
    for (int i=0;i<count;++i) {
        int idx = start+i;
        double candleCenter = drawLeft + (idx - mainStart) * totalPer + totalPer / 2.0;
        double ydif = r.center().y() - (m_dif[idx]/maxV) * (r.height()/2.0);
        double ydea = r.center().y() - (m_dea[idx]/maxV) * (r.height()/2.0);
        if (i==0) { pdif.moveTo(candleCenter, ydif); pdea.moveTo(candleCenter, ydea); }
        else { pdif.lineTo(candleCenter, ydif); pdea.lineTo(candleCenter, ydea); }
    }
    QPen penDif(Qt::yellow, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen penDea(Qt::cyan, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(penDif); p.drawPath(pdif);
    p.setPen(penDea); p.drawPath(pdea);

    // crosshair — 使用主图的 candleCenterX 确保与主图精确对齐
    if (m_crossIdx>=start && m_crossIdx < start+count) {
        double x = mainLeft + (m_crossIdx - mainStart) * totalPer + totalPer / 2.0;
        p.setPen(QPen(Qt::magenta, 1.5, Qt::DashLine));
        p.drawLine(int(x), r.top(), int(x), r.bottom());
    }
}
