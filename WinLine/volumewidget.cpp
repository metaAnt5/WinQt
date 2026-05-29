#include "volumewidget.h"
#include "chartconfig.h"
#include <QPainter>

VolumeWidget::VolumeWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(80);
}

void VolumeWidget::setData(const QVector<Candle> &data) {
    m_data = data;
    update();
}

void VolumeWidget::setViewport(int startIndex, int count) {
    m_viewStart = qBound(0, startIndex, qMax(0, m_data.size()-1));
    m_viewCount = qBound(0, count, qMax(0, m_data.size()-m_viewStart));
    update();
}

void VolumeWidget::setCrosshairIndex(int index) {
    m_crossIdx = index;
    update();
}

void VolumeWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_data.isEmpty()) return;
    int start = m_viewStart;
    int count = m_viewCount > 0 ? m_viewCount : m_data.size();
    if (start + count > m_data.size()) count = m_data.size() - start;
    if (count <= 0) return;
    QRect r = rect().adjusted(40, 4, -10, -4);
    double maxVol = 0; for (int i = start; i < start+count; ++i) maxVol = qMax(maxVol, m_data[i].volume);

    // 使用主图的 totalPer 和 mainRect.left() 确保 K 棒中心对齐
    double totalPer = ChartConfig::totalPer();
    int mainLeft = ChartConfig::mainRect().left();
    int mainStart = ChartConfig::startIndex();
    double barW = (totalPer > 0) ? totalPer : qMax(2.0, (r.width() / (double)qMax(1, count)));
    double drawLeft = (totalPer > 0) ? static_cast<double>(mainLeft) : static_cast<double>(r.left());

    for (int i = 0; i < count; ++i) {
        int idx = start + i;
        double candleCenter = drawLeft + (idx - mainStart) * totalPer + totalPer / 2.0;
        double x = candleCenter - barW * 0.4;  // volume bar centered on candle center
        double h = (m_data[idx].volume / maxVol) * r.height();
        QRectF br(x, r.bottom()-h, barW*0.8, h);
        bool rise = m_data[idx].close >= m_data[idx].open;
        QColor color = rise ? QColor(220,20,60) : QColor(0,180,0);
        p.fillRect(br, color);
    }
    // draw crosshair vertical — 使用主图的 candleCenterX 确保与主图精确对齐
    if (m_crossIdx >= start && m_crossIdx < start+count) {
        double x = static_cast<double>(ChartConfig::candleCenterX(m_crossIdx));
        p.setPen(QPen(Qt::magenta, 1.5, Qt::DashLine));
        p.drawLine(int(x), r.top(), int(x), r.bottom());
    }
}
