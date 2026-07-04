#include "shape.h"
#include "lineshape.h"
#include "trendshape.h"
#include "triangleshape.h"
#include "fixedshape.h"
#include <QtMath>
#include <QJsonDocument>

// ─── Shape 构造函数 ───
Shape::Shape() {}

// ─── 几何工具函数 ───

double pointToLineDist2(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    double l2 = dx*dx + dy*dy;
    if (l2 <= 1e-12) return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    return qAbs((dy*(p.x()-a.x()) - dx*(p.y()-a.y())) / qSqrt(l2));
}

double pointToRayDist2(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    double l2 = dx*dx + dy*dy;
    if (l2 <= 1e-12) return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    double t = ((p.x()-a.x())*dx + (p.y()-a.y())*dy) / l2;
    if (t < 0.0)
        return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    double projx = a.x() + t * dx;
    double projy = a.y() + t * dy;
    return qSqrt((p.x()-projx)*(p.x()-projx) + (p.y()-projy)*(p.y()-projy));
}

double pointToSegDist2(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    double l2 = dx*dx + dy*dy;
    if (l2 <= 1e-12) return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    double t = ((p.x()-a.x())*dx + (p.y()-a.y())*dy) / l2;
    if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    double projx = a.x() + t * dx;
    double projy = a.y() + t * dy;
    return qSqrt((p.x()-projx)*(p.x()-projx) + (p.y()-projy)*(p.y()-projy));
}

bool intersectLines2(const QPointF &p1, const QPointF &p2,
                     const QPointF &q1, const QPointF &q2, QPointF &out)
{
    double x1 = p1.x(), y1 = p1.y();
    double x2 = p2.x(), y2 = p2.y();
    double x3 = q1.x(), y3 = q1.y();
    double x4 = q2.x(), y4 = q2.y();
    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (qFuzzyIsNull(denom)) return false;
    double det1 = x1*y2 - y1*x2;
    double det2 = x3*y4 - y3*x4;
    out.setX((det1*(x3 - x4) - (x1 - x2)*det2) / denom);
    out.setY((det1*(y3 - y4) - (y1 - y2)*det2) / denom);
    return true;
}

// ─── Shape 基类序列化 ───

QJsonObject Shape::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["type"] = static_cast<int>(type());
    obj["name"] = name;
    obj["text"] = text;
    obj["color"] = color.isValid() ? color.name() : "#FFFFFF";
    obj["ownerShapeId"] = ownerShapeId;
    obj["tradePrice"] = tradePrice;
    if (tradeTime.isValid())
        obj["tradeTime"] = tradeTime.toString(Qt::ISODate);
    obj["quantity"] = quantity;
    obj["profit"] = profit;
    obj["scriptName"] = scriptName;
    obj["scriptParams"] = scriptParams;
    if (followsKLine()) {
        obj["candleIdx1"] = x1;
        obj["price1"] = y1;
        obj["candleIdx2"] = x2;
        obj["price2"] = y2;
    } else {
        obj["normX"] = x1;
        obj["normY"] = y1;
    }
    return obj;
}

void Shape::fromJson(const QJsonObject &obj)
{
    id = obj["id"].toInt();
    name = obj["name"].toString();
    text = obj["text"].toString();
    color = QColor(obj["color"].toString("#FFFFFF"));
    ownerShapeId = obj["ownerShapeId"].toInt();
    tradePrice = obj["tradePrice"].toDouble();
    QString ts = obj["tradeTime"].toString();
    if (!ts.isEmpty()) tradeTime = QDateTime::fromString(ts, Qt::ISODate);
    quantity = obj["quantity"].toInt(1);
    profit = obj["profit"].toDouble();
    scriptName = obj["scriptName"].toString();
    scriptParams = obj["scriptParams"].toString();

    if (obj.contains("candleIdx1")) {
        x1 = obj["candleIdx1"].toDouble();
        y1 = obj["price1"].toDouble();
        x2 = obj["candleIdx2"].toDouble();
        y2 = obj["price2"].toDouble();
    } else {
        x1 = obj["normX"].toDouble(0.5);
        y1 = obj["normY"].toDouble(0.5);
    }
}

QSharedPointer<Shape> Shape::createFromJson(const QJsonObject &obj)
{
    int t = obj["type"].toInt();
    ShapeType st = static_cast<ShapeType>(t);

    QSharedPointer<Shape> s;
    switch (st) {
    case ShapeType::Line:
        s = QSharedPointer<LineShape>::create();
        break;
    case ShapeType::Trend:
        s = QSharedPointer<TrendShape>::create();
        break;
    case ShapeType::UpTriangle:
        s = QSharedPointer<TriangleShape>::create();
        qSharedPointerCast<TriangleShape>(s)->up = true;
        break;
    case ShapeType::DownTriangle:
        s = QSharedPointer<TriangleShape>::create();
        qSharedPointerCast<TriangleShape>(s)->up = false;
        break;
    case ShapeType::Fixed:
    case ShapeType::Text:
        s = QSharedPointer<FixedShape>::create();
        break;
    default:
        qWarning() << "Shape::createFromJson: unknown type" << t << "- skipping";
        return nullptr;
    }
    if (s) {
        s->fromJson(obj);
    }
    return s;
}
