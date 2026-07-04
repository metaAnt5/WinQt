#include "lineshape.h"
#include "../klinewidget.h"
#include <QPainterPath>

LineShape::LineShape()
{
    m_type = ShapeType::Line;
}

void LineShape::draw(QPainter &p, const KLineWidget *kw, bool selected) const
{
    QPointF screenP1, screenP2;
    kw->dataCoordToScreen(x1, y1, screenP1);
    kw->dataCoordToScreen(x1 + 1, y1, screenP2); // 水平线，p2 用相同价格

    QRect mr = kw->mainChartRect();
    QPen sp(selected ? Qt::yellow : (color.isValid() ? color : Qt::white));
    sp.setWidth(selected ? 3 : 2);
    sp.setCapStyle(Qt::RoundCap);
    p.setPen(sp);

    // 画无限水平线：取左边缘和右边缘的 Y 交点
    double y = screenP1.y();
    p.drawLine(mr.left(), (int)y, mr.right(), (int)y);

    // 选中时画端点手柄
    if (selected) {
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(screenP1.x(), y), 5, 5);
    }
}

int LineShape::hitTest(const QPointF &screenPt, const KLineWidget *kw) const
{
    QPointF screenP1, screenP2;
    kw->dataCoordToScreen(x1, y1, screenP1);
    kw->dataCoordToScreen(x1 + 1, y1, screenP2);

    // 端点检测
    double d1 = qSqrt((screenP1.x() - screenPt.x()) * (screenP1.x() - screenPt.x()) +
                      (screenP1.y() - screenPt.y()) * (screenP1.y() - screenPt.y()));
    if (d1 <= 10.0) return 1;

    // 无限水平线检测：垂直距离
    double dist = qAbs(screenP1.y() - screenPt.y());
    // 同时 X 必须在主图区域内
    QRect mr = kw->mainChartRect();
    if (dist <= 10.0 && screenPt.x() >= mr.left() && screenPt.x() <= mr.right())
        return -1;

    return -2;
}

void LineShape::moveBy(const QPointF &deltaScreen, const KLineWidget *kw)
{
    // 水平线只移动价格
    QRect mr = kw->mainChartRect();
    double priceRange = kw->m_maxPrice - kw->m_minPrice;
    if (priceRange <= 0 || mr.height() <= 0) return;
    double dPrice = -deltaScreen.y() * priceRange / mr.height();
    y1 += dPrice;
    // x1 保持不变（candleIdx），x2 延伸
    x2 = kw->data().isEmpty() ? x1 : qMax(x1, double(kw->data().size() - 1));
}

void LineShape::dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw)
{
    Q_UNUSED(endpoint)
    // 水平线只有一个可拖端点（价格）
    double candleIdx, price;
    kw->screenToDataCoord(screenPt, candleIdx, price);
    y1 = price;
}
