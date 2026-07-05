#include "klinewidget.h"
#include "shapedialog.h"
#include "chartconfig.h"
#include "shapes/lineshape.h"
#include "shapes/trendshape.h"
#include "shapes/triangleshape.h"
#include "shapes/fixedshape.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QPainterPath>
#include <QToolTip>
#include <QInputDialog>
#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QColorDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>


KLineWidget::KLineWidget(QWidget *parent)
    : QWidget(parent), m_minPrice(0), m_maxPrice(0), m_scale(1.0), m_candleWidth(6.0), m_gap(2.0), m_startIndex(0), m_panning(false), m_crosshairVisible(false), m_rightPadding(80), m_timeframe(KLineWidget::TF_1m), m_baseMinutes(1)
    , m_toolMode(Tool_None), m_selectedShapeIndex(-1), m_drawing(false), m_draggingEndpoint(0), m_movingShape(false), m_nextShapeId(1)
    , m_lastPrice(0), m_lastOpen(0), m_lastHigh(0), m_lastLow(0), m_lastVolume(0), m_prevClose(0), m_connected(false)
{
    setMinimumSize(600, 500);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_loadingLabel = new QLabel(this);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(0, 0, 0, 180);"
        "  color: #00FFFF;"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  border: 2px solid #00FFFF;"
        "  border-radius: 8px;"
        "  padding: 20px;"
        "}");
    m_loadingLabel->setText(QStringLiteral("Loading..."));
    m_loadingLabel->setVisible(false);

    m_realtimeLabel = new QLabel(this);
    m_realtimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_realtimeLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(0, 0, 0, 200);"
        "  color: #FFFFFF;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  border: 1px solid #555555;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "}");
    m_realtimeLabel->setVisible(false);
}

void KLineWidget::showLoading(const QString &msg) { /* ... same as before ... */ if (!m_loadingLabel) return; m_loadingLabel->setText(msg); int w = qMin(width() * 3 / 4, 400); int h = 100; m_loadingLabel->setGeometry((width() - w) / 2, (height() - h) / 2, w, h); m_loadingLabel->raise(); m_loadingLabel->setVisible(true); }
void KLineWidget::hideLoading() { if (m_loadingLabel) m_loadingLabel->setVisible(false); }

void KLineWidget::showNoData()
{
    if (!m_noDataLabel) {
        m_noDataLabel = new QLabel(this);
        m_noDataLabel->setAlignment(Qt::AlignCenter);
        m_noDataLabel->setStyleSheet(
            "QLabel { background-color: rgba(0, 0, 0, 180); color: #888888; font-size: 20px; font-weight: bold; border: 2px dashed #666666; border-radius: 8px; padding: 20px; }");
        m_noDataLabel->setText(QStringLiteral("暂无数据"));
    }
    int w = qMin(width() * 3 / 4, 300); int h = 80;
    m_noDataLabel->setGeometry((width() - w) / 2, (height() - h) / 2, w, h);
    m_noDataLabel->raise(); m_noDataLabel->setVisible(true);
}
void KLineWidget::hideNoData() { if (m_noDataLabel) m_noDataLabel->setVisible(false); }

void KLineWidget::setData(const QVector<Candle> &data) { setData(data, 1); }
void KLineWidget::setData(const QVector<Candle> &data, int baseMinutes)
{
    m_data = data;
    m_baseMinutes = qMax(1, baseMinutes);
    m_timeframe = static_cast<Timeframe>(baseMinutes);
    if (!m_data.isEmpty()) {
        const Candle &last = m_data.last();
        m_lastPrice = last.close; m_lastOpen = last.open; m_lastHigh = last.high; m_lastLow = last.low; m_lastVolume = last.volume;
        m_prevClose = (m_data.size() >= 2) ? m_data[m_data.size() - 2].close : last.close;
        updateRealtimeLabel();
    } else { m_lastPrice = 0; m_realtimeLabel->setVisible(false); }
    m_startIndex = qMax(0, m_data.size() - visibleCount());
    updateRange(); calculateMovingAverages();
    if (m_data.isEmpty()) { hideLoading(); showNoData(); } else { hideNoData(); }
    if (!m_symbol.isEmpty() && !m_data.isEmpty()) loadShapes();
    ChartConfig::setLayout(mainChartRect(), totalPer(), m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
    emit dataAggregated(m_data); emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
    update();
}

void KLineWidget::updateRange()
{
    if (m_data.isEmpty()) { m_minPrice = 0; m_maxPrice = 0; return; }
    int count = visibleCount(); int end = qMin(m_startIndex + count, m_data.size());
    if (end <= m_startIndex) { m_minPrice = m_data.first().low; m_maxPrice = m_data.first().high; return; }
    m_minPrice = m_data.at(m_startIndex).low; m_maxPrice = m_data.at(m_startIndex).high;
    for (int i = m_startIndex; i < end; ++i) {
        const Candle &c = m_data.at(i);
        if (c.low < m_minPrice) m_minPrice = c.low;
        if (c.high > m_maxPrice) m_maxPrice = c.high;
    }
    double pad = (m_maxPrice - m_minPrice) * 0.06; if (pad <= 0) pad = 1.0;
    m_minPrice -= pad; m_maxPrice += pad;
    if (m_minPrice == m_maxPrice) { m_minPrice -= 1; m_maxPrice += 1; }
    if (m_ma5.isEmpty() && !m_data.isEmpty()) calculateMovingAverages();
}

int KLineWidget::visibleCount() const { int w = mainChartRect().width(); double tp = (m_candleWidth * m_scale) + m_gap; if (tp <= 0) return 1; return qMax(1, int(w / tp)); }
void KLineWidget::ensureStartIndexVisible() { if (m_startIndex < 0) m_startIndex = 0; int maxStart = qMax(0, m_data.size() - visibleCount()); if (m_startIndex > maxStart) m_startIndex = maxStart; }

void KLineWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event); ensureStartIndexVisible(); updateRange();
    if (m_crosshairVisible && !m_data.isEmpty()) snapCrosshairTo(m_crosshairPos);
    emit dataAggregated(m_data); emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
}

