#pragma once
#include "shape.h"

struct TriangleShape : Shape {
    bool up = true;  // true=上三角, false=下三角

    TriangleShape();
    ShapeType type() const override { return up ? ShapeType::UpTriangle : ShapeType::DownTriangle; }

    void draw(QPainter &p, const KLineWidget *kw, bool selected) const override;
    int hitTest(const QPointF &screenPt, const KLineWidget *kw) const override;
    void moveBy(const QPointF &, const KLineWidget *) override {}  // 空操作
    void dragEndpoint(int, const QPointF &, const KLineWidget *) override {}
};
