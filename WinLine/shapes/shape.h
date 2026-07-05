#pragma once

#include <QString>
#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <QPainter>
#include <QSharedPointer>
#include <QRect>

class KLineWidget;

enum class ShapeType { Line = 0, Trend, UpTriangle, DownTriangle, Fixed = 100, Text = 101 };
enum class Attachment { KLineBound = 0, Fixed };

// ─── 工具函数（几何计算，不依赖 KLineWidget） ───
// 点到无限直线的距离
double pointToLineDist2(const QPointF &p, const QPointF &a, const QPointF &b);
// 点到射线的距离（从 a 出发，经过 b）
double pointToRayDist2(const QPointF &p, const QPointF &a, const QPointF &b);
// 点到线段的距离（端点间）
double pointToSegDist2(const QPointF &p, const QPointF &a, const QPointF &b);
// 两条直线交点
bool intersectLines2(const QPointF &p1, const QPointF &p2,
                     const QPointF &q1, const QPointF &q2, QPointF &out);

// ─── Shape 基类 ───
class Shape {
public:
    int id = 0;
    QString name;
    QString text;
    QColor color;
    int ownerShapeId = 0;   // 父 shape id（0 = 无父）
    bool fromScript = false; // 脚本创建，不保存到磁盘
    // 交易字段
    double tradePrice = 0.0;
    QDateTime tradeTime;
    int quantity = 1;
    double profit = 0.0;
    // 脚本字段
    QString scriptName;
    QString scriptParams;
    // 所属品种周期（用于多品种过滤，防止跨品种显示）
    QString symbol;
    int timeframe = 0; // 周期（分钟），0=不限制

    // 坐标（子类按需使用）：
    //   KLineShape: (x,y) = (candleIndex, price)
    //   FixedShape: (x,y) = (normX, normY) 0..1
    double x1 = 0.0, y1 = 0.0;
    double x2 = 0.0, y2 = 0.0;  // 仅 TrendShape 用

    // Lua 脚本驱动的选中标记（与 UI 的 m_selectedShapeIndex 独立）
    bool selected = false;

    Shape();
    virtual ~Shape() = default;

    // ─── 类型 / 附着方式 ───
    virtual ShapeType type() const { return m_type; }
    void setType(ShapeType t) { m_type = t; }

    Attachment attachment() const { return m_attachment; }
    void setAttachment(Attachment a) { m_attachment = a; }

    // 是否可以手动拖动（拖动端点或整体移动）
    bool canDrag() const { return m_canDrag; }
    void setCanDrag(bool d) { m_canDrag = d; }

    // 是否跟随 K 线缩放/移动
    bool followsKLine() const { return m_attachment == Attachment::KLineBound; }

    // ─── 绘制 ───
    virtual void draw(QPainter &p, const KLineWidget *kw, bool selected) const = 0;

    // ─── 命中检测 ───
    // 返回值: >0 表示命中端点编号(1=p1, 2=p2), -1 表示命中线体, -2 表示未命中
    virtual int hitTest(const QPointF &screenPt, const KLineWidget *kw) const = 0;

    // ─── 移动整体（屏幕像素偏移量） ───
    virtual void moveBy(const QPointF &deltaScreen, const KLineWidget *kw) = 0;

    // ─── 拖动端点 ───
    // endpoint: 1=p1, 2=p2
    virtual void dragEndpoint(int endpoint, const QPointF &screenPt, const KLineWidget *kw) = 0;

    // ─── 序列化 ───
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject &obj);

    // 工厂方法：从 JSON 创建对应子类对象
    static QSharedPointer<Shape> createFromJson(const QJsonObject &obj);

protected:
    ShapeType m_type = ShapeType::Line;
    Attachment m_attachment = Attachment::KLineBound;
    bool m_canDrag = true;
};