// =====================================================================
// mousePressEvent - 使用多态 hitTest
// =====================================================================
void KLineWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) { setToolMode(Tool_None); return; }

    // Fixed tool
    if (m_toolMode == Tool_Fixed) {
        QPointF norm = screenToNorm(event->pos());
        auto s = QSharedPointer<FixedShape>::create();
        s->x1 = norm.x(); s->y1 = norm.y();
        s->text = QStringLiteral("Note"); s->color = QColor(255, 200, 100);
        addShape(s); saveShapes();
        m_toolMode = Tool_None; setCursor(Qt::ArrowCursor);
        return;
    }

    // Normal mode
    if (m_toolMode == Tool_None && event->button() == Qt::LeftButton) {
        QPointF pt = event->pos();
        int clickedIdx = -1, dragEndpoint = 0;
        double minDist = 1e9;

        // hit-test all shapes using polymorphic hitTest
        for (int i = 0; i < m_shapes.size(); ++i) {
            auto &s = m_shapes[i];
            int hit = s->hitTest(pt, this);
            if (hit > 0) { // endpoint hit
                double d = 1e9;
                QPointF sp1, sp2;
                if (s->followsKLine()) {
                    dataCoordToScreen(s->x1, s->y1, sp1);
                    dataCoordToScreen(s->x2, s->y2, sp2);
                } else {
                    QRect cr = mainChartRect();
                    sp1 = QPointF(cr.left() + s->x1 * cr.width(), cr.top() + s->y1 * cr.height());
                    sp2 = sp1;
                }
                double d1 = (sp1 - pt).manhattanLength(), d2 = (sp2 - pt).manhattanLength();
                if (d1 < minDist) { minDist = d1; clickedIdx = i; dragEndpoint = 1; }
                if (d2 < minDist) { minDist = d2; clickedIdx = i; dragEndpoint = 2; }
            } else if (hit == -1) { // body hit
                if (minDist > 5 && (!s->canDrag() || minDist > 10)) {
                    minDist = 5; clickedIdx = i; dragEndpoint = 0;
                }
            }
        }

        // 子 shape 重定向到父 shape：子 shape 不可独立选中/拖动
        if (clickedIdx >= 0 && m_shapes[clickedIdx]->ownerShapeId > 0) {
            int parentId = m_shapes[clickedIdx]->ownerShapeId;
            for (int i = 0; i < m_shapes.size(); ++i) {
                if (m_shapes[i]->id == parentId) {
                    clickedIdx = i;
                    dragEndpoint = 0;
                    break;
                }
            }
        }

        bool bodyHit = (clickedIdx >= 0);

        // 端点命中且可拖动
        if (clickedIdx >= 0 && dragEndpoint > 0 && m_shapes[clickedIdx]->canDrag()) {
            m_selectedShapeIndex = clickedIdx;
            m_draggingEndpoint = dragEndpoint;
            m_lastMousePos = event->pos();
            update(); emit shapeSelected(m_selectedShapeIndex);
            return;
        }

        // 体命中 — 选中 + 标记可移动（如果 shape 可拖动）
        if (bodyHit) {
            m_selectedShapeIndex = clickedIdx;
            m_draggingEndpoint = 0;
            // ★ 只有当 shape 可拖动时才标记 m_movingShape=true，以便 release 时保存
            m_movingShape = m_shapes[clickedIdx]->canDrag();
            m_lastMousePos = event->pos();
            // ★ body 命中时不应启动 panning（shape 优先）
            m_panning = false;
            setCursor(Qt::ArrowCursor);
            update(); emit shapeSelected(m_selectedShapeIndex);
            return;  // ★ 直接 return，不执行下面的 panning 代码
        }

        // 无命中则清空选中 + 启动 panning
        if (!bodyHit) {
            m_selectedShapeIndex = -1;
            m_panning = true; m_draggingEndpoint = 0;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            update(); emit shapeSelected(m_selectedShapeIndex);
        } else {
            // 实际上 bodyHit 已提前 return，不会到这里
            m_selectedShapeIndex = -1;
        }
        return;
    }

    // Drawing mode
    if (m_toolMode != Tool_None && event->button() == Qt::LeftButton) {
        if (m_data.isEmpty()) { m_toolMode = Tool_None; setCursor(Qt::ArrowCursor); update(); return; }
        setCursor(Qt::CrossCursor);
        QPointF pt = event->pos();

        // 如果是绘制中的第二个点击（如 Trend 的终点）
        if (m_drawing && m_selectedShapeIndex >= 0) {
            auto &s = m_shapes[m_selectedShapeIndex];
            screenToDataCoord(pt, s->x2, s->y2);
            m_drawing = false; m_draggingEndpoint = 0; m_movingShape = false;
            m_selectedShapeIndex = -1;
            saveShapes(); setToolMode(Tool_None);
            update();
            return;
        }

        // 非绘制中：尝试命中已有 shape（拖动端点或整体）
        // ★ 绘制中（m_drawing=true）时跳过 hit-test，避免干扰趋势线第二下点击
        if (m_drawing) return;
        int clickedIdx = -1, dragEndpoint = 0;
        double minDist = 1e9;
        for (int i = 0; i < m_shapes.size(); ++i) {
            auto &s = m_shapes[i];
            if (!m_drawing && !s->canDrag()) continue;
            int hit = s->hitTest(pt, this);
            if (hit > 0) {
                double d = 1e9;
                if (minDist > 10) { minDist = 10; clickedIdx = i; dragEndpoint = hit; }
            } else if (hit == -1 && s->canDrag()) {
                if (minDist > 5) { minDist = 5; clickedIdx = i; dragEndpoint = 0; }
            }
        }
        if (clickedIdx >= 0) {
            m_selectedShapeIndex = clickedIdx; m_draggingEndpoint = dragEndpoint;
            m_movingShape = (dragEndpoint == 0); m_drawing = false;
            m_lastMousePos = event->pos(); update();
            return;
        }

        // 创建新 shape
        QSharedPointer<Shape> ns;
        switch (m_toolMode) {
        case Tool_Line:
            ns = QSharedPointer<LineShape>::create();
            ns->color = QColor(200, 200, 50);
            break;
        case Tool_Trend:
            ns = QSharedPointer<TrendShape>::create();
            ns->color = QColor(100, 200, 255);
            break;
        case Tool_UpTriangle:
            ns = QSharedPointer<TriangleShape>::create();
            qSharedPointerCast<TriangleShape>(ns)->up = true;
            ns->color = QColor(100, 255, 100);
            break;
        case Tool_DownTriangle:
            ns = QSharedPointer<TriangleShape>::create();
            qSharedPointerCast<TriangleShape>(ns)->up = false;
            ns->color = QColor(255, 100, 100);
            break;
        default: return;
        }

        screenToDataCoord(pt, ns->x1, ns->y1);
        ns->x2 = ns->x1; ns->y2 = ns->y1;
        ns->id = m_nextShapeId++;
        ns->name = QString("shape_%1").arg(ns->id);

        if (ns->type() == ShapeType::Trend) {
            // 趋势线需等第二下点击
            m_drawing = true; m_draggingEndpoint = 2;
            m_shapes.append(ns);
            m_selectedShapeIndex = m_shapes.size() - 1;
            m_lastMousePos = event->pos(); update(); setFocus();
            return;
        }

        if (ns->type() == ShapeType::Line) {
            // 水平线一次完成
            ns->y2 = ns->y1;
            ns->x2 = qMax(0.0, double(m_data.size() - 1));
            addShape(ns); saveShapes();
            update(); setFocus();
            return;
        }

        // 三角形一次完成
        addShape(ns);
        m_selectedShapeIndex = m_shapes.size() - 1;
        m_lastMousePos = event->pos(); update(); setFocus();
        return;
    }

    // Fallback panning
    if (event->button() == Qt::LeftButton) {
        m_panning = true; m_lastMousePos = event->pos(); setCursor(Qt::ClosedHandCursor);
    }
}

