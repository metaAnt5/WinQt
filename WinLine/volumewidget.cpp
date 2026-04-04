#include "volumewidget.h"
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
    double barW = qMax(2.0, (r.width() / (double)count));
    for (int i = 0; i < count; ++i) {
        int idx = start + i;
        double x = r.left() + i * barW;
        double h = (m_data[idx].volume / maxVol) * r.height();
        QRectF br(x, r.bottom()-h, barW*0.8, h);
        bool rise = m_data[idx].close >= m_data[idx].open;
        QColor color = rise ? QColor(220,20,60) : QColor(0,180,0);
        p.fillRect(br, color);
    }
    // draw crosshair vertical
    if (m_crossIdx >= start && m_crossIdx < start+count) {
        int i = m_crossIdx - start;
        double x = r.left() + i * barW + barW/2.0;
        p.setPen(QPen(Qt::magenta, 1.5, Qt::DashLine));
        p.drawLine(int(x), r.top(), int(x), r.bottom());
    }
}
