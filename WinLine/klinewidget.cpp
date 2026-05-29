#include "klinewidget.h"
#include "shapedialog.h"
#include "chartconfig.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QPainterPath>
#include <QToolTip>
#include <QInputDialog>
#include <QKeyEvent>
#include <QColorDialog>
#include <QLabel>
#include <QVBoxLayout>


KLineWidget::KLineWidget(QWidget *parent)
    : QWidget(parent), m_minPrice(0), m_maxPrice(0), m_scale(1.0), m_candleWidth(6.0), m_gap(2.0), m_startIndex(0), m_panning(false), m_crosshairVisible(false), m_rightPadding(80), m_timeframe(KLineWidget::TF_1m), m_baseMinutes(1)
    , m_toolMode(Tool_None), m_selectedShapeIndex(-1), m_drawing(false), m_draggingEndpoint(0), m_movingShape(false), m_nextShapeId(1)
{
    setMinimumSize(600, 500);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Loading overlay label
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

    // Realtime price label
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

void KLineWidget::showLoading(const QString &msg)
{
    if (!m_loadingLabel) return;
    m_loadingLabel->setText(msg);
    // Center the label in the widget
    int w = qMin(width() * 3 / 4, 400);
    int h = 100;
    m_loadingLabel->setGeometry((width() - w) / 2, (height() - h) / 2, w, h);
    m_loadingLabel->raise();
    m_loadingLabel->setVisible(true);
}

void KLineWidget::hideLoading()
{
    if (m_loadingLabel) {
        m_loadingLabel->setVisible(false);
    }
}

// helper: distance from point to segment (bounded between endpoints)
static double pointSegDist(const QPointF &p, const QPointF &a, const QPointF &b){
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

// helper: intersection of two lines (p1-p2) and (q1-q2). Returns true if not parallel and sets out to intersection point.
static bool intersectLines(const QPointF &p1, const QPointF &p2, const QPointF &q1, const QPointF &q2, QPointF &out)
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

void KLineWidget::setData(const QVector<Candle> &data)
{
    setData(data, 1);
}

void KLineWidget::setData(const QVector<Candle> &data, int baseMinutes)
{
    m_allData = data;
    m_baseMinutes = qMax(1, baseMinutes);
    // default timeframe: use the data as-is
    setTimeframe(m_timeframe);
}

void KLineWidget::updateRange()
{
    if (m_data.isEmpty()) {
        m_minPrice = 0;
        m_maxPrice = 0;
        return;
    }
    // determine visible range for main chart
    int count = visibleCount();
    int end = qMin(m_startIndex + count, m_data.size());
    if (end <= m_startIndex) {
        m_minPrice = m_data.first().low;
        m_maxPrice = m_data.first().high;
        return;
    }
    m_minPrice = m_data.at(m_startIndex).low;
    m_maxPrice = m_data.at(m_startIndex).high;
    for (int i = m_startIndex; i < end; ++i) {
        const Candle &c = m_data.at(i);
        if (c.low < m_minPrice) m_minPrice = c.low;
        if (c.high > m_maxPrice) m_maxPrice = c.high;
    }
    // add small padding so candles don't touch top/bottom
    double pad = (m_maxPrice - m_minPrice) * 0.06;
    if (pad <= 0) pad = 1.0;
    m_minPrice -= pad;
    m_maxPrice += pad;
    if (m_minPrice == m_maxPrice) {
        m_minPrice -= 1;
        m_maxPrice += 1;
    }
    
    // ensure moving averages are calculated
    if (m_ma5.isEmpty() && !m_data.isEmpty()) {
        calculateMovingAverages();
    }
}

int KLineWidget::visibleCount() const
{
    // use main chart rect width to compute visible count so indicators align with main chart
    int w = mainChartRect().width();
    double tp = (m_candleWidth * m_scale) + m_gap;
    if (tp <= 0) return 1;
    int cnt = qMax(1, int(w / tp));
    return cnt;
}

void KLineWidget::ensureStartIndexVisible()
{
    if (m_startIndex < 0) m_startIndex = 0;
    int maxStart = qMax(0, m_data.size() - visibleCount());
    if (m_startIndex > maxStart) m_startIndex = maxStart;
}

void KLineWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    ensureStartIndexVisible();
    updateRange();

    // keep crosshair synchronized after resize
    if (m_crosshairVisible && !m_data.isEmpty()) {
        snapCrosshairTo(m_crosshairPos);
    }
}

void KLineWidget::mousePressEvent(QMouseEvent *event)
{
    // allow middle button to quickly switch to normal mode (no drawing)
    if (event->button() == Qt::MiddleButton) {
        setToolMode(Tool_None);
        return;
    }

    // Normal mode: allow selecting shapes to delete or edit
    if (m_toolMode == Tool_None) {
        if (event->button() == Qt::LeftButton) {
            QPointF pt = event->pos();
            int clickedIdx = -1;
            int dragEndpoint = 0;
            double minDist = 1e9;

            // try to select a shape using screen coordinates directly
            // convert data coords to screen coords and check distance
            for (int i = 0; i < m_shapes.size(); ++i) {
                const Shape &s = m_shapes[i];

                // convert data coordinates to screen coordinates
                QPointF screenP1, screenP2;
                dataCoordToScreen(s.candleIdx1, s.price1, screenP1);
                dataCoordToScreen(s.candleIdx2, s.price2, screenP2);

                // check endpoint proximity (higher priority)
                double dist1 = qSqrt((screenP1.x() - pt.x()) * (screenP1.x() - pt.x()) +
                                     (screenP1.y() - pt.y()) * (screenP1.y() - pt.y()));
                double dist2 = qSqrt((screenP2.x() - pt.x()) * (screenP2.x() - pt.x()) +
                                     (screenP2.y() - pt.y()) * (screenP2.y() - pt.y()));

                if (dist1 < minDist && dist1 <= 10.0) {
                    minDist = dist1;
                    clickedIdx = i;
                    dragEndpoint = 1;
                }
                if (dist2 < minDist && dist2 <= 10.0) {
                    minDist = dist2;
                    clickedIdx = i;
                    dragEndpoint = 2;
                }

                // check line proximity (lower priority) - use extended line for infinite/ray lines
                if (dragEndpoint == 0) {
                    double lineDist = 1e9;

                    if (s.type == Shape_Line) {
                        // infinite line: extend infinitely in both directions
                        lineDist = pointToLineDist(pt, screenP1, screenP2);
                    } else if (s.type == Shape_Trend) {
                        // ray: extend from p1 through p2 to edge
                        lineDist = pointToRayDist(pt, screenP1, screenP2);
                    } else {
                        // segment: only between p1 and p2
                        lineDist = pointSegDist(pt, screenP1, screenP2);
                    }

                    if (lineDist < minDist && lineDist <= 10.0) {
                        minDist = lineDist;
                        clickedIdx = i;
                        dragEndpoint = 0;
                    }
                }
            }

            if (clickedIdx >= 0 && dragEndpoint > 0) {
                m_selectedShapeIndex = clickedIdx;
                m_draggingEndpoint = dragEndpoint;
            } else if (clickedIdx >= 0) {
                m_selectedShapeIndex = clickedIdx;
                m_draggingEndpoint = 0;
            } else {
                m_panning = true;
                m_selectedShapeIndex = -1;
                m_draggingEndpoint = 0;
                setCursor(Qt::ClosedHandCursor);
            }
            m_lastMousePos = event->pos();
            update();
            return;
        }
    }

    // Drawing tool handling — 没数据时不处理画图操作
    if (m_toolMode != Tool_None) {
        if (m_data.isEmpty()) {
            m_toolMode = Tool_None;
            setCursor(Qt::ArrowCursor);
            update();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            // ensure cross cursor remains while drawing
            setCursor(Qt::CrossCursor);
            QPointF pt = event->pos();
            // try to hit existing shape endpoints or lines first (for dragging)
            int clickedIdx = -1;
            int dragEndpoint = 0;
            double minDist = 1e9;

            for (int i = 0; i < m_shapes.size(); ++i) {
                const Shape &s = m_shapes[i];
                QPointF screenP1, screenP2;
                dataCoordToScreen(s.candleIdx1, s.price1, screenP1);
                dataCoordToScreen(s.candleIdx2, s.price2, screenP2);

                // check endpoint proximity
                double dist1 = qSqrt((screenP1.x() - pt.x())*(screenP1.x() - pt.x()) + (screenP1.y() - pt.y())*(screenP1.y() - pt.y()));
                double dist2 = qSqrt((screenP2.x() - pt.x())*(screenP2.x() - pt.x()) + (screenP2.y() - pt.y())*(screenP2.y() - pt.y()));

                if (dist1 < minDist && dist1 <= 10.0) { minDist = dist1; clickedIdx = i; dragEndpoint = 1; }
                if (dist2 < minDist && dist2 <= 10.0) { minDist = dist2; clickedIdx = i; dragEndpoint = 2; }

                // check line proximity
                if (dragEndpoint == 0) {
                    double lineDist = 1e9;
                    if (s.type == Shape_Line) lineDist = pointToLineDist(pt, screenP1, screenP2);
                    else if (s.type == Shape_Trend) lineDist = pointToRayDist(pt, screenP1, screenP2);
                    else lineDist = pointToLineDist(pt, screenP1, screenP2);
                    if (lineDist < minDist && lineDist <= 10.0) {
                        minDist = lineDist; clickedIdx = i; dragEndpoint = 0;
                    }
                }
            }

            if (clickedIdx >= 0) {
                m_selectedShapeIndex = clickedIdx;
                m_draggingEndpoint = dragEndpoint;
                m_movingShape = (dragEndpoint == 0);
                m_drawing = false;
                m_lastMousePos = event->pos();
                update();
                return;
            }

            // start new shape: convert screen coord to data coord
            Shape ns;
            // map ToolMode to ShapeType: Tool_None->none, Tool_Line->Shape_Line, ...
            ns.type = (ShapeType)(m_toolMode - 1);
            ns.selected = true;
            ns.text.clear();
            ns.id = m_nextShapeId++;
            ns.name = QString("shape_%1").arg(ns.id);
            ns.color = Qt::white;
            // convert screen to data coordinates
            screenToDataCoord(pt, ns.candleIdx1, ns.price1);
            ns.candleIdx2 = ns.candleIdx1; ns.price2 = ns.price1;

            // For trade shapes, snap to candle price and record trade info
            if (ns.type == Shape_TradeBuy || ns.type == Shape_TradeSell ||
                ns.type == Shape_TradeShort || ns.type == Shape_TradeCover) {
                int idx = ns.candleIdx1;
                if (idx >= 0 && idx < m_data.size()) {
                    double tradePrice = m_data[idx].close;     // 默认取收盘价
                    ns.price1 = tradePrice;
                    ns.price2 = tradePrice;
                    ns.tradePrice = tradePrice;
                    ns.tradeTime = m_data[idx].date;
                    ns.quantity = 1;
                    ns.profit = 0.0;
                    // 设置默认名称
                    if (ns.type == Shape_TradeBuy) ns.name = QStringLiteral("做多买入");
                    else if (ns.type == Shape_TradeSell) ns.name = QStringLiteral("做多卖出");
                    else if (ns.type == Shape_TradeShort) ns.name = QStringLiteral("做空卖出");
                    else if (ns.type == Shape_TradeCover) ns.name = QStringLiteral("做空买入");
                    ns.text = ns.name;
                }
                m_drawing = false;
            }
            // For horizontal line, p2 has same price but different candle index
            else if (ns.type == Shape_HLine || ns.type == Shape_VLine) {
                // p1 and p2 at same position for now, user can drag to define
                m_drawing = true;
                m_draggingEndpoint = 2;
            }
            // For text shapes, immediately open the dialog to set text and color
            else if (ns.type == Shape_Text) {
                ShapeDialog dlg(this);
                dlg.setShapeName(ns.name);
                dlg.setShapeColor(ns.color);
                if (dlg.exec() == QDialog::Accepted) {
                    ns.name = dlg.getShapeName();
                    ns.color = dlg.getShapeColor();
                    ns.text = ns.name;
                } else {
                    return; // Cancel, don't create the shape
                }
                m_drawing = false;
            } else {
                m_drawing = true;
                m_draggingEndpoint = 2;
            }

            m_shapes.append(ns);
            m_selectedShapeIndex = m_shapes.size() - 1;
            m_lastMousePos = event->pos();
            update();
            setFocus();
            return;
        }
    }

    // Panning / crosshair fallback - existing behavior
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void KLineWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Normal mode: allow dragging endpoints to edit shapes
    if (m_toolMode == Tool_None) {
        if (m_selectedShapeIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
            Shape &s = m_shapes[m_selectedShapeIndex];
            if (m_draggingEndpoint == 1) {
                screenToDataCoord(event->pos(), s.candleIdx1, s.price1);
                update();
                return;
            }
            if (m_draggingEndpoint == 2) {
                screenToDataCoord(event->pos(), s.candleIdx2, s.price2);
                update();
                return;
            }
        }
    }

    // Drawing tool interactions
    if (m_toolMode != Tool_None) {
        if (m_selectedShapeIndex >= 0) {
            Shape &s = m_shapes[m_selectedShapeIndex];
            if (m_draggingEndpoint == 1) {
                screenToDataCoord(event->pos(), s.candleIdx1, s.price1);
                update();
                return;
            }
            if (m_draggingEndpoint == 2) {
                screenToDataCoord(event->pos(), s.candleIdx2, s.price2);
                update();
                return;
            }
            if (m_movingShape && (event->buttons() & Qt::LeftButton)) {
                QPointF oldP1, oldP2;
                dataCoordToScreen(s.candleIdx1, s.price1, oldP1);
                dataCoordToScreen(s.candleIdx2, s.price2, oldP2);
                QPointF delta = event->pos() - oldP1;
                // apply delta back to data coords
                QPointF newP1 = oldP1 + delta;
                QPointF newP2 = oldP2 + delta;
                screenToDataCoord(newP1, s.candleIdx1, s.price1);
                screenToDataCoord(newP2, s.candleIdx2, s.price2);
                m_lastMousePos = event->pos();
                update();
                return;
            }
            if (m_drawing) {
                screenToDataCoord(event->pos(), s.candleIdx2, s.price2);
                update();
                return;
            }
        }
    }

    // existing panning, crosshair, tooltip logic
    if (m_panning && !m_data.isEmpty()) {
        int dx = event->pos().x() - m_lastMousePos.x();
        double totalPer = (m_candleWidth * m_scale) + m_gap;
        if (totalPer > 0) {
            int deltaIndex = int(-dx / totalPer);
            if (deltaIndex != 0) {
                m_startIndex += deltaIndex;
                ensureStartIndexVisible();
                updateRange();
                update();
                m_lastMousePos = event->pos();
                emit viewportChanged(m_startIndex, visibleCount());
                emit layoutChanged(m_startIndex, visibleCount(), totalPer, candleBodyWidth(), mainChartRect());
                // keep crosshair in sync after viewport change
                if (m_crosshairVisible) {
                    snapCrosshairTo(m_crosshairPos);
                }
            }
        }
    }
    if (m_crosshairVisible) {
        // snap to x center but use mouse y
        snapCrosshairTo(event->pos());
        update();
    }

    // tooltip logic
    if (!m_data.isEmpty()) {
        // use main chart rect left to align with main chart instead of hardcoded 40
        QRect mr = mainChartRect();
        int contentLeft = mr.left();
        double totalPer = (m_candleWidth * m_scale) + m_gap;
        if (totalPer > 0) {
            int relX = event->pos().x() - contentLeft;
            int relIdx = int((double)relX / totalPer + 0.5);
            int idx = m_startIndex + relIdx;
            if (idx >= 0 && idx < m_data.size()) {
                const Candle &c = m_data.at(idx);
                double x = contentLeft + relIdx * totalPer;
                double bodyW = m_candleWidth * m_scale;
                double bodyLeft = x + (totalPer - bodyW) / 2.0;
                double bodyRight = bodyLeft + bodyW;
                int marginTop = 10;
                int marginBottom = 20;
                QRect mainRect(contentLeft, marginTop, mr.width(), mr.height());
                auto priceToYLocal = [&](double price){ double ratio = (price - m_minPrice) / (m_maxPrice - m_minPrice); return mainRect.bottom() - ratio * mainRect.height(); };
                double yOpen = priceToYLocal(c.open);
                double yClose = priceToYLocal(c.close);
                double bodyTop = qMin(yOpen, yClose);
                double bodyBottom = qMax(yOpen, yClose);
                QPoint pos = event->pos();
                if (pos.x() >= int(bodyLeft) && pos.x() <= int(bodyRight) && pos.y() >= int(bodyTop)-3 && pos.y() <= int(bodyBottom)+3) {
                    QString timeStr;
                    if (m_timeframe == TF_DAILY) timeStr = c.date.toString("yyyy-MM-dd");
                    else timeStr = c.date.toString("yyyy-MM-dd HH:mm");
                    QString tip = QString("<b>%1</b><br>Open: %2<br>High: %3<br>Low: %4<br>Close: %5<br>Vol: %6")
                            .arg(timeStr)
                            .arg(c.open, 0, 'f', 2)
                            .arg(c.high, 0, 'f', 2)
                            .arg(c.low, 0, 'f', 2)
                            .arg(c.close, 0, 'f', 2)
                            .arg(c.volume);
                    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
                } else {
                    QToolTip::hideText();
                }
            }
        }
    }
}

void KLineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_toolMode == Tool_None) {
        // stop panning in normal mode
        if (m_panning) {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
        }
        m_draggingEndpoint = 0;
        update();
        return;
    }
    
    // Drawing modes: stop dragging/panning
    m_draggingEndpoint = 0;
    m_movingShape = false;
    m_drawing = false;
    if (m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    update();
}

void KLineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 没数据时不处理双击（禁止十字交叉线）
    if (m_data.isEmpty()) {
        m_crosshairVisible = false;
        update();
        return;
    }
    QPointF pt = event->pos();
    int hitIndex = -1;

    // if double-click near any shape (endpoint or segment), open properties
    for (int i = 0; i < m_shapes.size(); ++i) {
        const Shape &s = m_shapes[i];

        // convert data coordinates to screen coordinates
        QPointF screenP1, screenP2;
        dataCoordToScreen(s.candleIdx1, s.price1, screenP1);
        dataCoordToScreen(s.candleIdx2, s.price2, screenP2);

        // check endpoint proximity
        double dist1 = qSqrt((screenP1.x() - pt.x()) * (screenP1.x() - pt.x()) +
                             (screenP1.y() - pt.y()) * (screenP1.y() - pt.y()));
        double dist2 = qSqrt((screenP2.x() - pt.x()) * (screenP2.x() - pt.x()) +
                             (screenP2.y() - pt.y()) * (screenP2.y() - pt.y()));

        if (dist1 <= 10.0 || dist2 <= 10.0) {
            hitIndex = i;
            break;
        }

        // check line proximity - use extended line for infinite/ray lines
        double lineDist = 1e9;

        if (s.type == Shape_Line) {
            // infinite line
            lineDist = pointToLineDist(pt, screenP1, screenP2);
        } else if (s.type == Shape_Trend) {
            // ray
            lineDist = pointToRayDist(pt, screenP1, screenP2);
        } else {
            // segment
            lineDist = pointSegDist(pt, screenP1, screenP2);
        }

        if (lineDist <= 10.0) {
            hitIndex = i;
            break;
        }
    }

    if (hitIndex >= 0) {
        // hit a shape: open properties dialog
        editShapeProperties(hitIndex);
    } else {
        // no shape hit: toggle crosshair
        m_crosshairVisible = !m_crosshairVisible;
        if (m_crosshairVisible) {
            m_crosshairPos = event->pos();
            const int marginLeft = 40;
            const int marginTop = 10;
            const int marginBottom = 20;
            QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
            double priceRange = m_maxPrice - m_minPrice;
            if (priceRange != 0) {
                double ratio = double(mainRect.bottom() - m_crosshairPos.y()) / double(mainRect.height());
                double price = m_minPrice + ratio * priceRange;
                double totalPer = (m_candleWidth * m_scale) + m_gap;
                int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / (totalPer > 0 ? totalPer : 1.0) + 0.5);
                idx = qBound(0, idx, m_data.size()-1);
                // snap crosshair X to candle center
                double xCenter = mainRect.left() + (idx - m_startIndex) * totalPer + totalPer / 2.0;
                m_crosshairPos.setX(int(xCenter + 0.5));
                snapCrosshairTo(m_crosshairPos);
            }
            // ensure repaint so indicator widgets show the vertical line
            update();
        } else {
            // hide crosshair: notify indicators to remove vertical line and reset price/index
            emit crosshairIndexChanged(-1);
            emit crosshairPriceChanged(0.0, -1);
            emit crosshairScreenXChanged(-1);
            // ensure repaint so indicator widgets remove the vertical line
            update();
        }
    }
}

void KLineWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        // only delete in normal mode when a shape is selected
        if (m_toolMode == Tool_None) {
            deleteSelectedShape();
        }
    }
    else if (event->key() == Qt::Key_Escape) {
        m_selectedShapeIndex = -1;
        update();
    }
    else QWidget::keyPressEvent(event);
}

void KLineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // background
    p.fillRect(rect(), QColor(10, 10, 10));

    // margins
    const int marginLeft = 5;
    const int marginTop = 10;
    const int marginBottom = 20;
    const int priceAxisWidth = 20;  // wider space for price text (supports ~10 digits)
    QRect mainRect(marginLeft + priceAxisWidth, marginTop, width() - marginLeft - priceAxisWidth - 10 - m_rightPadding, height() - marginTop - marginBottom);

    // draw price scale (Y-axis) - light gray color with at least 10 price levels
    {
        const int marginLeft = 10;
        const int priceAxisWidth = 40;
        const int marginTop = 10;
        const int marginBottom = 20;
        QRect priceMainRect(marginLeft + priceAxisWidth, marginTop, width() - marginLeft - priceAxisWidth - 10 - m_rightPadding, height() - marginTop - marginBottom);
        double priceRange = m_maxPrice - m_minPrice;
        if (priceRange > 0) {
            // ensure at least 10 price ticks visible
            double step = priceRange / 10.0;  // 10 divisions minimum
            if (step <= 0) step = 1.0;

            p.setFont(QFont("Arial", 8));

            for (double price = m_minPrice; price <= m_maxPrice; price += step) {
                double ratio = (price - m_minPrice) / priceRange;
                int y = priceMainRect.bottom() - ratio * priceMainRect.height();

                // draw tick mark - light gray color
                p.setPen(QPen(QColor(180, 180, 180), 2));
                p.drawLine(priceMainRect.left() - 5, y, priceMainRect.left() - 2, y);

                // draw price label - light gray (support up to ~10 digit numbers)
                p.setPen(QPen(QColor(200, 200, 200), 1));
                QString priceStr = QString::number(price, 'f', 2);
                QFontMetrics fm(p.font());
                int textWidth = fm.horizontalAdvance(priceStr);
                // use wider left margin to support 10+ digit prices
                p.drawText(marginLeft + priceAxisWidth - textWidth - 8, y - 4, priceStr);
            }
        }
    }

    // draw time scale (X-axis) - light cyan color with higher density
    {
        // use mainRect computed above for consistent left position
        double totalPer = (m_candleWidth * m_scale) + m_gap;
        if (totalPer > 0 && !m_data.isEmpty()) {
            int visCount = visibleCount();
            int tickInterval = qMax(1, visCount / 20);
            p.setFont(QFont("Arial", 8));
            for (int i = m_startIndex; i < m_startIndex + visCount && i < m_data.size(); ++i) {
                if ((i - m_startIndex) % tickInterval == 0) {
                    double x = mainRect.left() + (i - m_startIndex) * totalPer + totalPer / 2.0;
                    int y = height() - marginBottom;
                    p.setPen(QPen(QColor(100, 200, 200), 2));
                    p.drawLine(x, y + 2, x, y + 5);
                    p.setPen(QPen(QColor(150, 220, 220), 1));
                    QString timeStr;
                    if (i < m_data.size()) {
                        if (m_timeframe == TF_DAILY) timeStr = m_data[i].date.toString("yyyy-MM-dd");
                        else timeStr = m_data[i].date.toString("yyyy-MM-dd HH:mm");
                    }
                    QFontMetrics fm(p.font());
                    int textWidth = fm.horizontalAdvance(timeStr);
                    p.drawText(x - textWidth / 2, y + 15, timeStr);
                }
            }
        }
    }
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0; // avoid div by zero
    double totalPer = (m_candleWidth * m_scale) + m_gap;

    // publish layout for other indicator widgets
    ChartConfig::setLayout(mainRect, totalPer, m_startIndex, visibleCount(), candleBodyWidth(), m_rightPadding);

    auto priceToY = [&](double price){ double ratio = (price - m_minPrice) / priceRange; return mainRect.bottom() - ratio * mainRect.height(); };

    for (int i = m_startIndex; i < m_startIndex + visibleCount() && i < m_data.size(); ++i) {
        const Candle &c = m_data.at(i);
        double x = mainRect.left() + (i - m_startIndex) * totalPer;
        double ox = x + totalPer / 2.0;
        double yHigh = priceToY(c.high);
        double yLow = priceToY(c.low);
        double yOpen = priceToY(c.open);
        double yClose = priceToY(c.close);

        bool rise = c.close >= c.open;
        QColor color = rise ? QColor(220,20,60) : QColor(0,200,0); // red/green
        p.setPen(QPen(color));
        p.setBrush(color);

        // wick
        p.drawLine(QPointF(ox, yHigh), QPointF(ox, yLow));

        // body
        double bodyW = m_candleWidth * m_scale;
        QRectF bodyRect(x + (totalPer - bodyW) / 2.0, std::min(yOpen, yClose), bodyW, qMax(1.0, fabs(yClose - yOpen)));
        p.fillRect(bodyRect, color);
        p.drawRect(bodyRect);
    }

    // draw user shapes (lines, gestures, text) with selection handles
    for (int i = 0; i < m_shapes.size(); ++i) {
        const Shape &s = m_shapes.at(i);
        // convert data coords to screen coords for drawing
        QPointF screenP1, screenP2;
        dataCoordToScreen(s.candleIdx1, s.price1, screenP1);
        dataCoordToScreen(s.candleIdx2, s.price2, screenP2);

        QPen sp((i == m_selectedShapeIndex) ? Qt::yellow : s.color.isValid() ? s.color : Qt::white);
        sp.setWidth(2);
        sp.setCapStyle(Qt::RoundCap);
        p.setPen(sp);
        if (s.type == Shape_Line) {
            // draw infinite line through screenP1-screenP2 clipped to mainRect
            QLineF ln(screenP1, screenP2);
            if (qFuzzyIsNull(ln.dx()) && qFuzzyIsNull(ln.dy())) continue;
            QVector<QPointF> inters;
            QPointF e1 = mainRect.topLeft(); QPointF e2 = mainRect.topRight();
            QPointF e3 = mainRect.bottomRight(); QPointF e4 = mainRect.bottomLeft();
            QPointF ip;
            if (intersectLines(screenP1, screenP2, e1, e2, ip)) {
                // ip must be within edge segment
                if (ip.x() >= qMin(e1.x(), e2.x()) - 1e-6 && ip.x() <= qMax(e1.x(), e2.x()) + 1e-6 &&
                    ip.y() >= qMin(e1.y(), e2.y()) - 1e-6 && ip.y() <= qMax(e1.y(), e2.y()) + 1e-6) inters.append(ip);
            }
            if (intersectLines(screenP1, screenP2, e2, e3, ip)) {
                if (ip.x() >= qMin(e2.x(), e3.x()) - 1e-6 && ip.x() <= qMax(e2.x(), e3.x()) + 1e-6 &&
                    ip.y() >= qMin(e2.y(), e3.y()) - 1e-6 && ip.y() <= qMax(e2.y(), e3.y()) + 1e-6) inters.append(ip);
            }
            if (intersectLines(screenP1, screenP2, e3, e4, ip)) {
                if (ip.x() >= qMin(e3.x(), e4.x()) - 1e-6 && ip.x() <= qMax(e3.x(), e4.x()) + 1e-6 &&
                    ip.y() >= qMin(e3.y(), e4.y()) - 1e-6 && ip.y() <= qMax(e3.y(), e4.y()) + 1e-6) inters.append(ip);
            }
            if (intersectLines(screenP1, screenP2, e4, e1, ip)) {
                if (ip.x() >= qMin(e4.x(), e1.x()) - 1e-6 && ip.x() <= qMax(e4.x(), e1.x()) + 1e-6 &&
                    ip.y() >= qMin(e4.y(), e1.y()) - 1e-6 && ip.y() <= qMax(e4.y(), e1.y()) + 1e-6) inters.append(ip);
            }
            if (inters.size() >= 2) {
                // remove duplicates
                QVector<QPointF> unique;
                for (const QPointF &ptp: inters) {
                    bool found = false;
                    for (const QPointF &qpt: unique) if (qAbs(qpt.x()-ptp.x())<1e-4 && qAbs(qpt.y()-ptp.y())<1e-4) { found = true; break; }
                    if (!found) unique.append(ptp);
                }
                if (unique.size() >= 2) p.drawLine(unique.first(), unique.last());
            } else {
                p.drawLine(screenP1, screenP2);
            }
        } else if (s.type == Shape_Trend) {
            // draw ray from screenP1 through screenP2 to edge of mainRect
            QLineF ln(screenP1, screenP2);
            if (qFuzzyIsNull(ln.dx()) && qFuzzyIsNull(ln.dy())) continue;
            // extend to far intersection
            QPointF e1 = mainRect.topLeft(); QPointF e2 = mainRect.topRight();
            QPointF e3 = mainRect.bottomRight(); QPointF e4 = mainRect.bottomLeft();
            QPointF ip;
            qreal bestT = -1e12;
            QPointF bestPt;
            bool found = false;
            auto checkEdge = [&](const QPointF &a, const QPointF &b){
                if (!intersectLines(screenP1, screenP2, a, b, ip)) return;
                if (ip.x() < qMin(a.x(), b.x()) - 1e-6 || ip.x() > qMax(a.x(), b.x()) + 1e-6) return;
                if (ip.y() < qMin(a.y(), b.y()) - 1e-6 || ip.y() > qMax(a.y(), b.y()) + 1e-6) return;
                double t;
                if (!qFuzzyIsNull(ln.dx())) t = (ip.x() - screenP1.x()) / ln.dx();
                else t = (ip.y() - screenP1.y()) / ln.dy();
                // only accept intersections that lie in the forward direction of the ray
                if (t > 1e-6 && t > bestT) {
                    bestT = t;
                    bestPt = ip;
                    found = true;
                }
            };
            checkEdge(e1, e2);
            checkEdge(e2, e3);
            checkEdge(e3, e4);
            checkEdge(e4, e1);
            if (found) {
                p.drawLine(screenP1, bestPt);
            }
        } else if (s.type == Shape_GestureUp) {
            // 画从 p1 到 p2 的线段，终点画三角形箭头
            p.drawLine(screenP1, screenP2);
            // 计算箭头方向（从 p1 指向 p2）
            double angle = qAtan2(screenP2.y() - screenP1.y(), screenP2.x() - screenP1.x());
            double arrowLen = 10.0;
            double arrowAngle = 0.5; // ~28度
            QPointF arrowP1(screenP2.x() - arrowLen * qCos(angle - arrowAngle),
                           screenP2.y() - arrowLen * qSin(angle - arrowAngle));
            QPointF arrowP2(screenP2.x() - arrowLen * qCos(angle + arrowAngle),
                           screenP2.y() - arrowLen * qSin(angle + arrowAngle));
            // 填充箭头三角形
            QPolygonF arrowHead;
            arrowHead << screenP2 << arrowP1 << arrowP2;
            p.setBrush(sp.color());
            p.drawPolygon(arrowHead);
        } else if (s.type == Shape_GestureDown) {
            // 画从 p1 到 p2 的线段，终点画三角形箭头（向下箭头）
            p.drawLine(screenP1, screenP2);
            double angle = qAtan2(screenP2.y() - screenP1.y(), screenP2.x() - screenP1.x());
            double arrowLen = 10.0;
            double arrowAngle = 0.5;
            QPointF arrowP1(screenP2.x() - arrowLen * qCos(angle - arrowAngle),
                           screenP2.y() - arrowLen * qSin(angle - arrowAngle));
            QPointF arrowP2(screenP2.x() - arrowLen * qCos(angle + arrowAngle),
                           screenP2.y() - arrowLen * qSin(angle + arrowAngle));
            QPolygonF arrowHead;
            arrowHead << screenP2 << arrowP1 << arrowP2;
            p.setBrush(sp.color());
            p.drawPolygon(arrowHead);
        } else if (s.type == Shape_HLine) {
            // 水平线：从主图左边缘到右边缘
            QRect mr = mainChartRect();
            double y = screenP1.y();
            p.drawLine(mr.left(), (int)y, mr.right(), (int)y);
        } else if (s.type == Shape_VLine) {
            // 垂直线：从主图上边缘到下边缘
            QRect mr = mainChartRect();
            double x = screenP1.x();
            p.drawLine((int)x, mr.top(), (int)x, mr.bottom());
        } else if (s.type == Shape_TradeBuy || s.type == Shape_TradeCover) {
            // 买入/平仓标记 (K线下方绿色向上箭头)
            QColor tradeColor = (s.type == Shape_TradeBuy) ? QColor(0, 220, 0) : QColor(220, 100, 0);
            QString label = (s.type == Shape_TradeBuy) ? QStringLiteral("B") : QStringLiteral("C");
            QString sideName = (s.type == Shape_TradeBuy) ? QStringLiteral("做多买入") : QStringLiteral("做空买入");
            
            // 画向上箭头
            double arrowSize = 8;
            qreal cx = screenP1.x();
            qreal baseY = screenP1.y() + 6; // K线下方
            QPolygonF arrowHead;
            arrowHead << QPointF(cx, baseY - arrowSize)
                      << QPointF(cx - arrowSize * 0.6, baseY)
                      << QPointF(cx + arrowSize * 0.6, baseY);
            p.setBrush(tradeColor);
            p.setPen(QPen(tradeColor, 1));
            p.drawPolygon(arrowHead);
            
            // 画标签背景
            QString tradeText = QString("%1 %2").arg(label).arg(s.tradePrice, 0, 'f', 2);
            if (s.quantity > 1) tradeText += QString(" x%1").arg(s.quantity);
            if (s.type == Shape_TradeCover && s.profit != 0.0) {
                tradeText += QString(" %1%2").arg(s.profit >= 0 ? "+" : "").arg(s.profit, 0, 'f', 2);
            }
            p.setFont(QFont("Arial", 9, QFont::Bold));
            QFontMetrics fm(p.font());
            int tw = fm.horizontalAdvance(tradeText) + 8;
            int th = fm.height() + 4;
            QRectF labelRect(cx - tw/2, baseY + 2, tw, th);
            p.setBrush(QColor(0, 0, 0, 200));
            p.setPen(QPen(tradeColor, 1));
            p.drawRoundedRect(labelRect, 3, 3);
            
            // 画文字
            p.setPen(tradeColor);
            p.drawText(labelRect, Qt::AlignCenter, tradeText);
        } else if (s.type == Shape_TradeSell || s.type == Shape_TradeShort) {
            // 卖出/做空标记 (K线上方红色向下箭头)
            QColor tradeColor = (s.type == Shape_TradeSell) ? QColor(0, 220, 0) : QColor(220, 0, 0);
            QString label = (s.type == Shape_TradeSell) ? QStringLiteral("S") : QStringLiteral("SS");
            QString sideName = (s.type == Shape_TradeSell) ? QStringLiteral("做多卖出") : QStringLiteral("做空卖出");
            
            // 画向下箭头
            double arrowSize = 8;
            qreal cx = screenP1.x();
            qreal baseY = screenP1.y() - 6; // K线上方
            QPolygonF arrowHead;
            arrowHead << QPointF(cx, baseY + arrowSize)
                      << QPointF(cx - arrowSize * 0.6, baseY)
                      << QPointF(cx + arrowSize * 0.6, baseY);
            p.setBrush(tradeColor);
            p.setPen(QPen(tradeColor, 1));
            p.drawPolygon(arrowHead);
            
            // 画标签背景
            QString tradeText = QString("%1 %2").arg(label).arg(s.tradePrice, 0, 'f', 2);
            if (s.quantity > 1) tradeText += QString(" x%1").arg(s.quantity);
            if (s.type == Shape_TradeSell && s.profit != 0.0) {
                tradeText += QString(" %1%2").arg(s.profit >= 0 ? "+" : "").arg(s.profit, 0, 'f', 2);
            }
            p.setFont(QFont("Arial", 9, QFont::Bold));
            QFontMetrics fm(p.font());
            int tw = fm.horizontalAdvance(tradeText) + 8;
            int th = fm.height() + 4;
            QRectF labelRect(cx - tw/2, baseY - th - 2, tw, th);
            p.setBrush(QColor(0, 0, 0, 200));
            p.setPen(QPen(tradeColor, 1));
            p.drawRoundedRect(labelRect, 3, 3);
            
            // 画文字
            p.setPen(tradeColor);
            p.drawText(labelRect, Qt::AlignCenter, tradeText);
        } else if (s.type == Shape_Text) {
            // draw text with point - use p1 as the center point
            int textW = 160;
            int textH = 24;
            // Draw point at p1
            p.setBrush(s.color.isValid() ? s.color : Qt::white);
            p.setPen(s.color.isValid() ? s.color : Qt::white);
            p.drawEllipse(screenP1, 4, 4);
            
            // Draw text relative to the point
            QRectF tr(screenP1.x() - textW/2, screenP1.y() - textH - 5, textW, textH);
            p.setFont(QFont("Arial", 10));
            p.setPen(s.color.isValid() ? s.color : Qt::white);
            p.drawText(tr, Qt::AlignCenter, s.text);
        }
        
        // draw endpoint handles for selected shape (but not for text - text is already complete)
        if (i == m_selectedShapeIndex && s.type != Shape_Text) {
            p.setBrush(Qt::yellow);
            p.drawEllipse(screenP1, 4, 4);
            p.drawEllipse(screenP2, 4, 4);
        }
    }
    
    // draw moving averages
    drawMovingAverages(p);
    
    // draw crosshair if visible
    if (m_crosshairVisible) {
        const int marginLeft = 40;
        const int marginTop = 10;
        const int marginBottom = 20;
        QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);

        // draw vertical and horizontal lines in bright cyan color
        p.setPen(QPen(QColor(0, 255, 255), 2, Qt::SolidLine));
        p.drawLine(m_crosshairPos.x(), mainRect.top(), m_crosshairPos.x(), mainRect.bottom());
        p.drawLine(mainRect.left(), m_crosshairPos.y(), mainRect.right(), m_crosshairPos.y());

        // draw center point in bright yellow
        p.setPen(QPen(QColor(255, 255, 0), 2));
        p.setBrush(QColor(255, 255, 0));
        p.drawEllipse(m_crosshairPos, 4, 4);

        // draw crosshair value labels (price and time)
        double priceRange = m_maxPrice - m_minPrice;
        if (priceRange != 0) {
            double ratio = double(mainRect.bottom() - m_crosshairPos.y()) / double(mainRect.height());
            double price = m_minPrice + ratio * priceRange;

            double totalPer = (m_candleWidth * m_scale) + m_gap;
            int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / (totalPer > 0 ? totalPer : 1.0) + 0.5);
            idx = qBound(0, idx, m_data.size()-1);

            // draw time label at bottom of vertical line (竖线显示时间在下方)
            p.setFont(QFont("Arial", 11, QFont::Bold));
            p.setPen(QPen(QColor(255, 255, 0), 1));
            QString timeStr;
            if (idx >= 0 && idx < m_data.size()) {
                // Always show full format: YYYY-MM-DD HH:mm
                timeStr = m_data[idx].date.toString("yyyy-MM-dd HH:mm");
            }
            QFontMetrics fm(p.font());
            int textWidth = fm.horizontalAdvance(timeStr);
            int textHeight = fm.height();
            p.drawText(m_crosshairPos.x() - textWidth / 2, mainRect.bottom() + 15, timeStr);

            // draw price label at right of horizontal line (横线显示价格)
            QString priceStr = QString::number(price, 'f', 2);
            int priceWidth = fm.horizontalAdvance(priceStr);
            p.drawText(mainRect.right() + 5, m_crosshairPos.y() + textHeight / 2, priceStr);
        }
    }
}