// =====================================================================
// mouseMoveEvent - 使用多态 moveBy / dragEndpoint
// =====================================================================
void KLineWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Normal mode: drag endpoints or move shapes
    if (m_toolMode == Tool_None && m_selectedShapeIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        auto &s = m_shapes[m_selectedShapeIndex];
        if (!s->canDrag()) return;

        if (m_draggingEndpoint > 0) {
            s->dragEndpoint(m_draggingEndpoint, event->pos(), this);
            update(); return;
        }

        // 移动整体
        QPointF delta = QPointF(event->pos()) - QPointF(m_lastMousePos);
        if (delta.manhattanLength() > 3) {
            s->moveBy(delta, this);
            m_lastMousePos = event->pos();
            update(); return;
        }
    }

    // Drawing mode
    if (m_toolMode != Tool_None && m_selectedShapeIndex >= 0) {
        auto &s = m_shapes[m_selectedShapeIndex];
        if (m_draggingEndpoint > 0) {
            s->dragEndpoint(m_draggingEndpoint, event->pos(), this);
            update(); return;
        }
        if (m_drawing) {
            s->dragEndpoint(2, event->pos(), this);
            update(); return;
        }
    }

    // Panning
    if (m_panning && !m_data.isEmpty()) {
        int dx = event->pos().x() - m_lastMousePos.x();
        double tp = (m_candleWidth * m_scale) + m_gap;
        if (tp > 0) {
            int dIdx = int(-dx / tp);
            if (dIdx != 0) {
                m_startIndex += dIdx; ensureStartIndexVisible(); updateRange();
                update(); m_lastMousePos = event->pos();
                ChartConfig::setLayout(mainChartRect(), tp, m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
                emit viewportChanged(m_startIndex, visibleCount());
                emit layoutChanged(m_startIndex, visibleCount(), tp, candleBodyWidth(), mainChartRect());
                if (m_crosshairVisible) snapCrosshairTo(m_crosshairPos);
            }
        }
    }

    if (m_crosshairVisible) { snapCrosshairTo(event->pos()); update(); }

    // Tooltip
    if (!m_data.isEmpty()) {
        QRect mr = mainChartRect(); int contentLeft = mr.left();
        double tp = (m_candleWidth * m_scale) + m_gap;
        if (tp > 0) {
            int relX = event->pos().x() - contentLeft;
            int relIdx = int(double(relX) / tp + 0.5);
            int idx = m_startIndex + relIdx;
            if (idx >= 0 && idx < m_data.size()) {
                const Candle &c = m_data.at(idx);
                double x = contentLeft + relIdx * tp;
                double bodyW = m_candleWidth * m_scale;
                double bodyLeft = x + (tp - bodyW) / 2.0;
                double bodyRight = bodyLeft + bodyW;
                QRect mainRect(contentLeft, 10, mr.width(), mr.height());
                auto priceToY2 = [&](double p){ double r2 = (p - m_minPrice) / (m_maxPrice - m_minPrice); return mainRect.bottom() - r2 * mainRect.height(); };
                double yOpen = priceToY2(c.open), yClose = priceToY2(c.close);
                QPoint pos = event->pos();
                if (pos.x() >= int(bodyLeft) && pos.x() <= int(bodyRight) && pos.y() >= int(qMin(yOpen, yClose))-3 && pos.y() <= int(qMax(yOpen, yClose))+3) {
                    QString timeStr = (m_timeframe == TF_DAILY) ? c.date.toString("yyyy-MM-dd") : c.date.toString("yyyy-MM-dd HH:mm");
                    QToolTip::showText(event->globalPosition().toPoint(),
                        QString("<b>%1</b><br>Open: %2<br>High: %3<br>Low: %4<br>Close: %5<br>Vol: %6")
                            .arg(timeStr).arg(c.open,0,'f',2).arg(c.high,0,'f',2).arg(c.low,0,'f',2).arg(c.close,0,'f',2).arg(c.volume), this);
                } else QToolTip::hideText();
            }
        }
    }
}

void KLineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_toolMode == Tool_None) {
        if (m_panning) { m_panning = false; setCursor(Qt::ArrowCursor); }
        if (m_draggingEndpoint > 0 || m_movingShape) saveShapes();
        m_draggingEndpoint = 0; m_movingShape = false; update(); return;
    }
    m_draggingEndpoint = 0; m_movingShape = false;
    if (m_panning) { m_panning = false; setCursor(Qt::ArrowCursor); }
    update();
}

// =====================================================================
// mouseDoubleClickEvent - 使用多态 hitTest
// =====================================================================
void KLineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_data.isEmpty()) { m_crosshairVisible = false; update(); return; }
    QPointF pt = event->pos();
    int hitIndex = -1;
    for (int i = 0; i < m_shapes.size(); ++i) {
        if (m_shapes[i]->hitTest(pt, this) != -2) { hitIndex = i; break; }
    }
    if (hitIndex >= 0) {
        // 子 shape 重定向到父 shape
        if (m_shapes[hitIndex]->ownerShapeId > 0) {
            int parentId = m_shapes[hitIndex]->ownerShapeId;
            for (int i = 0; i < m_shapes.size(); ++i) {
                if (m_shapes[i]->id == parentId) {
                    hitIndex = i;
                    break;
                }
            }
        }
        m_selectedShapeIndex = hitIndex; m_draggingEndpoint = 0;
        emit shapeSelected(hitIndex); emit shapeDoubleClicked(hitIndex);
    } else {
        m_crosshairVisible = !m_crosshairVisible;
        if (m_crosshairVisible) {
            m_crosshairPos = event->pos();
            QRect mainRect = mainChartRect();
            double priceRange = m_maxPrice - m_minPrice;
            if (priceRange != 0) {
                double totalPer = (m_candleWidth * m_scale) + m_gap;
                int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / (totalPer > 0 ? totalPer : 1.0) + 0.5);
                idx = qBound(0, idx, m_data.size()-1);
                double xCenter = mainRect.left() + (idx - m_startIndex) * totalPer + totalPer / 2.0;
                m_crosshairPos.setX(int(xCenter + 0.5)); snapCrosshairTo(m_crosshairPos);
            }
            update();
        } else {
            emit crosshairIndexChanged(-1); emit crosshairPriceChanged(0.0, -1); emit crosshairScreenXChanged(-1);
            update();
        }
    }
}

void KLineWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) { if (m_toolMode == Tool_None) deleteSelectedShape(); }
    else if (event->key() == Qt::Key_Escape) { m_selectedShapeIndex = -1; update(); }
    else QWidget::keyPressEvent(event);
}

