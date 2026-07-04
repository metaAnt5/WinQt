#include "fixedshape.h"
#include "../klinewidget.h"

FixedShape::FixedShape()
{
    m_type = ShapeType::Fixed;
    m_attachment = Attachment::Fixed;
}

void FixedShape::draw(QPainter &p, const KLineWidget *kw, bool selected) const
{
    QRect cr = kw->mainChartRect();
    if (cr.isEmpty()) return;

    int sx = cr.left() + int(x1 * cr.width());
    int sy = cr.top() + int(y1 * cr.height());
    QPointF center(sx, sy);

    QColor sc = color.isValid() ? color : QColor(255, 200, 100);
    QPen pen(selected ? Qt::yellow : sc, 2);
    p.setPen(pen);

    QString label = text.isEmpty() ? (name.isEmpty() ? QStringLiteral("Note") : name) : text;
    QFont f = p.font();
    f.setPointSize(10);
    if (selected) f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    int tw = fm.horizontalAdvance(label) + 8;
    int th = fm.height() + 4;

    // 圆点
    double r = selected ? 9 : 8;
    p.setBrush(selected ? sc.lighter(170) : sc);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, r, r);

    // 文本背景
    QRectF bg(sx + r + 4, sy - th / 2, tw, th);
    p.setBrush(QColor(0, 0, 0, 160));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);

    // 文本
    p.setPen(selected ? QPen(sc.lighter(200), 2) : pen);
    p.drawText(bg, Qt::AlignCenter, label);
}

int FixedShape::hitTest(const QPointF &screenPt, const KLineWidget *kw) const
{
    QRect cr = kw->mainChartRect();
    if (cr.isEmpty()) return -2;

    int sx = cr.left() + int(x1 * cr.width());
    int sy = cr.top() + int(y1 * cr.height());
    QPointF center(sx, sy);

    double d = qSqrt((center.x() - screenPt.x()) * (center.x() - screenPt.x()) +
                     (center.y() - screenPt.y()) * (center.y() - screenPt.y()));
    return (d <= 15.0) ? -1 : -2;
}

void FixedShape::moveBy(const QPointF &deltaScreen, const KLineWidget *kw)
{
    QRect cr = kw->mainChartRect();
    if (cr.width() <= 0 || cr.height() <= 0) return;

    x1 = qBound(0.0, x1 + deltaScreen.x() / cr.width(), 1.0);
    y1 = qBound(0.0, y1 + deltaScreen.y() / cr.height(), 1.0);
}

void FixedShape::dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw)
{
    Q_UNUSED(endpoint)
    // 固定文本只有一个点，拖到新位置
    QPointF norm = kw->screenToNorm(screenPt.toPoint());
    x1 = norm.x();
    y1 = norm.y();
}