void KLineWidget::setTimeframe(Timeframe tf)
{
    m_timeframe = tf;
    int targetMin = int(m_timeframe);
    if (targetMin <= m_baseMinutes) {
        m_data = m_allData;
    } else {
        int factor = targetMin / m_baseMinutes;
        if (factor <= 1) m_data = m_allData;
        else aggregateData(factor);
    }
    m_startIndex = qMax(0, m_data.size() - visibleCount());
    updateRange();
    calculateMovingAverages();
    emit dataAggregated(m_data);
    emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
    update();

    // keep crosshair synchronized after timeframe change
    if (m_crosshairVisible && !m_data.isEmpty()) {
        snapCrosshairTo(m_crosshairPos);
    }
}

void KLineWidget::setToolMode(ToolMode m)
{
    m_toolMode = m;
    m_selectedShapeIndex = -1;
    m_drawing = false;
    m_draggingEndpoint = 0;
    // set cursor according to mode
    if (m_toolMode == Tool_None) setCursor(Qt::ArrowCursor);
    else setCursor(Qt::CrossCursor);
    update();
}

void KLineWidget::deleteSelectedShape()
{
    if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_shapes.size()) {
        m_shapes.removeAt(m_selectedShapeIndex);
        m_selectedShapeIndex = -1;
        update();
    }
}