// =====================================================================
// paintEvent - 使用多态 draw
// =====================================================================
void KLineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), QColor(10, 10, 10));

    QRect mainRect = mainChartRect();
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    double totalPer = (m_candleWidth * m_scale) + m_gap;

    // Price axis
    {
        const int marginLeft = 10, priceAxisWidth = 40, marginTop = 10, marginBottom = 20;
        QRect pmr(marginLeft + priceAxisWidth, marginTop, width() - marginLeft - priceAxisWidth - 10 - m_rightPadding, height() - marginTop - marginBottom);
        if (priceRange > 0) {
            double step = priceRange / 10.0; if (step <= 0) step = 1.0;
            p.setFont(QFont("Arial", 8));
            for (double pr = m_minPrice; pr <= m_maxPrice; pr += step) {
                double ratio = (pr - m_minPrice) / priceRange;
                int y = pmr.bottom() - ratio * pmr.height();
                p.setPen(QPen(QColor(180,180,180),2)); p.drawLine(pmr.left()-5, y, pmr.left()-2, y);
                p.setPen(QPen(QColor(200,200,200),1));
                QString ps = QString::number(pr, 'f', 2);
                QFontMetrics fm(p.font());
                p.drawText(marginLeft + priceAxisWidth - fm.horizontalAdvance(ps) - 8, y-4, ps);
            }
        }
    }

    // Time axis
    {
        const int marginBottom = 20;
        if (totalPer > 0 && !m_data.isEmpty()) {
            int visCount = visibleCount();
            int tickInterval = qMax(1, visCount / 20);
            p.setFont(QFont("Arial", 8));
            for (int i = m_startIndex; i < m_startIndex + visCount && i < m_data.size(); ++i) {
                if ((i - m_startIndex) % tickInterval == 0) {
                    double x = mainRect.left() + (i - m_startIndex) * totalPer + totalPer / 2.0;
                    int y = height() - marginBottom;
                    p.setPen(QPen(QColor(100,200,200),2)); p.drawLine(int(x), y+2, int(x), y+5);
                    p.setPen(QPen(QColor(150,220,220),1));
                    QString ts = (m_timeframe == TF_DAILY) ? m_data[i].date.toString("yyyy-MM-dd") : m_data[i].date.toString("yyyy-MM-dd HH:mm");
                    QFontMetrics fm(p.font());
                    p.drawText(int(x) - fm.horizontalAdvance(ts)/2, y+15, ts);
                }
            }
        }
    }

    ChartConfig::setLayout(mainRect, totalPer, m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
    auto priceToY = [&](double price){ double r = (price - m_minPrice) / priceRange; return mainRect.bottom() - r * mainRect.height(); };

    // Draw candlesticks
    for (int i = m_startIndex; i < m_startIndex + visibleCount() && i < m_data.size(); ++i) {
        const Candle &c = m_data.at(i);
        double x = mainRect.left() + (i - m_startIndex) * totalPer;
        double ox = x + totalPer / 2.0;
        double yHigh = priceToY(c.high), yLow = priceToY(c.low);
        double yOpen = priceToY(c.open), yClose = priceToY(c.close);
        bool rise = c.close >= c.open;
        QColor clr = rise ? QColor(220,20,60) : QColor(0,200,0);
        p.setPen(QPen(clr)); p.setBrush(clr);
        p.drawLine(QPointF(ox, yHigh), QPointF(ox, yLow));
        double bodyW = m_candleWidth * m_scale;
        QRectF bodyRect(x + (totalPer - bodyW) / 2.0, std::min(yOpen, yClose), bodyW, qMax(1.0, fabs(yClose - yOpen)));
        p.fillRect(bodyRect, clr); p.drawRect(bodyRect);
    }

    // Draw user shapes via polymorphic draw（仅显示匹配当前品种周期的 shape）
    for (int i = 0; i < m_shapes.size(); ++i) {
        const auto &s = m_shapes[i];
        // 运行中 shape 自动补齐 symbol/tf
        if (s->symbol.isEmpty()) s->symbol = m_symbol;
        if (s->timeframe == 0) s->timeframe = m_baseMinutes;
        // 品种不匹配跳过（Fixed/Text 全局固定显示不受限）
        if (s->attachment() != Attachment::Fixed && s->followsKLine()) {
            if (s->symbol != m_symbol && !s->symbol.isEmpty()) continue;
            if (s->timeframe != m_baseMinutes && s->timeframe != 0) continue;
        }
        s->draw(p, this, i == m_selectedShapeIndex);
    }

    // Moving averages
    drawMovingAverages(p);

    // Real-time price line
    if (m_lastPrice > 0 && priceRange > 0) {
        double ratio = (m_lastPrice - m_minPrice) / priceRange;
        int y = mainRect.bottom() - int(ratio * mainRect.height());
        y = qBound(mainRect.top(), y, mainRect.bottom());
        QPen pricePen(QColor(255,165,0,200), 2, Qt::DashLine);
        p.setPen(pricePen); p.drawLine(mainRect.left(), y, mainRect.right(), y);
        p.setFont(QFont("Arial", 10, QFont::Bold));
        QString ps = QString::number(m_lastPrice, 'f', 2);
        QFontMetrics fm(p.font());
        int tw = fm.horizontalAdvance(ps) + 8, th = fm.height() + 2;
        QRectF lr(mainRect.right() + 2, y - th/2 - 1, tw, th);
        p.setBrush(QColor(255,165,0,180)); p.setPen(Qt::NoPen); p.drawRoundedRect(lr, 3, 3);
        p.setPen(Qt::white); p.drawText(lr, Qt::AlignCenter, ps);
    }

    // Crosshair
    if (m_crosshairVisible) {
        p.setPen(QPen(QColor(0,255,255), 2, Qt::SolidLine));
        p.drawLine(m_crosshairPos.x(), mainRect.top(), m_crosshairPos.x(), mainRect.bottom());
        p.drawLine(mainRect.left(), m_crosshairPos.y(), mainRect.right(), m_crosshairPos.y());
        p.setPen(QPen(QColor(255,255,0),2)); p.setBrush(QColor(255,255,0)); p.drawEllipse(m_crosshairPos, 4, 4);
        if (priceRange != 0) {
            double ratio = double(mainRect.bottom() - m_crosshairPos.y()) / double(mainRect.height());
            double pr = m_minPrice + ratio * priceRange;
            int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / (totalPer > 0 ? totalPer : 1.0) + 0.5);
            idx = qBound(0, idx, m_data.size()-1);
            p.setFont(QFont("Arial", 11, QFont::Bold)); p.setPen(QPen(QColor(255,255,0),1));
            QString timeStr = (idx >= 0 && idx < m_data.size()) ? m_data[idx].date.toString("yyyy-MM-dd HH:mm") : QString();
            QFontMetrics fm2(p.font());
            p.drawText(m_crosshairPos.x() - fm2.horizontalAdvance(timeStr)/2, mainRect.bottom() + 15, timeStr);
            p.drawText(mainRect.right() + 5, m_crosshairPos.y() + fm2.height()/2, QString::number(pr, 'f', 2));
        }
    }

    // Crosshair info box
    if (m_crosshairVisible) {
        int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / (totalPer > 0 ? totalPer : 1.0) + 0.5);
        idx = qBound(0, idx, m_data.size()-1);
        if (priceRange > 0) {
            double ratio = double(mainRect.bottom() - m_crosshairPos.y()) / double(mainRect.height());
            double pr = m_minPrice + ratio * priceRange;
            drawCrosshairInfoBox(p, idx, pr);
        }
    }
}

