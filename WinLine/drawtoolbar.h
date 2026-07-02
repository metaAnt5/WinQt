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
//   L       — 水平线（单点创建，只能垂直拖拽）
//   T       — 射线（趋势线）
//   U       — 上三角（单点创建，宽度=K棒宽度）
//   D       — 下三角（单点创建，宽度=K棒宽度）
//   F       — 固定标签
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

    // Normal: 鼠标指针（简化）
    QIcon iconNormal = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(4,4, 4,20);
        p.drawLine(4,4, 16,10);
        p.drawLine(4,12, 12,17);
    });
    // 水平线：一条水平横线
    QIcon iconLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,50), 2.5));
        p.drawLine(2,12, 22,12);
    });
    // 趋势线（射线）：斜线+箭头
    QIcon iconTrend = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(80,180,255), 2.5));
        p.drawLine(3,21, 13,7);
        p.setBrush(QColor(80,180,255));
        QPolygonF arrow;
        arrow << QPointF(13,7) << QPointF(9,12) << QPointF(17,11);
        p.drawPolygon(arrow);
    });
    // 上三角：空心上三角
    QIcon iconUp = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(80,255,80), 2));
        QPolygonF tri; tri << QPointF(12,3) << QPointF(3,19) << QPointF(21,19);
        p.drawPolygon(tri);
    });
    // 下三角：空心下三角
    QIcon iconDown = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 2));
        QPolygonF tri; tri << QPointF(12,21) << QPointF(3,5) << QPointF(21,5);
        p.drawPolygon(tri);
    });
    // 固定标签：实心圆点
    QIcon iconFixed = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,200,100), 2));
        p.setBrush(QColor(255,200,100));
        p.drawEllipse(QPoint(12,12), 5, 5);
    });
    // 删除：叉号
    QIcon iconDelete = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 2.5));
        p.drawLine(5,5, 19,19); p.drawLine(19,5, 5,19);
    });
    // 清除：垃圾桶简化
    QIcon iconClear = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,200), 1.5));
        p.drawRect(5,9, 14,13); p.drawLine(3,9, 21,9);
        p.drawLine(9,9, 9,5); p.drawLine(15,9, 15,5);
        p.drawLine(9,5, 15,5);
    });

    QToolBar *tb = new QToolBar(QStringLiteral("画图工具"), parent);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QAction *aNormal = tb->addAction(iconNormal, QStringLiteral("选择"));
    aNormal->setToolTip(QStringLiteral("选择 (Esc)"));
    aNormal->setShortcut(QKeySequence(Qt::Key_Escape));

    QAction *aLine = tb->addAction(iconLine, QStringLiteral("水平线"));
    aLine->setToolTip(QStringLiteral("水平线 (L)"));
    aLine->setShortcut(QKeySequence(Qt::Key_L));

    QAction *aTrend = tb->addAction(iconTrend, QStringLiteral("趋势线"));
    aTrend->setToolTip(QStringLiteral("趋势线 (T)"));
    aTrend->setShortcut(QKeySequence(Qt::Key_T));

    QAction *aUp = tb->addAction(iconUp, QStringLiteral("上三角"));
    aUp->setToolTip(QStringLiteral("上三角形 (U)"));
    aUp->setShortcut(QKeySequence(Qt::Key_U));

    QAction *aDown = tb->addAction(iconDown, QStringLiteral("下三角"));
    aDown->setToolTip(QStringLiteral("下三角形 (D)"));
    aDown->setShortcut(QKeySequence(Qt::Key_D));

    tb->addSeparator();

    QAction *aFixed = tb->addAction(iconFixed, QStringLiteral("固定标签"));
    aFixed->setToolTip(QStringLiteral("固定标签 (F)"));
    aFixed->setShortcut(QKeySequence(Qt::Key_F));

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
    QObject::connect(aFixed,  &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Fixed); });
    QObject::connect(aDelete, &QAction::triggered, [k](){ k->deleteSelectedShape(); });
    QObject::connect(aClear,  &QAction::triggered, [k](){ k->clearShapes(); });

    return tb;
}