void KLineWidget::clearShapes()
{
    m_shapes.clear();
    m_selectedShapeIndex = -1;
    update();
}

void KLineWidget::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    double factor = (delta > 0) ? 1.1 : 0.9;
    double oldScale = m_scale;
    m_scale *= factor;
    m_scale = qBound(0.4, m_scale, 5.0);

    double totalPerOld = (m_candleWidth * oldScale) + m_gap;
    double totalPerNew = (m_candleWidth * m_scale) + m_gap;
    if (totalPerOld > 0 && totalPerNew > 0 && !m_data.isEmpty()) {
        int mouseX = int(event->position().x());
        QRect mr = mainChartRect();
        int contentLeft = mr.left();
        int contentWidth = qMax(1, mr.width());
        double relative = (mouseX - contentLeft) / (double)contentWidth;
        relative = qBound(0.0, relative, 1.0);
        int visibleOld = qMax(1, int((contentWidth) / totalPerOld));
        int focusIndex = m_startIndex + int(relative * visibleOld);

        double focusOffset = (mouseX - contentLeft) - (focusIndex - m_startIndex) * totalPerOld;
        int computedStart = int(focusIndex - (mouseX - contentLeft - focusOffset) / totalPerNew);
        m_startIndex = computedStart;
        ensureStartIndexVisible();
        updateRange();
        emit viewportChanged(m_startIndex, visibleCount());
        emit layoutChanged(m_startIndex, visibleCount(), totalPerNew, candleBodyWidth(), mainChartRect());
        update();

        if (m_crosshairVisible) snapCrosshairTo(m_crosshairPos);
    }
}

