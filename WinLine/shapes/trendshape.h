#pragma once
#include "shape.h"

struct TrendShape : Shape {
    TrendShape();
    ShapeType type() const override { return ShapeType::Trend; }
    // 趋势线（射线）：可拖动两个端点和整体移动
    void draw(QPainter &p, const KLineWidget *kw, bool selected) const override;
    int hitTest(const QPointF &screenPt, const KLineWidget *kw) const override;
    void moveBy(const QPointF &deltaScreen, const KLineWidget *kw) override;
    void dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw) override;
};