void KLineWidget::setTimeframe(Timeframe tf)
{
    m_timeframe = tf; m_baseMinutes = int(tf);
    m_startIndex = qMax(0, m_data.size() - visibleCount());
    updateRange(); calculateMovingAverages();
    ChartConfig::setLayout(mainChartRect(), totalPer(), m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
    emit dataAggregated(m_data); emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
    update();
    if (m_crosshairVisible && !m_data.isEmpty()) snapCrosshairTo(m_crosshairPos);
}

void KLineWidget::setToolMode(ToolMode m)
{
    if (m_drawing && m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_shapes.size())
        m_shapes.removeAt(m_selectedShapeIndex);
    m_toolMode = m; m_selectedShapeIndex = -1; m_drawing = false; m_draggingEndpoint = 0; m_movingShape = false;
    if (m_toolMode == Tool_None) setCursor(Qt::ArrowCursor);
    else if (m_toolMode == Tool_Fixed) setCursor(Qt::PointingHandCursor);
    else setCursor(Qt::CrossCursor);
    update();
}

// =====================================================================
// deleteSelectedShape - 支持子 shape 删除
// =====================================================================
void KLineWidget::deleteSelectedShape()
{
    if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_shapes.size()) {
        int parentId = m_shapes[m_selectedShapeIndex]->id;
        m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
            [parentId](const QSharedPointer<Shape> &s) {
                return s->id == parentId || s->ownerShapeId == parentId;
            }), m_shapes.end());
        saveShapes();
    }
    m_selectedShapeIndex = -1; update();
}

void KLineWidget::clearShapes() { m_shapes.clear(); m_selectedShapeIndex = -1; saveShapes(); update(); }

void KLineWidget::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    double factor = (delta > 0) ? 1.1 : 0.9;
    double oldScale = m_scale;
    m_scale *= factor; m_scale = qBound(0.4, m_scale, 5.0);
    double totalPerOld = (m_candleWidth * oldScale) + m_gap;
    double totalPerNew = (m_candleWidth * m_scale) + m_gap;
    if (totalPerOld > 0 && totalPerNew > 0 && !m_data.isEmpty()) {
        int mouseX = int(event->position().x());
        QRect mr = mainChartRect(); int contentLeft = mr.left(), contentWidth = qMax(1, mr.width());
        double relative = qBound(0.0, (mouseX - contentLeft) / double(contentWidth), 1.0);
        int visibleOld = qMax(1, int(contentWidth / totalPerOld));
        int focusIndex = m_startIndex + int(relative * visibleOld);
        double focusOffset = (mouseX - contentLeft) - (focusIndex - m_startIndex) * totalPerOld;
        int computedStart = int(focusIndex - (mouseX - contentLeft - focusOffset) / totalPerNew);
        m_startIndex = computedStart; ensureStartIndexVisible(); updateRange();
        ChartConfig::setLayout(mainChartRect(), totalPerNew, m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
        emit viewportChanged(m_startIndex, visibleCount());
        emit layoutChanged(m_startIndex, visibleCount(), totalPerNew, candleBodyWidth(), mainChartRect());
        update();
        if (m_crosshairVisible) snapCrosshairTo(m_crosshairPos);
    }
}

// ─── 坐标转换 ───

void KLineWidget::screenToDataCoord(const QPointF &screenPt, double &candleIdx, double &price) const
{
    if (m_data.isEmpty()) { candleIdx = 0; price = 0.0; return; }
    QRect mr = mainChartRect(); double tp = (m_candleWidth * m_scale) + m_gap;
    if (tp > 0 && mr.width() > 0) {
        double relX = screenPt.x() - mr.left();
        int relIdx = int(relX / tp + 0.5);
        candleIdx = qBound(0.0, double(m_startIndex + relIdx), double(m_data.size() - 1));
    } else { candleIdx = m_startIndex; }
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    if (mr.height() > 0) {
        double ratio = double(mr.bottom() - screenPt.y()) / double(mr.height());
        price = m_minPrice + ratio * priceRange;
    } else { price = m_minPrice; }
}

void KLineWidget::dataCoordToScreen(double candleIdx, double price, QPointF &screenPt) const
{
    QRect mr = mainChartRect(); double tp = totalPer();
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    double xCenter = mr.left() + (candleIdx - m_startIndex) * tp + tp / 2.0;
    double ratio = (price - m_minPrice) / priceRange;
    double y = mr.bottom() - ratio * mr.height();
    screenPt = QPointF(xCenter, y);
}

double KLineWidget::candleBodyWidth() const { return m_candleWidth * m_scale; }

// ─── MA ───

void KLineWidget::calculateMovingAverages()
{
    m_ma5.clear(); m_ma10.clear(); m_ma20.clear(); m_ma60.clear();
    if (m_data.isEmpty()) return;
    for (int i = 0; i < m_data.size(); ++i) {
        int s = qMax(0,i-4); double sum=0; for(int j=s;j<=i;++j) sum+=m_data[j].close; m_ma5.append(sum/(i-s+1));
    }
    for (int i = 0; i < m_data.size(); ++i) {
        int s = qMax(0,i-9); double sum=0; for(int j=s;j<=i;++j) sum+=m_data[j].close; m_ma10.append(sum/(i-s+1));
    }
    for (int i = 0; i < m_data.size(); ++i) {
        int s = qMax(0,i-19); double sum=0; for(int j=s;j<=i;++j) sum+=m_data[j].close; m_ma20.append(sum/(i-s+1));
    }
    for (int i = 0; i < m_data.size(); ++i) {
        int s = qMax(0,i-59); double sum=0; for(int j=s;j<=i;++j) sum+=m_data[j].close; m_ma60.append(sum/(i-s+1));
    }
}