void KLineWidget::aggregateData(int factor)
{
    m_data.clear();
    if (factor <= 1) { m_data = m_allData; return; }
    int n = m_allData.size();
    for (int i = 0; i < n; i += factor) {
        int end = qMin(i + factor, n);
        Candle agg = m_allData[i];
        agg.open = m_allData[i].open;
        agg.high = m_allData[i].high;
        agg.low = m_allData[i].low;
        agg.close = m_allData[end - 1].close;
        agg.volume = 0;
        for (int j = i; j < end; ++j) {
            agg.high = qMax(agg.high, m_allData[j].high);
            agg.low = qMin(agg.low, m_allData[j].low);
            agg.volume += m_allData[j].volume;
        }
        // use timestamp of the last candle in the group
        agg.date = m_allData[end - 1].date;
        m_data.append(agg);
    }
    calculateMovingAverages();
}

void KLineWidget::editShapeProperties(int index)
{
    if (index < 0 || index >= m_shapes.size()) return;
    Shape &s = m_shapes[index];

    ShapeDialog dlg(this);
    dlg.setShapeName(s.name);
    dlg.setShapeColor(s.color.isValid() ? s.color : Qt::white);

    if (dlg.exec() == QDialog::Accepted) {
        s.name = dlg.getShapeName();
        s.color = dlg.getShapeColor();
        // For text shapes, update the text content from name
        if (s.type == Shape_Text) {
            s.text = s.name;
        }
        update();
    }
}

