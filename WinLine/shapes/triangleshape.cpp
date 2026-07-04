#include "triangleshape.h"
#include "../klinewidget.h"

TriangleShape::TriangleShape()
{
    m_type = ShapeType::UpTriangle; // 默认上三角，up flag 由创建者设置
    m_canDrag = false;
}

void TriangleShape::draw(QPainter &p, const KLineWidget *kw, bool selected) const
{
    // 三角形跟随 K 线，只有索引在可见范围时才绘制
    int idx = static_cast<int>(x1);
    if (idx < kw->m_startIndex || idx >= kw->m_startIndex + kw->visibleCount())
        return;

    double bw = kw->candleBodyWidth();
    if (bw < 2.0) bw = 8.0 * kw->m_scale;
    double h = bw * 1.2;
    double cx = kw->candleCenterXForIndex(x1);

    QPointF screenP1;
    kw->dataCoordToScreen(x1, y1, screenP1);
    double cy = screenP1.y();

    QPolygonF tri;
    if (up) {
        tri << QPointF(cx, cy - h)
            << QPointF(cx - bw/2.0, cy)
            << QPointF(cx + bw/2.0, cy);
    } else {
        tri << QPointF(cx, cy + h)
            << QPointF(cx - bw/2.0, cy)
            << QPointF(cx + bw/2.0, cy);
    }

    QColor fillColor = selected ? color.lighter(150) : (color.isValid() ? color : (up ? QColor(100,255,100) : QColor(255,100,100)));
    p.setBrush(fillColor);
    p.setPen(selected ? QPen(Qt::yellow, 2) : QPen(fillColor.darker(130), 1));
    p.drawPolygon(tri);
}

int TriangleShape::hitTest(const QPointF &screenPt, const KLineWidget *kw) const
{
    int idx = static_cast<int>(x1);
    if (idx < kw->m_startIndex || idx >= kw->m_startIndex + kw->visibleCount())
        return -2;

    double bw = kw->candleBodyWidth();
    if (bw < 2.0) bw = 8.0 * kw->m_scale;
    double h = bw * 1.2;
    double cx = kw->candleCenterXForIndex(x1);

    QPointF screenP1;
    kw->dataCoordToScreen(x1, y1, screenP1);
    double cy = screenP1.y();

    QPolygonF tri;
    if (up) {
        tri << QPointF(cx, cy - h)
            << QPointF(cx - bw/2.0, cy)
            << QPointF(cx + bw/2.0, cy);
    } else {
        tri << QPointF(cx, cy + h)
            << QPointF(cx - bw/2.0, cy)
            << QPointF(cx + bw/2.0, cy);
    }

    // 检测点是否在三角形内（扩展 5px 容差）
    QPolygonF expanded;
    for (const QPointF &pt : tri) {
        double dx = pt.x() - cx;
        double dy = pt.y() - cy;
        double scale = 1.0 + 5.0 / qMax(qAbs(dx), qAbs(dy)); // 向外扩展5px
        expanded << QPointF(cx + dx * scale, cy + dy * scale);
    }

    return expanded.containsPoint(screenPt, Qt::OddEvenFill) ? -1 : -2;
}
