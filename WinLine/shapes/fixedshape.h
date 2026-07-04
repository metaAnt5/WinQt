#pragma once
#include "shape.h"

struct FixedShape : Shape {
    FixedShape();
    ShapeType type() const override { return ShapeType::Fixed; }
    // 固定文本：可拖动整体，不跟随K线
    void draw(QPainter &p, const KLineWidget *kw, bool selected) const override;
    int hitTest(const QPointF &screenPt, const KLineWidget *kw) const override;
    void moveBy(const QPointF &deltaScreen, const KLineWidget *kw) override;
    void dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw) override;
};
