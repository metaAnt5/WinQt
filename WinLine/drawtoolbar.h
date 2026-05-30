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

    // Normal: 鼠标指针（退出画图模式）
    QIcon iconNormal = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 1.5));
        p.drawLine(4,4, 4,20);
        p.drawLine(4,4, 16,12);
        p.drawLine(4,12, 12,16);
        p.drawLine(16,12, 20,20);
    });

    // Line: 一条贯穿的斜线，两端带小点表示无限延伸
    QIcon iconLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,50), 2));
        p.drawLine(3,21, 21,3);
        p.setBrush(QColor(200,200,50));
        p.drawEllipse(QPoint(3,21), 2,2);
        p.drawEllipse(QPoint(21,3), 2,2);
    });

    // Trend: 射线 —— 左侧圆点起点，右侧延伸
    QIcon iconTrend = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,200,255), 2));
        p.drawLine(4,20, 18,6);
        p.drawLine(18,6, 12,6);
        p.drawLine(18,6, 18,12);
        p.setBrush(QColor(100,200,255));
        p.drawEllipse(QPoint(4,20), 2,2);
    });

    // HLine: 水平线
    QIcon iconHLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,255), 2));
        p.drawLine(2,12, 22,12);
        p.drawLine(2,10, 2,14);
        p.drawLine(22,10, 22,14);
    });

    // VLine: 垂直线
    QIcon iconVLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,255), 2));
        p.drawLine(12,2, 12,22);
        p.drawLine(10,2, 14,2);
        p.drawLine(10,22, 14,22);
    });

    // Text: 字母 A
    QIcon iconText = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 2));
        QFont f = p.font(); f.setPixelSize(18); f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0,0,24,24), Qt::AlignCenter, "A");
    });

    // 上箭头
    QIcon iconUp = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,255,100), 2));
        p.setBrush(QColor(100,255,100));
        QPolygonF arrow;
        arrow << QPointF(12,2) << QPointF(4,12) << QPointF(9,12)
              << QPointF(9,22) << QPointF(15,22) << QPointF(15,12)
              << QPointF(20,12);
        p.drawPolygon(arrow);
    });

    // 下箭头
    QIcon iconDown = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,100,100), 2));
        p.setBrush(QColor(255,100,100));
        QPolygonF arrow;
        arrow << QPointF(12,22) << QPointF(4,12) << QPointF(9,12)
              << QPointF(9,2) << QPointF(15,2) << QPointF(15,12)
              << QPointF(20,12);
        p.drawPolygon(arrow);
    });

    // Delete: 红叉
    QIcon iconDelete = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 3));
        p.drawLine(4,4, 20,20);
        p.drawLine(20,4, 4,20);
    });

    // Clear: 垃圾桶
    QIcon iconClear = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,200), 1.5));
        p.drawRect(5,9, 14,13);
        p.drawLine(3,9, 21,9);
        p.drawLine(9,9, 9,5);
        p.drawLine(15,9, 15,5);
        p.drawLine(9,5, 15,5);
        p.drawLine(8,13, 16,13);
        p.drawLine(8,17, 16,17);
    });

    QToolBar *tb = new QToolBar(QStringLiteral("画图工具"), parent);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

    // ============================================================
    // 创建各工具按钮，设置快捷键和 ToolTip
    // ============================================================
    QAction *aNormal = tb->addAction(iconNormal, QStringLiteral("选择"));
    aNormal->setToolTip(QStringLiteral("选择 / 移动 (Esc)"));
    aNormal->setShortcut(QKeySequence(Qt::Key_Escape));

    QAction *aLine = tb->addAction(iconLine, QStringLiteral("直线"));
    aLine->setToolTip(QStringLiteral("直线 — 无限延长 (L)"));
    aLine->setShortcut(QKeySequence(Qt::Key_L));

    QAction *aTrend = tb->addAction(iconTrend, QStringLiteral("射线"));
    aTrend->setToolTip(QStringLiteral("射线 — 趋势线 (T)"));
    aTrend->setShortcut(QKeySequence(Qt::Key_T));

    QAction *aHLine = tb->addAction(iconHLine, QStringLiteral("水平线"));
    aHLine->setToolTip(QStringLiteral("水平线 (H)"));
    aHLine->setShortcut(QKeySequence(Qt::Key_H));

    QAction *aVLine = tb->addAction(iconVLine, QStringLiteral("垂直线"));
    aVLine->setToolTip(QStringLiteral("垂直线 (V)"));
    aVLine->setShortcut(QKeySequence(Qt::Key_V));

    QAction *aText = tb->addAction(iconText, QStringLiteral("文字"));
    aText->setToolTip(QStringLiteral("文字标注 (X)"));
    aText->setShortcut(QKeySequence(Qt::Key_X));

    QAction *aUp = tb->addAction(iconUp, QStringLiteral("买入"));
    aUp->setToolTip(QStringLiteral("上箭头 — 买入标注 (U)"));
    aUp->setShortcut(QKeySequence(Qt::Key_U));

    QAction *aDown = tb->addAction(iconDown, QStringLiteral("卖出"));
    aDown->setToolTip(QStringLiteral("下箭头 — 卖出标注 (D)"));
    aDown->setShortcut(QKeySequence(Qt::Key_D));

    QAction *aDelete = tb->addAction(iconDelete, QStringLiteral("删除"));
    aDelete->setToolTip(QStringLiteral("删除选中图形 (Del)"));
    aDelete->setShortcut(QKeySequence(Qt::Key_Delete));

    QAction *aClear = tb->addAction(iconClear, QStringLiteral("清除"));
    aClear->setToolTip(QStringLiteral("清除所有图形 (Shift+Del)"));
    aClear->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));

    // ============================================================
    // 连接信号
    // ============================================================
    QObject::connect(aNormal, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_None); });
    QObject::connect(aLine,   &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Line); });
    QObject::connect(aTrend,  &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Trend); });
    QObject::connect(aHLine,  &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_HLine); });
    QObject::connect(aVLine,  &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_VLine); });
    QObject::connect(aText,   &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Text); });
    QObject::connect(aUp,     &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureUp); });
    QObject::connect(aDown,   &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureDown); });
    QObject::connect(aDelete, &QAction::triggered, [k](){ k->deleteSelectedShape(); });
    QObject::connect(aClear,  &QAction::triggered, [k](){ k->clearShapes(); });

    return tb;
}