void KLineWidget::drawMovingAverages(QPainter &p)
{
    if (m_ma5.isEmpty() && !m_data.isEmpty()) calculateMovingAverages();
    QRect mr = mainChartRect(); double pr = m_maxPrice - m_minPrice;
    if (pr <= 0) return;
    auto p2y = [&](double price){ double r = (price-m_minPrice)/pr; return mr.bottom()-r*mr.height(); };
    double tp = totalPer(); int vc = visibleCount(), cl = mr.left();
    auto drawMA = [&](const QVector<double> &ma, QColor clr) {
        if (ma.isEmpty()) return;
        p.setPen(QPen(clr, 1)); QPointF lastPt;
        for (int i = 0; i < vc && m_startIndex+i < m_data.size(); ++i) {
            int idx = m_startIndex+i;
            if (idx >= 0 && idx < ma.size()) {
                QPointF pt(cl + i*tp + tp/2, p2y(ma[idx]));
                if (i>0) p.drawLine(lastPt, pt); lastPt = pt;
            }
        }
    };
    if (m_showMA5) drawMA(m_ma5, QColor(255,255,0));
    if (m_showMA10) drawMA(m_ma10, QColor(255,255,255));
    if (m_showMA20) drawMA(m_ma20, QColor(0,255,255));
    if (m_showMA60) drawMA(m_ma60, QColor(255,0,255));
}

// ─── Layout ───

QRect KLineWidget::mainChartRect() const {
    const int ml=5, mt=10, mb=20, paw=20;
    int rp = m_rightPadding;
    double tp = (m_candleWidth * m_scale) + m_gap;
    if (tp > 0) rp += int(3 * tp);
    return QRect(ml+paw, mt, width()-ml-paw-10-rp, height()-mt-mb);
}
double KLineWidget::totalPer() const { return (m_candleWidth * m_scale) + m_gap; }
int KLineWidget::candleCenterXForIndex(int index) const {
    QRect r = mainChartRect(); double tp = totalPer();
    return int(r.left() + (index - m_startIndex) * tp + tp / 2.0 + 0.5);
}
int KLineWidget::indexForScreenX(int screenX) const {
    QRect r = mainChartRect(); double tp = totalPer();
    int rel = int((screenX - r.left()) / (tp > 0 ? tp : 1.0) + 0.5);
    return qBound(0, m_startIndex + rel, m_data.size()-1);
}

void KLineWidget::snapCrosshairTo(const QPointF &pos)
{
    if (m_data.isEmpty()) return;
    QRect r = mainChartRect(); double tp = totalPer();
    double y = pos.y(); int idx = m_startIndex;
    if (tp > 0 && r.width() > 0) {
        int rel = int((pos.x() - r.left()) / tp + 0.5);
        idx = qBound(0, m_startIndex + rel, m_data.size()-1);
        m_crosshairPos.setX(int(r.left() + (idx - m_startIndex) * tp + tp / 2.0 + 0.5));
    }
    m_crosshairPos.setY(int(y));
    double pr = m_maxPrice - m_minPrice; if (qFuzzyCompare(pr, 0.0)) pr = 1.0;
    double ratio = double(r.bottom() - m_crosshairPos.y()) / double(r.height());
    emit crosshairIndexChanged(idx); emit crosshairPriceChanged(m_minPrice + ratio * pr, idx);
    emit crosshairScreenXChanged(candleCenterXForIndex(idx));
}

// ─── Realtime ───

void KLineWidget::updateRealtimeCandle(const Candle &c)
{
    bool newBar = false;
    if (m_data.isEmpty()) { m_data.append(c); newBar = true; }
    else {
        const Candle &last = m_data.last();
        qint64 ds = qAbs(last.date.secsTo(c.date));
        int tol = (m_baseMinutes >= 60) ? 120 : 30;
        if (ds <= tol) { m_data.last() = c; newBar = false; }
        else if (c.date > last.date) { m_data.append(c); newBar = true; }
        else return;
    }
    if (m_data.size() >= 2) m_prevClose = m_data[m_data.size() - 2].close;
    m_lastPrice = c.close; m_lastOpen = c.open; m_lastHigh = c.high; m_lastLow = c.low; m_lastVolume = c.volume;
    updateRealtimeLabel();
    { int vc = visibleCount(); if (m_startIndex + vc < m_data.size()) m_startIndex = qMax(0, m_data.size() - vc); }
    updateRange(); calculateMovingAverages();
    emit candleUpdated(c, newBar);
    ChartConfig::setLayout(mainChartRect(), totalPer(), m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);
    emit dataAggregated(m_data); emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
    update();
}

int KLineWidget::findCandleIndexByTime(const QDateTime &time) const
{
    if (m_data.isEmpty() || !time.isValid()) return -1;
    int lo=0, hi=m_data.size()-1;
    while (lo <= hi) {
        int mid = (lo+hi)/2;
        if (m_data[mid].date == time) return mid;
        if (m_data[mid].date < time) lo = mid+1; else hi = mid-1;
    }
    if (hi < 0) return 0; if (lo >= m_data.size()) return m_data.size()-1;
    return (qAbs(m_data[lo].date.msecsTo(time)) < qAbs(m_data[hi].date.msecsTo(time))) ? lo : hi;
}

void KLineWidget::setSymbol(const QString &s) { m_symbol = s; updateRealtimeLabel(); }