// helper: convert screen coord to data coord (candle index & price)
void KLineWidget::screenToDataCoord(const QPointF &screenPt, int &candleIdx, double &price)
{
    // 没数据时返回默认值，防止 qBound 崩
    if (m_data.isEmpty()) {
        candleIdx = 0;
        price = 0.0;
        return;
    }
    QRect mr = mainChartRect();
    double totalPer = (m_candleWidth * m_scale) + m_gap;

    // convert screen x to candle index
    if (totalPer > 0 && mr.width() > 0) {
        double relX = screenPt.x() - mr.left();
        int relIdx = int(relX / totalPer + 0.5);
        candleIdx = m_startIndex + relIdx;
        candleIdx = qBound(0, candleIdx, m_data.size() - 1);
    } else {
        candleIdx = m_startIndex;
    }

    // convert screen y to price
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    if (mr.height() > 0) {
        double ratio = double(mr.bottom() - screenPt.y()) / double(mr.height());
        price = m_minPrice + ratio * priceRange;
    } else {
        price = m_minPrice;
    }
}

// helper: convert data coord (candle index & price) to screen coord
void KLineWidget::dataCoordToScreen(int candleIdx, double price, QPointF &screenPt)
{
    QRect mr = mainChartRect();
    double tp = totalPer();
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;

    double xCenter = mr.left() + (candleIdx - m_startIndex) * tp + tp / 2.0;
    double ratio = (price - m_minPrice) / priceRange;
    double y = mr.bottom() - ratio * mr.height();
    screenPt = QPointF(xCenter, y);
}

double KLineWidget::candleBodyWidth() const {
    return (m_candleWidth * m_scale);
}

double KLineWidget::pointToLineDist(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    double l2 = dx*dx + dy*dy;
    if (l2 <= 1e-12) return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    // formula: distance from point p to line through a and b
    double dist = qAbs((dy*(p.x()-a.x()) - dx*(p.y()-a.y())) / qSqrt(l2));
    return dist;
}

// helper: distance from point to ray (starting from a, extending through b)
double KLineWidget::pointToRayDist(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    double l2 = dx*dx + dy*dy;
    if (l2 <= 1e-12) return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));

    // parameter t along ray from a to b
    double t = ((p.x()-a.x())*dx + (p.y()-a.y())*dy) / l2;

    if (t < 0.0) {
        // point is before ray start, distance to start point
        return qSqrt((p.x()-a.x())*(p.x()-a.x()) + (p.y()-a.y())*(p.y()-a.y()));
    }

    // project point onto ray
    double projx = a.x() + t * dx;
    double projy = a.y() + t * dy;
    return qSqrt((p.x()-projx)*(p.x()-projx) + (p.y()-projy)*(p.y()-projy));
}

void KLineWidget::calculateMovingAverages()
{
    m_ma5.clear();
    m_ma10.clear();
    m_ma20.clear();
    m_ma60.clear();
    
    if (m_data.isEmpty()) return;
    
    // Calculate 5-day MA
    for (int i = 0; i < m_data.size(); ++i) {
        int start = qMax(0, i - 4);
        double sum = 0;
        for (int j = start; j <= i; ++j) {
            sum += m_data[j].close;
        }
        m_ma5.append(sum / (i - start + 1));
    }
    
    // Calculate 10-day MA
    for (int i = 0; i < m_data.size(); ++i) {
        int start = qMax(0, i - 9);
        double sum = 0;
        for (int j = start; j <= i; ++j) {
            sum += m_data[j].close;
        }
        m_ma10.append(sum / (i - start + 1));
    }
    
    // Calculate 20-day MA
    for (int i = 0; i < m_data.size(); ++i) {
        int start = qMax(0, i - 19);
        double sum = 0;
        for (int j = start; j <= i; ++j) {
            sum += m_data[j].close;
        }
        m_ma20.append(sum / (i - start + 1));
    }
    
    // Calculate 60-day MA
    for (int i = 0; i < m_data.size(); ++i) {
        int start = qMax(0, i - 59);
        double sum = 0;
        for (int j = start; j <= i; ++j) {
            sum += m_data[j].close;
        }
        m_ma60.append(sum / (i - start + 1));
    }
}

