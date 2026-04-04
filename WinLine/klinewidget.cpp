#include "klinewidget.h"
#include "shapedialog.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QPainterPath>
#include <QToolTip>
#include <QInputDialog>
#include <QKeyEvent>
#include <QColorDialog>

KLineWidget::KLineWidget(QWidget *parent)
    : QWidget(parent), m_minPrice(0), m_maxPrice(0), m_scale(1.0), m_candleWidth(6.0), m_gap(2.0), m_startIndex(0), m_panning(false), m_crosshairVisible(false), m_rightPadding(80), m_timeframe(KLineWidget::TF_1m), m_baseMinutes(1)
    , m_toolMode(Tool_None), m_selectedShapeIndex(-1), m_drawing(false), m_draggingEndpoint(0), m_movingShape(false), m_nextShapeId(1)
{
    setMinimumSize(600, 500);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// simplified helper: distance from point to segment
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

static int hitTestEndpoint(const KLineWidget::Shape &s, const QPointF &pt, qreal tol = 6.0) {
    if (qSqrt((s.p1.x()-pt.x())*(s.p1.x()-pt.x()) + (s.p1.y()-pt.y())*(s.p1.y()-pt.y())) <= tol) return 1;
    if (qSqrt((s.p2.x()-pt.x())*(s.p2.x()-pt.x()) + (s.p2.y()-pt.y())*(s.p2.y()-pt.y())) <= tol) return 2;
    return 0;
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
    int w = width() - 50 - m_rightPadding; // leave right padding
    double totalPer = (m_candleWidth * m_scale) + m_gap;
    if (totalPer <= 0) return 1;
    int cnt = qMax(1, int(w / totalPer));
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

    // Drawing tool handling
    if (m_toolMode != Tool_None) {
        if (event->button() == Qt::LeftButton) {
            // ensure cross cursor remains while drawing
            setCursor(Qt::CrossCursor);
            QPointF pt = event->pos();
            // endpoint hit
            for (int i = 0; i < m_shapes.size(); ++i) {
                int hit = hitTestEndpoint(m_shapes[i], pt);
                if (hit) {
                    m_selectedShapeIndex = i;
                    m_draggingEndpoint = hit;
                    m_movingShape = false;
                    m_drawing = false;
                    m_lastMousePos = event->pos();
                    update();
                    return;
                }
            }
            // whole-shape hit
            for (int i = 0; i < m_shapes.size(); ++i) {
                const Shape &s = m_shapes[i];
                if (pointSegDist(pt, s.p1, s.p2) < 6.0) {
                    m_selectedShapeIndex = i;
                    m_movingShape = true;
                    m_draggingEndpoint = 0;
                    m_lastMousePos = event->pos();
                    update();
                    return;
                }
            }
            // start new shape: convert screen coord to data coord
            Shape ns; ns.type = (ShapeType)(m_toolMode - 1); ns.p1 = pt; ns.p2 = pt; ns.selected = true;
            ns.text.clear();
            ns.id = m_nextShapeId++;
            ns.name = QString("shape_%1").arg(ns.id);
            ns.color = Qt::white;
            // convert screen to data coordinates
            screenToDataCoord(pt, ns.candleIdx1, ns.price1);
            ns.candleIdx2 = ns.candleIdx1; ns.price2 = ns.price1;
            
            // For text shapes, immediately open the dialog to set text and color
            if (ns.type == Shape_Text) {
                ShapeDialog dlg(this);
                dlg.setShapeName(ns.name);
                dlg.setShapeColor(ns.color);
                if (dlg.exec() == QDialog::Accepted) {
                    ns.name = dlg.getShapeName();
                    ns.color = dlg.getShapeColor();
                    ns.text = ns.name; // Use name as text content
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
                s.p1 = event->pos();
                update();
                return;
            }
            if (m_draggingEndpoint == 2) {
                screenToDataCoord(event->pos(), s.candleIdx2, s.price2);
                s.p2 = event->pos();
                update();
                return;
            }
            if (m_movingShape && (event->buttons() & Qt::LeftButton)) {
                QPoint delta = event->pos() - m_lastMousePos;
                s.p1 += delta; s.p2 += delta;
                // update data coords too
                screenToDataCoord(s.p1, s.candleIdx1, s.price1);
                screenToDataCoord(s.p2, s.candleIdx2, s.price2);
                m_lastMousePos = event->pos();
                update();
                return;
            }
            if (m_drawing) {
                screenToDataCoord(event->pos(), s.candleIdx2, s.price2);
                s.p2 = event->pos();
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
            }
        }
    }
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
            int idx = m_startIndex + int((m_crosshairPos.x() - mainRect.left()) / totalPer + 0.5);
            idx = qBound(0, idx, m_data.size()-1);
            emit crosshairPriceChanged(price, idx);
            emit crosshairIndexChanged(idx);
        }
        update();
    }

    // tooltip logic
    if (!m_data.isEmpty()) {
        int marginLeft = 40;
        double totalPer = (m_candleWidth * m_scale) + m_gap;
        if (totalPer > 0) {
            int relX = event->pos().x() - marginLeft;
            int relIdx = int((double)relX / totalPer + 0.5);
            int idx = m_startIndex + relIdx;
            if (idx >= 0 && idx < m_data.size()) {
                const Candle &c = m_data.at(idx);
                double x = marginLeft + relIdx * totalPer;
                double bodyW = m_candleWidth * m_scale;
                double bodyLeft = x + (totalPer - bodyW) / 2.0;
                double bodyRight = bodyLeft + bodyW;
                int marginTop = 10;
                int marginBottom = 20;
                QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
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
                emit crosshairPriceChanged(price, idx);
                emit crosshairIndexChanged(idx);
            }
        }
        update();
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
    const int marginLeft = 40;
    const int marginTop = 10;
    const int marginBottom = 20;
    QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);

    // draw price scale (Y-axis) - brighter color with higher density
    {
        const int marginLeft = 40;
        const int marginTop = 10;
        const int marginBottom = 20;
        QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
        double priceRange = m_maxPrice - m_minPrice;
        if (priceRange > 0) {
            // higher density: more tick marks
            double step = priceRange / 20.0;  // 20 divisions for higher density
            if (step <= 0) step = 1.0;

            p.setFont(QFont("Arial", 8));

            for (double price = m_minPrice; price <= m_maxPrice; price += step) {
                double ratio = (price - m_minPrice) / priceRange;
                int y = mainRect.bottom() - ratio * mainRect.height();

                // draw tick mark - bright cyan color
                p.setPen(QPen(QColor(0, 200, 255), 2));
                p.drawLine(mainRect.left() - 5, y, mainRect.left() - 2, y);

                // draw price label - bright yellow
                p.setPen(QPen(QColor(255, 255, 0), 1));
                QString priceStr = QString::number(price, 'f', 2);
                QFontMetrics fm(p.font());
                int textWidth = fm.horizontalAdvance(priceStr);
                p.drawText(mainRect.left() - textWidth - 8, y - 4, priceStr);
            }
        }
    }

    // draw time scale (X-axis) - brighter color with higher density
    {
        const int marginLeft = 40;
        const int marginBottom = 20;
        double totalPer = (m_candleWidth * m_scale) + m_gap;
        if (totalPer > 0 && !m_data.isEmpty()) {
            int visCount = visibleCount();

            // higher density: more tick marks - reduce interval for denser ticks
            int tickInterval = qMax(1, visCount / 20);  // ~20 major ticks visible instead of 10

            p.setFont(QFont("Arial", 8));

            for (int i = m_startIndex; i < m_startIndex + visCount && i < m_data.size(); ++i) {
                if ((i - m_startIndex) % tickInterval == 0) {
                    double x = marginLeft + (i - m_startIndex) * totalPer + totalPer / 2.0;
                    int y = height() - marginBottom;

                    // draw tick mark - bright cyan color
                    p.setPen(QPen(QColor(0, 200, 255), 2));
                    p.drawLine(x, y + 2, x, y + 5);

                    // draw time label - bright yellow
                    p.setPen(QPen(QColor(255, 255, 0), 1));
                    QString timeStr;
                    if (i < m_data.size()) {
                        if (m_timeframe == TF_DAILY) {
                            // Standard format: YYYY-MM-DD
                            timeStr = m_data[i].date.toString("yyyy-MM-dd");
                        } else {
                            // Standard format: YYYY-MM-DD HH:mm
                            timeStr = m_data[i].date.toString("yyyy-MM-dd HH:mm");
                        }
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
            QPointF bestPt = screenP1;
            if (intersectLines(screenP1, screenP2, e1, e2, ip)) {
                // check within edge
                if (ip.x() >= qMin(e1.x(), e2.x()) - 1e-6 && ip.x() <= qMax(e1.x(), e2.x()) + 1e-6 &&
                    ip.y() >= qMin(e1.y(), e2.y()) - 1e-6 && ip.y() <= qMax(e1.y(), e2.y()) + 1e-6) {
                    double t = qFuzzyIsNull(ln.dx()) ? (ip.y()-screenP1.y())/ln.dy() : (ip.x()-screenP1.x())/ln.dx();
                    if (t > bestT) { bestT = t; bestPt = ip; }
                }
            }
            if (intersectLines(screenP1, screenP2, e2, e3, ip)) {
                if (ip.x() >= qMin(e2.x(), e3.x()) - 1e-6 && ip.x() <= qMax(e2.x(), e3.x()) + 1e-6 &&
                    ip.y() >= qMin(e2.y(), e3.y()) - 1e-6 && ip.y() <= qMax(e2.y(), e3.y()) + 1e-6) {
                    double t = qFuzzyIsNull(ln.dx()) ? (ip.y()-screenP1.y())/ln.dy() : (ip.x()-screenP1.x())/ln.dx();
                    if (t > bestT) { bestT = t; bestPt = ip; }
                }
            }
            if (intersectLines(screenP1, screenP2, e3, e4, ip)) {
                if (ip.x() >= qMin(e3.x(), e4.x()) - 1e-6 && ip.x() <= qMax(e3.x(), e4.x()) + 1e-6 &&
                    ip.y() >= qMin(e3.y(), e4.y()) - 1e-6 && ip.y() <= qMax(e3.y(), e4.y()) + 1e-6) {
                    double t = qFuzzyIsNull(ln.dx()) ? (ip.y()-screenP1.y())/ln.dy() : (ip.x()-screenP1.x())/ln.dx();
                    if (t > bestT) { bestT = t; bestPt = ip; }
                }
            }
            if (intersectLines(screenP1, screenP2, e4, e1, ip)) {
                if (ip.x() >= qMin(e4.x(), e1.x()) - 1e-6 && ip.x() <= qMax(e4.x(), e1.x()) + 1e-6 &&
                    ip.y() >= qMin(e4.y(), e1.y()) - 1e-6 && ip.y() <= qMax(e4.y(), e1.y()) + 1e-6) {
                    double t = qFuzzyIsNull(ln.dx()) ? (ip.y()-screenP1.y())/ln.dy() : (ip.x()-screenP1.x())/ln.dx();
                    if (t > bestT) { bestT = t; bestPt = ip; }
                }
            }
            p.drawLine(screenP1, bestPt);
        } else if (s.type == Shape_GestureUp) {
            p.drawLine(screenP1, screenP2);
            QPointF h = screenP2 - screenP1; h = QPointF(-h.y(), h.x());
            p.drawLine(screenP2, screenP2 - h*0.2);
            p.drawLine(screenP2, screenP2 + h*0.2);
        } else if (s.type == Shape_GestureDown) {
            p.drawLine(screenP1, screenP2);
            QPointF h = screenP2 - screenP1; h = QPointF(-h.y(), h.x());
            p.drawLine(screenP1, screenP1 - h*0.2);
            p.drawLine(screenP1, screenP1 + h*0.2);
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
    update();
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
        int contentLeft = 40;
        int contentWidth = qMax(1, width() - contentLeft - m_rightPadding);
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
        update();
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
    const int marginLeft = 40;
    const int marginTop = 10;
    const int marginBottom = 20;
    QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
    double totalPer = (m_candleWidth * m_scale) + m_gap;

    // convert screen x to candle index
    if (totalPer > 0 && mainRect.width() > 0) {
        double relX = screenPt.x() - mainRect.left();
        int relIdx = int(relX / totalPer + 0.5);
        candleIdx = m_startIndex + relIdx;
        candleIdx = qBound(0, candleIdx, m_data.size() - 1);
    } else {
        candleIdx = m_startIndex;
    }

    // convert screen y to price
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;
    if (mainRect.height() > 0) {
        double ratio = double(mainRect.bottom() - screenPt.y()) / double(mainRect.height());
        price = m_minPrice + ratio * priceRange;
    } else {
        price = m_minPrice;
    }
}

// helper: convert data coord (candle index & price) to screen coord
void KLineWidget::dataCoordToScreen(int candleIdx, double price, QPointF &screenPt)
{
    const int marginLeft = 40;
    const int marginTop = 10;
    const int marginBottom = 20;
    QRect mainRect(marginLeft, marginTop, width() - marginLeft - 10 - m_rightPadding, height() - marginTop - marginBottom);
    double totalPer = (m_candleWidth * m_scale) + m_gap;
    double priceRange = m_maxPrice - m_minPrice;
    if (qFuzzyCompare(priceRange, 0.0)) priceRange = 1.0;

    // convert candle index to screen x
    double x = mainRect.left() + (candleIdx - m_startIndex) * totalPer;

    // convert price to screen y
    double ratio = (price - m_minPrice) / priceRange;
    double y = mainRect.bottom() - ratio * mainRect.height();

    screenPt = QPointF(x, y);
}

// helper: distance from point to infinite line
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
    
    double totalPer = (m_candleWidth * m_scale) + m_gap;
    int visCount = visibleCount();
    
    auto priceToY = [&](double price) {
        double ratio = (price - m_minPrice) / priceRange;
        return mainRect.bottom() - ratio * mainRect.height();
    };
    
    // Draw MA5 (yellow)
    if (m_showMA5 && !m_ma5.isEmpty()) {
        p.setPen(QPen(QColor(255, 255, 0), 1));
        QPointF lastPoint;
        for (int i = 0; i < visCount && m_startIndex + i < m_data.size(); ++i) {
            int idx = m_startIndex + i;
            if (idx >= 0 && idx < m_ma5.size()) {
                double x = marginLeft + i * totalPer + totalPer / 2.0;
                double y = priceToY(m_ma5[idx]);
                QPointF point(x, y);
                if (i > 0) {
                    p.drawLine(lastPoint, point);
                }
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
                double x = marginLeft + i * totalPer + totalPer / 2.0;
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
                double x = marginLeft + i * totalPer + totalPer / 2.0;
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
                double x = marginLeft + i * totalPer + totalPer / 2.0;
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
