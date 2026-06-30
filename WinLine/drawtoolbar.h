#pragma once
#include <QToolBar>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QAction>
#include <QKeySequence>
#include <functional>

class KLineWidget;

// ============================================================
// 创建画图工具栏（共用函数）
// 主窗口和模拟回放窗口使用同一份代码，避免重复
// 
// 快捷键:
//   Esc     — 选择/鼠标
//   L       — 直线（无限延长）
//   T       — 射线（趋势线）
//   H       — 水平线
//   V       — 垂直线
//   X       — 文字标注
//   U       — 上箭头（买入）
//   D       — 下箭头（卖出）
//   Delete  — 删除选中图形
//   Shift+Delete — 清除所有图形
// ============================================================
inline QToolBar* createDrawingToolbar(KLineWidget *k, QWidget *parent)
{
    auto makeIcon = [](std::function<void(QPainter&)> draw) -> QIcon {
        QPixmap px(24, 24);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        draw(p);
        p.end();
        return QIcon(px);
    };

    // Normal: 鼠标指针
    QIcon iconNormal = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(3,3, 3,21);
        p.drawLine(3,3, 15,10);
        p.drawLine(3,12, 12,17);
        p.drawLine(15,10, 21,21);
    });
    // Line（水平线）：一条水平横线 + 左侧小圆点，表示单点创建水平线
    QIcon iconLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,50), 2.5));
        p.drawLine(2,12, 22,12);
        p.setBrush(QColor(200,200,50));
        p.drawEllipse(QPoint(3,12), 3,3);
    });
    // Trend（趋势线/射线）：从起点出发的斜线 + 末端箭头
    QIcon iconTrend = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(80,180,255), 2.5));
        p.drawLine(3,21, 13,7);
        // 箭头
        p.setBrush(QColor(80,180,255));
        QPolygonF arrow;
        arrow << QPointF(13,7) << QPointF(9,12) << QPointF(17,11);
        p.drawPolygon(arrow);
        // 起点圆点
        p.drawEllipse(QPoint(3,21), 2,2);
    });
    // UpTriangle（上三角/买入标记）
    QIcon iconUp = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(80,255,80), 2));
        p.setBrush(QColor(80,255,80));
        QPolygonF tri; tri << QPointF(12,3) << QPointF(3,19) << QPointF(21,19);
        p.drawPolygon(tri);
        // 中间的 + 号
        p.setPen(QPen(Qt::darkGreen, 2));
        p.drawLine(12,8, 12,15);
        p.drawLine(8,11, 16,11);
    });
    // DownTriangle（下三角/卖出标记）
    QIcon iconDown = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 2));
        p.setBrush(QColor(255,80,80));
        QPolygonF tri; tri << QPointF(12,21) << QPointF(3,5) << QPointF(21,5);
        p.drawPolygon(tri);
        // 中间的 - 号
        p.setPen(QPen(Qt::darkRed, 2));
        p.drawLine(8,13, 16,13);
    });
    // Fixed: Dot + text
    QIcon iconFixedDot = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,200,100), 2));
        p.setBrush(QColor(255,200,100));
        p.drawEllipse(QPoint(6,12), 4, 4);
        p.drawLine(11,12, 21,12);
    });
    // Fixed: Triangle + text
    QIcon iconFixedTri = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,200,255), 2));
        p.setBrush(QColor(100,200,255));
        QPolygonF tri; tri << QPointF(8,4) << QPointF(2,14) << QPointF(14,14);
        p.drawPolygon(tri);
        p.drawLine(15,12, 21,12);
    });
    // Delete
    QIcon iconDelete = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 2.5));
        p.drawLine(5,5, 19,19); p.drawLine(19,5, 5,19);
    });
    // Clear
    QIcon iconClear = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,200), 1.5));
        p.drawRect(5,9, 14,13); p.drawLine(3,9, 21,9);
        p.drawLine(9,9, 9,5); p.drawLine(15,9, 15,5);
        p.drawLine(9,5, 15,5); p.drawLine(8,13, 16,13); p.drawLine(8,17, 16,17);
    });

    QToolBar *tb = new QToolBar(QStringLiteral("画图工具"), parent);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QAction *aNormal = tb->addAction(iconNormal, QStringLiteral("选择"));
    aNormal->setToolTip(QStringLiteral("选择 (Esc)"));
    aNormal->setShortcut(QKeySequence(Qt::Key_Escape));

    QAction *aLine = tb->addAction(iconLine, QStringLiteral("直线"));
    aLine->setToolTip(QStringLiteral("直线 (L)"));
    aLine->setShortcut(QKeySequence(Qt::Key_L));

    QAction *aTrend = tb->addAction(iconTrend, QStringLiteral("射线"));
    aTrend->setToolTip(QStringLiteral("趋势线 (T)"));
    aTrend->setShortcut(QKeySequence(Qt::Key_T));

    QAction *aUp = tb->addAction(iconUp, QStringLiteral("上三角"));
    aUp->setToolTip(QStringLiteral("上三角形 (U)"));
    aUp->setShortcut(QKeySequence(Qt::Key_U));

    QAction *aDown = tb->addAction(iconDown, QStringLiteral("下三角"));
    aDown->setToolTip(QStringLiteral("下三角形 (D)"));
    aDown->setShortcut(QKeySequence(Qt::Key_D));

    tb->addSeparator();  // Fixed tools

    QAction *aFixedDot = tb->addAction(iconFixedDot, QStringLiteral("圆点标签"));
    aFixedDot->setToolTip(QStringLiteral("固定位置圆点+文字 (Ctrl+1)"));
    aFixedDot->setCheckable(true);
    aFixedDot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));

    QAction *aFixedTri = tb->addAction(iconFixedTri, QStringLiteral("三角标签"));
    aFixedTri->setToolTip(QStringLiteral("固定位置三角+文字 (Ctrl+2)"));
    aFixedTri->setCheckable(true);
    aFixedTri->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));

    tb->addSeparator();

    QAction *aDelete = tb->addAction(iconDelete, QStringLiteral("删除"));
    aDelete->setToolTip(QStringLiteral("删除 (Del)"));
    aDelete->setShortcut(QKeySequence(Qt::Key_Delete));

    QAction *aClear = tb->addAction(iconClear, QStringLiteral("清除"));
    aClear->setToolTip(QStringLiteral("清除所有 (Shift+Del)"));
    aClear->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));

    QObject::connect(aNormal, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_None); });
    QObject::connect(aLine,   &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Line); });
    QObject::connect(aTrend,  &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Trend); });
    QObject::connect(aUp,     &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_UpTriangle); });
    QObject::connect(aDown,   &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_DownTriangle); });
    QObject::connect(aFixedDot, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_FixedDot); });
    QObject::connect(aFixedTri, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_FixedTriangle); });
    QObject::connect(aDelete, &QAction::triggered, [k](){ k->deleteSelectedShape(); });
    QObject::connect(aClear,  &QAction::triggered, [k](){ k->clearShapes(); });

    return tb;
}