void KLineWidget::drawMovingAverages(QPainter &p)
{
    // ensure moving averages are calculated if not already
    if (m_ma5.isEmpty() && !m_data.isEmpty()) {
        const_cast<KLineWidget*>(this)->calculateMovingAverages();
    }
    
    const int marginLeft = 40;
    const int marginTop = 10;
    const int marginBottom = 20;
    QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
    double priceRange = m_maxPrice - m_minPrice;
    if (priceRange <= 0) return;

    auto priceToY = [&](double price) {
        double ratio = (price - m_minPrice) / priceRange;
        return mainRect.bottom() - ratio * mainRect.height();
    };

    double totalPer = (m_candleWidth * m_scale) + m_gap;
    int visCount = visibleCount();
    int contentLeft = mainRect.left();
    // Draw MA5 (yellow)
    if (m_showMA5 && !m_ma5.isEmpty()) {
        p.setPen(QPen(QColor(255, 255, 0), 1));
        QPointF lastPoint;
        for (int i = 0; i < visCount && m_startIndex + i < m_data.size(); ++i) {
            int idx = m_startIndex + i;
            if (idx >= 0 && idx < m_ma5.size()) {
                double x = contentLeft + i * totalPer + totalPer / 2.0;
                double y = priceToY(m_ma5[idx]);
                QPointF point(x, y);
                if (i > 0) p.drawLine(lastPoint, point);
                lastPoint = point;
            }
        }
    }
    
    // Draw MA10 (white)
    if (m_showMA10 && !m_ma10.isEmpty()) {
        p.setPen(QPen(QColor(255, 255, 255), 1));
        QPointF lastPoint;
        for (int i = 0; i < visCount && m_startIndex + i < m_data.size(); ++i) {
            int idx = m_startIndex + i;
            if (idx >= 0 && idx < m_ma10.size()) {
                double x = contentLeft + i * totalPer + totalPer / 2.0;
                double y = priceToY(m_ma10[idx]);
                QPointF point(x, y);
                if (i > 0) {
                    p.drawLine(lastPoint, point);
                }
                lastPoint = point;
            }
        }
    }
    
    // Draw MA20 (cyan)
    if (m_showMA20 && !m_ma20.isEmpty()) {
        p.setPen(QPen(QColor(0, 255, 255), 1));
        QPointF lastPoint;
        for (int i = 0; i < visCount && m_startIndex + i < m_data.size(); ++i) {
            int idx = m_startIndex + i;
            if (idx >= 0 && idx < m_ma20.size()) {
                double x = contentLeft + i * totalPer + totalPer / 2.0;
                double y = priceToY(m_ma20[idx]);
                QPointF point(x, y);
                if (i > 0) {
                    p.drawLine(lastPoint, point);
                }
                lastPoint = point;
            }
        }
    }
    
    // Draw MA60 (magenta)
    if (m_showMA60 && !m_ma60.isEmpty()) {
        p.setPen(QPen(QColor(255, 0, 255), 1));
        QPointF lastPoint;
        for (int i = 0; i < visCount && m_startIndex + i < m_data.size(); ++i) {
            int idx = m_startIndex + i;
            if (idx >= 0 && idx < m_ma60.size()) {
                double x = contentLeft + i * totalPer + totalPer / 2.0;
                double y = priceToY(m_ma60[idx]);
                QPointF point(x, y);
                if (i > 0) {
                    p.drawLine(lastPoint, point);
                }
                lastPoint = point;
            }
        }
    }
}

QRect KLineWidget::mainChartRect() const {
    const int marginLeft = 5;
    const int marginTop = 10;
    const int marginBottom = 20;
    const int priceAxisWidth = 20;
    return QRect(marginLeft + priceAxisWidth, marginTop, width() - marginLeft - priceAxisWidth - 10 - m_rightPadding, height() - marginTop - marginBottom);
}

double KLineWidget::totalPer() const {
    return (m_candleWidth * m_scale) + m_gap;
}

int KLineWidget::candleCenterXForIndex(int index) const {
    QRect r = mainChartRect();
    double tp = totalPer();
    if (tp <= 0) return r.left();
    int rel = index - m_startIndex;
    double xCenter = r.left() + rel * tp + tp / 2.0;
    return int(xCenter + 0.5);
}

int KLineWidget::indexForScreenX(int screenX) const {
    QRect r = mainChartRect();
    double tp = totalPer();
    if (tp <= 0) return m_startIndex;
    int rel = int((screenX - r.left()) / tp + 0.5);
    int idx = m_startIndex + rel;
    idx = qBound(0, idx, m_data.size()-1);
    return idx;
}

void KLineWidget::snapCrosshairTo(const QPointF &pos)
{
    if (m_data.isEmpty()) return;
    QRect r = mainChartRect();
    double tp = totalPer();
    // default keep Y
    double y = pos.y();
    int idx = m_startIndex;
    if (tp > 0 && r.width() > 0) {
        int rel = int((pos.x() - r.left()) / tp + 0.5);
        idx = qBound(0, m_startIndex + rel, m_data.size()-1);
        double xCenter = r.left() + (idx - m_startIndex) * tp + tp / 2.0;
        m_crosshairPos.setX(int(xCenter + 0.5));
    }
    m_crosshairPos.setY(int(y));
    double priceRange = m_maxPrice - m_minPrice; if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    double ratio = double(r.bottom() - m_crosshairPos.y()) / double(r.height());
    double price = m_minPrice + ratio * priceRange;
    emit crosshairIndexChanged(idx);
    emit crosshairPriceChanged(price, idx);
    emit crosshairScreenXChanged(candleCenterXForIndex(idx));
}

void KLineWidget::updateRealtimeCandle(const Candle &c)
{
    if (m_data.isEmpty()) {
        m_data.append(c);
        m_allData.append(c);
        updateRange();
        calculateMovingAverages();
        emit dataAggregated(m_data);
        update();
        return;
    }

    // 判断是同一根 K 线更新还是新 K 线追加
    if (c.date == m_data.last().date) {
        // 替换最后一条
        m_data.last() = c;
        if (!m_allData.isEmpty() && m_allData.last().date == c.date) {
            m_allData.last() = c;
        } else {
            m_allData.append(c);
        }
    } else if (c.date > m_data.last().date) {
        // 追加新 K 线
        m_data.append(c);
        m_allData.append(c);
        // 自动滚动到最新
        int visCount = visibleCount();
        if (m_startIndex + visCount < m_data.size()) {
            m_startIndex = qMax(0, m_data.size() - visCount);
        }
    } else {
        return; // 旧数据忽略
    }

    updateRange();
    calculateMovingAverages();
    m_lastPrice = c.close;
    m_lastOpen = c.open;
    updateRealtimeLabel();

    emit dataAggregated(m_data);
    emit viewportChanged(m_startIndex, visibleCount());
    emit layoutChanged(m_startIndex, visibleCount(), totalPer(), candleBodyWidth(), mainChartRect());
    update();
}

void KLineWidget::setSymbol(const QString &s)
{
    m_symbol = s;
    updateRealtimeLabel();
}

void KLineWidget::setConnectionStatus(bool connected)
{
    m_connected = connected;
    updateRealtimeLabel();
}

void KLineWidget::updateRealtimeLabel()
{
    if (!m_realtimeLabel) return;
    if (m_lastPrice == 0) {
        m_realtimeLabel->setVisible(false);
        return;
    }

    // 构建显示文本：● ▲ 4508.02 +1.59
    double change = m_lastPrice - m_lastOpen;
    QString arrow;
    QColor priceColor;
    if (change >= 0) {
        arrow = QStringLiteral("▲");
        priceColor = QColor(220, 20, 60); // 红色（涨）
    } else {
        arrow = QStringLiteral("▼");
        priceColor = QColor(0, 180, 0);   // 绿色（跌）
    }

    // 连接状态颜色
    QString dotColor = m_connected ? QStringLiteral("#00FF00") : QStringLiteral("#FF0000");

    QString text = QStringLiteral("<span style='color:%1;'>&#9679;</span> "
                                  "<span style='color:%2;'>%3 %4</span> "
                                  "<span style='color:%5;'>%6%7</span>")
        .arg(dotColor)
        .arg(priceColor.name())
        .arg(arrow)
        .arg(m_lastPrice, 0, 'f', 2)
        .arg(priceColor.name())
        .arg(change >= 0 ? "+" : "")
        .arg(change, 0, 'f', 2);

    m_realtimeLabel->setText(text);
    m_realtimeLabel->adjustSize();
    // 定位到右上角
    int labelW = m_realtimeLabel->width();
    int labelH = m_realtimeLabel->height();
    m_realtimeLabel->setGeometry(width() - labelW - 10, 10, labelW, labelH);
    m_realtimeLabel->raise();
    m_realtimeLabel->setVisible(true);
}
