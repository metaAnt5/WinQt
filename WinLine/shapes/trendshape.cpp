#include "trendshape.h"
#include "../klinewidget.h"

TrendShape::TrendShape()
{
    m_type = ShapeType::Trend;
}

void TrendShape::draw(QPainter &p, const KLineWidget *kw, bool selected) const
{
    QPointF screenP1, screenP2;
    kw->dataCoordToScreen(x1, y1, screenP1);
    kw->dataCoordToScreen(x2, y2, screenP2);

    QPen sp(selected ? Qt::yellow : (color.isValid() ? color : QColor(100, 200, 255)));
    sp.setWidth(selected ? 3 : 2);
    sp.setCapStyle(Qt::RoundCap);
    p.setPen(sp);

    QRect mr = kw->mainChartRect();
    // 射线从 p1 通过 p2 延伸到主图边缘
    QLineF ln(screenP1, screenP2);
    if (qFuzzyIsNull(ln.dx()) && qFuzzyIsNull(ln.dy())) return;

    QPointF e1 = mr.topLeft(), e2 = mr.topRight();
    QPointF e3 = mr.bottomRight(), e4 = mr.bottomLeft();
    QPointF ip;
    double bestT = -1e12;
    QPointF bestPt;
    bool found = false;

    auto checkEdge = [&](const QPointF &a, const QPointF &b) {
        if (!intersectLines2(screenP1, screenP2, a, b, ip)) return;
        if (ip.x() < qMin(a.x(), b.x()) - 1e-6 || ip.x() > qMax(a.x(), b.x()) + 1e-6) return;
        if (ip.y() < qMin(a.y(), b.y()) - 1e-6 || ip.y() > qMax(a.y(), b.y()) + 1e-6) return;
        double t;
        if (!qFuzzyIsNull(ln.dx())) t = (ip.x() - screenP1.x()) / ln.dx();
        else t = (ip.y() - screenP1.y()) / ln.dy();
        if (t > 1e-6 && t > bestT) { bestT = t; bestPt = ip; found = true; }
    };

    checkEdge(e1, e2);
    checkEdge(e2, e3);
    checkEdge(e3, e4);
    checkEdge(e4, e1);

    if (found) {
        p.drawLine(screenP1, bestPt);
    } else {
        p.drawLine(screenP1, screenP2);
    }

    // 选中时画端点手柄
    if (selected) {
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(screenP1, 5, 5);
        p.drawEllipse(screenP2, 5, 5);
    }
}

int TrendShape::hitTest(const QPointF &screenPt, const KLineWidget *kw) const
{
    QPointF screenP1, screenP2;
    kw->dataCoordToScreen(x1, y1, screenP1);
    kw->dataCoordToScreen(x2, y2, screenP2);

    // 端点检测
    double d1 = qSqrt((screenP1.x()-screenPt.x())*(screenP1.x()-screenPt.x()) +
                      (screenP1.y()-screenPt.y())*(screenP1.y()-screenPt.y()));
    double d2 = qSqrt((screenP2.x()-screenPt.x())*(screenP2.x()-screenPt.x()) +
                      (screenP2.y()-screenPt.y())*(screenP2.y()-screenPt.y()));
    if (d1 <= 10.0) return 1;
    if (d2 <= 10.0) return 2;

    // 射线检测
    double rayDist = pointToRayDist2(screenPt, screenP1, screenP2);
    if (rayDist <= 10.0) return -1;

    return -2;
}

void TrendShape::moveBy(const QPointF &deltaScreen, const KLineWidget *kw)
{
    QRect mr = kw->mainChartRect();
    double priceRange = kw->m_maxPrice - kw->m_minPrice;
    double tp = kw->totalPer();
    if (priceRange <= 0 || mr.height() <= 0 || tp <= 0) return;

    double dPrice = -deltaScreen.y() * priceRange / mr.height();
    int dIdx = int(deltaScreen.x() / tp + 0.5);

    int maxIdx = kw->data().isEmpty() ? 0 : kw->data().size() - 1;
    x1 = qBound(0.0, x1 + dIdx, double(maxIdx));
    x2 = qBound(0.0, x2 + dIdx, double(maxIdx));
    y1 += dPrice;
    y2 += dPrice;
}

void TrendShape::dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw)
{
    double candleIdx, price;
    kw->screenToDataCoord(screenPt, candleIdx, price);
    if (endpoint == 1) {
        x1 = candleIdx;
        y1 = price;
    } else {
        x2 = candleIdx;
        y2 = price;
    }
}