QPointF KLineWidget::screenToNorm(const QPoint &screenPt) const
{
    QRect cr = mainChartRect(); if (cr.width() <= 0 || cr.height() <= 0) return {0.5,0.5};
    return {qBound(0.0, double(screenPt.x()-cr.left())/cr.width(), 1.0), qBound(0.0, double(screenPt.y()-cr.top())/cr.height(), 1.0)};
}
QPoint KLineWidget::normToScreen(double normX, double normY) const
{ QRect cr = mainChartRect(); return {cr.left()+int(normX*cr.width()), cr.top()+int(normY*cr.height())}; }
void KLineWidget::dataToNorm(int candleIdx, double price, double &normX, double &normY) const
{
    if (m_data.isEmpty() || mainChartRect().width() <= 0) { normX=0.5; normY=0.5; return; }
    QRect cr = mainChartRect(); double pr = m_maxPrice-m_minPrice;
    if (pr <= 0) { normX=0.5; normY=0.5; return; }
    double tp = totalPer();
    normX = qBound(0.0, (tp>0 ? (candleIdx-m_startIndex)*tp+tp/2.0 : 0.5)/cr.width(), 1.0);
    normY = qBound(0.0, 1.0-(price-m_minPrice)/pr, 1.0);
}
void KLineWidget::setConnectionStatus(bool connected) { m_connected = connected; updateRealtimeLabel(); }
void KLineWidget::updateRealtimeLabel()
{
    if (!m_realtimeLabel || m_lastPrice == 0) { if (m_realtimeLabel) m_realtimeLabel->setVisible(false); return; }
    double change = m_lastPrice - m_lastOpen;
    QString arrow = (change >= 0) ? QStringLiteral("▲") : QStringLiteral("▼");
    QString dot = QStringLiteral("●");
    QString text = QStringLiteral("%1 %2 %3 %4%5")
        .arg(dot).arg(arrow).arg(m_lastPrice,0,'f',2).arg(change>=0?"+":"").arg(change,0,'f',2);
    m_realtimeLabel->setText(text); m_realtimeLabel->adjustSize();
    m_realtimeLabel->setGeometry(width() - m_realtimeLabel->width() - 10, 10, m_realtimeLabel->width(), m_realtimeLabel->height());
    m_realtimeLabel->raise(); m_realtimeLabel->setVisible(true);
}

int KLineWidget::addShape(QSharedPointer<Shape> s)
{
    s->id = m_nextShapeId++;
    if (!s->color.isValid()) s->color = s->followsKLine() ? Qt::white : QColor(255,200,100);
    // 自动关联当前品种周期
    if (s->symbol.isEmpty()) s->symbol = m_symbol;
    if (s->timeframe == 0) s->timeframe = m_baseMinutes;
    m_shapes.append(s); update();
    return s->id;
}

// ─── Save/Load ───

QString KLineWidget::shapesFilePath() const
{
    if (m_symbol.isEmpty()) return {};
    QString dir = QCoreApplication::applicationDirPath() + "/data/shapes";
    QDir().mkpath(dir);
    return dir + "/" + m_symbol + "_" + QString::number(m_baseMinutes) + ".json";
}

void KLineWidget::saveShapes()
{
    QString path = shapesFilePath();
    if (path.isEmpty()) return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    emit shapesSaved(m_symbol, m_baseMinutes);

    QJsonArray arr;
    for (const auto &s : m_shapes) {
        if (s->fromScript) continue;
        arr.append(s->toJson());
    }
    QJsonObject root;
    root["symbol"] = m_symbol;
    root["timeframe"] = m_baseMinutes;
    root["nextId"] = m_nextShapeId;
    root["shapes"] = arr;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void KLineWidget::loadShapes()
{
    QString path = shapesFilePath();
    m_shapes.clear(); m_selectedShapeIndex = -1;
    if (path.isEmpty() || !QFile::exists(path)) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject root = doc.object();
    m_nextShapeId = root["nextId"].toInt(m_nextShapeId);
    QJsonArray arr = root["shapes"].toArray();

    m_shapes.clear();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        auto s = Shape::createFromJson(obj);
        if (s) m_shapes.append(s);
    }
    m_selectedShapeIndex = -1; update();
}

// ─── Crosshair Info Box ───

void KLineWidget::drawCrosshairInfoBox(QPainter &p, int candleIdx, double price)
{
    if (candleIdx < 0 || candleIdx >= m_data.size()) return;
    const Candle &c = m_data.at(candleIdx);
    QString timeStr = c.date.toString("yyyy-MM-dd HH:mm");
    double change = c.close - c.open;
    double pct = (c.open != 0) ? (change / c.open) * 100.0 : 0.0;
    QString changeStr = (change>=0?"+":"") + QString::number(change,'f',2) + " (" + (change>=0?"+":"") + QString::number(pct,'f',2) + "%)";
    QStringList lines = {timeStr, QString("O: %1  H: %2").arg(c.open,0,'f',2).arg(c.high,0,'f',2),
        QString("L: %1  C: %2").arg(c.low,0,'f',2).arg(c.close,0,'f',2), QString("V: %1").arg(c.volume,0,'f',0), changeStr};
    p.setFont(QFont("Consolas",9,QFont::Bold));
    QFontMetrics fm(p.font()); int maxW=0, totalH=0, lh=fm.height()+2;
    for (const auto &l : lines) { int lw = fm.horizontalAdvance(l)+12; if (lw>maxW) maxW=lw; totalH+=lh; }
    totalH += 6;
    int bx=m_crosshairPos.x()+15, by=m_crosshairPos.y()-totalH/2;
    if (bx+maxW > width()) bx=m_crosshairPos.x()-maxW-15;
    if (by < 5) by=5; if (by+totalH > height()-5) by=height()-totalH-5;
    p.setBrush(QColor(20,20,30,220)); p.setPen(QPen(QColor(255,255,0,180),1));
    p.drawRoundedRect(QRectF(bx,by,maxW,totalH),5,5);
    int ty=by+5;
    for (int i=0;i<lines.size();++i) {
        QColor clr = (i==0) ? QColor(255,220,100) : (i==lines.size()-1) ? (change>=0?QColor(220,20,60):QColor(0,200,0)) : QColor(200,200,200);
        p.setPen(clr); p.drawText(bx+6,ty,maxW-12,lh,Qt::AlignLeft|Qt::AlignVCenter,lines[i]); ty+=lh;
    }
}
