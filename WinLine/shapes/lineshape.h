#pragma once
#include "shape.h"

struct LineShape : Shape {
    LineShape();
    ShapeType type() const override { return ShapeType::Line; }
    // 水平线：可拖动端点（价格）和整体移动
    void draw(QPainter &p, const KLineWidget *kw, bool selected) const override;
    int hitTest(const QPointF &screenPt, const KLineWidget *kw) const override;
    void moveBy(const QPointF &deltaScreen, const KLineWidget *kw) override;
    void dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw) override;
};
