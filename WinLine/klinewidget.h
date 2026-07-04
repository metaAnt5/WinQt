#pragma once

#include <QWidget>
#include <QLabel>
#include <QVector>
#include <QDateTime>
#include <QString>
#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QMap>
#include <QSharedPointer>
#include "shapes/shape.h"

struct Candle {
    QDateTime date;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

Q_DECLARE_METATYPE(Candle)

class KLineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KLineWidget(QWidget *parent = nullptr);
    void setData(const QVector<Candle> &data);
    void setData(const QVector<Candle> &data, int baseMinutes);

    enum Timeframe {TF_1m = 1, TF_5m = 5, TF_15m = 15, TF_30m = 30, TF_60m = 60, TF_H4 = 240, TF_DAILY = 1440, TF_W1 = 10080, TF_MN = 43200};
    void setTimeframe(Timeframe tf);

    // Drawing tools
    enum ToolMode { Tool_None = 0, Tool_Line, Tool_Trend, Tool_UpTriangle, Tool_DownTriangle,
                    Tool_Fixed };

    void setToolMode(ToolMode m);
    const QVector<QSharedPointer<Shape>>& shapes() const { return m_shapes; }
    int selectedShapeIndex() const { return m_selectedShapeIndex; }
    void setShapes(const QVector<QSharedPointer<Shape>> &shapes) { m_shapes = shapes; m_selectedShapeIndex = -1; update(); }
    int addShape(QSharedPointer<Shape> s);
    void deleteSelectedShape();
    void clearShapes();

    // Loading overlay
    void showLoading(const QString &msg = QStringLiteral("Loading..."));
    void hideLoading();
    void screenToDataCoord(const QPointF &screenPt, double &candleIdx, double &price) const;
    void dataCoordToScreen(double candleIdx, double price, QPointF &screenPt) const;
    // 兼容旧接口，现在转发到 shapes 命名空间的几何函数
    double pointToLineDist(const QPointF &p, const QPointF &a, const QPointF &b) { return ::pointToLineDist2(p, a, b); }
    double pointToRayDist(const QPointF &p, const QPointF &a, const QPointF &b) { return ::pointToRayDist2(p, a, b); }

    // Layout / mapping helpers so other panes can align to the same candle centers
    QRect mainChartRect() const;
    double totalPer() const;
    int candleCenterXForIndex(int index) const;
    int indexForScreenX(int screenX) const;
    double candleBodyWidth() const;

    // Fixed shape helpers
    QPointF screenToNorm(const QPoint &screenPt) const;
    QPoint normToScreen(double normX, double normY) const;
    // Convert data coordinate (candle index, price) to normalized coordinate (0..1)
    void dataToNorm(int candleIdx, double price, double &normX, double &normY) const;

    // Save/Load shapes
    QString shapesFilePath() const;
    void saveShapes();
    void loadShapes();

    // Realtime data update
    void updateRealtimeCandle(const Candle &c);
    void setSymbol(const QString &s);
    void setConnectionStatus(bool connected);

    // Accessors for LuaEngine
    const QVector<Candle>& allData() const { return m_data; }
    const QString& symbol() const { return m_symbol; }
    int baseMinutes() const { return m_baseMinutes; }
    Timeframe timeframe() const { return m_timeframe; }

    // Helper: find candle index by time (binary search on m_data)
    int findCandleIndexByTime(const QDateTime &time) const;

    // 供 shapes 子类使用的内部数据（公开给 shapes/ 目录访问）
    const QVector<Candle>& data() const { return m_data; }
    int visibleCount() const;
    double m_minPrice = 0;
    double m_maxPrice = 0;
    double m_scale = 1.0;
    int m_startIndex = 0;

Q_SIGNALS:
    void crosshairIndexChanged(int index);
    void crosshairPriceChanged(double price, int index);
    void crosshairScreenXChanged(int screenX);
    void dataAggregated(const QVector<Candle> &data);
    void viewportChanged(int startIndex, int visibleCount);
    // Emit when viewport or layout (spacing) changes so indicators can align using the same mapping
    void layoutChanged(int startIndex, int visibleCount, double totalPer, double candleBodyWidth, QRect mainChartRect);
    // Emit when a shape is selected (index), for updating property panel
    void shapeSelected(int index);

    // Emit when shapes are saved to file (for Lua script engine cache refresh)
    void shapesSaved(const QString &symbol, int timeframe);

    // Emit when a realtime candle update arrives (for Lua script engine)
    void candleUpdated(const Candle &candle, bool isNewBar);

    // Emit when a shape is double-clicked (open properties dialog)
    void shapeDoubleClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QVector<Candle> m_data;    // displayed data (single source of truth)
    void updateRange();

    // interaction
    double m_candleWidth = 6.0;     // base candle width (before scale)
    double m_gap = 2.0;             // gap between candles
    QPoint m_lastMousePos;
    bool m_panning = false;

    // crosshair
    bool m_crosshairVisible = false;
    QPoint m_crosshairPos;

    // drawing tools
    ToolMode m_toolMode = Tool_None;
    QVector<QSharedPointer<Shape>> m_shapes;
    int m_selectedShapeIndex = -1;
    bool m_drawing = false;
    int m_draggingEndpoint = 0; // 0 = none, 1 = p1, 2 = p2
    bool m_movingShape = false;

    void ensureStartIndexVisible();

    // layout
    int m_rightPadding = 80;
    
    // Moving Average settings
    bool m_showMA5 = true;
    bool m_showMA10 = true;
    bool m_showMA20 = true;
    bool m_showMA60 = true;
    
    QVector<double> m_ma5;   // 5-day moving average
    QVector<double> m_ma10;  // 10-day moving average
    QVector<double> m_ma20;  // 20-day moving average
    QVector<double> m_ma60;  // 60-day moving average
    
    void calculateMovingAverages();
    void drawMovingAverages(QPainter &p); // extra right blank space so K lines don't touch edge
    Timeframe m_timeframe = TF_1m;
    int m_baseMinutes = 1; // base timeframe of m_data in minutes
    int m_nextShapeId = 1; // incremental id for shapes

    void snapCrosshairTo(const QPointF &pos);

    // Loading overlay
    QLabel *m_loadingLabel = nullptr;

    // No data overlay
    QLabel *m_noDataLabel = nullptr;
    void showNoData();
    void hideNoData();

    // Realtime price label & data
    QLabel *m_realtimeLabel = nullptr;
    double m_lastPrice = 0;
    double m_lastOpen = 0;
    double m_lastHigh = 0;
    double m_lastLow = 0;
    double m_lastVolume = 0;
    double m_prevClose = 0;   // 上一根K线的收盘价（用于计算涨跌）
    bool m_connected = false;
    QString m_symbol;

    void updateRealtimeLabel();

    // 绘制十字光标悬浮信息框（鼠标位置）
    void drawCrosshairInfoBox(QPainter &p, int candleIdx, double price);
};
