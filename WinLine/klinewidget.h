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
    // Drawing tools (simplified)
    enum ToolMode { Tool_None = 0, Tool_Line, Tool_Trend, Tool_UpTriangle, Tool_DownTriangle,
                    Tool_FixedDot, Tool_FixedTriangle };
    enum ShapeType { Shape_Line = 0, Shape_Trend, Shape_UpTriangle, Shape_DownTriangle,
                     Shape_FixedDot = 100, Shape_FixedTriangle };
    // Shape attachment category
    enum ShapeAttachment { Attach_KLineBound = 0,  // K-line bound, moves with zoom/pan
                           Attach_Fixed };          // Fixed position, does not move
    struct Shape {
        ShapeType type;
        QString text;
        bool selected;
        QString name;   // user-assigned name
        QColor color;   // user-selected color
        int id;         // unique identifier
        // attachment category
        ShapeAttachment attachment = Attach_KLineBound;
        // data coordinates: stick to K-line when zooming/panning (for KLineBound)
        int candleIdx1; // candle index for p1
        double price1;  // price for p1
        int candleIdx2; // candle index for p2
        double price2;  // price for p2
        // normalized coordinates 0..1 relative to chart area (for Fixed)
        double normX = 0.5;
        double normY = 0.5;
        // script ownership: 0 = user-created, >0 = child of shape with this id
        int ownerShapeId = 0;
        // trade-specific fields
        double tradePrice = 0.0;   // 成交价
        QDateTime tradeTime;       // 成交时间
        int quantity = 1;          // 数量
        double profit = 0.0;       // 平仓盈亏
        // script extension fields
        QString scriptName;        // 关联的 Lua 脚本名称（如 "ma_cross.lua"）
        QString scriptParams;      // 脚本参数（JSON 字符串，灵活扩展）
    };

    void setToolMode(ToolMode m);
    const QVector<Shape>& shapes() const { return m_shapes; }
    int selectedShapeIndex() const { return m_selectedShapeIndex; }
    void setShapes(const QVector<Shape> &shapes) { m_shapes = shapes; m_selectedShapeIndex = -1; update(); }
    // Adds a shape and assigns it a unique id; returns the shape id
    int addShape(const Shape &s);
    void deleteSelectedShape();
    void clearShapes();

    // Loading overlay
    void showLoading(const QString &msg = QStringLiteral("Loading..."));
    void hideLoading();
    void editShapeProperties(int index);
    void screenToDataCoord(const QPointF &screenPt, int &candleIdx, double &price);
    void dataCoordToScreen(int candleIdx, double price, QPointF &screenPt);
    double pointToLineDist(const QPointF &p, const QPointF &a, const QPointF &b);
    double pointToRayDist(const QPointF &p, const QPointF &a, const QPointF &b);

    // Layout / mapping helpers so other panes can align to the same candle centers
    QRect mainChartRect() const;
    double totalPer() const;
    int candleCenterXForIndex(int index) const;
    int indexForScreenX(int screenX) const;
    double candleBodyWidth() const;

    // Fixed shape helpers
    QPointF screenToNorm(const QPoint &screenPt) const;
    QPoint normToScreen(double normX, double normY) const;
    void drawFixedShapes(QPainter &p);

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

Q_SIGNALS:
    void crosshairIndexChanged(int index);
    void crosshairPriceChanged(double price, int index);
    void crosshairScreenXChanged(int screenX);
    void dataAggregated(const QVector<Candle> &data);
    void viewportChanged(int startIndex, int visibleCount);
    // Emit when viewport or layout (spacing) changes so indicators can align using the same mapping
    void layoutChanged(int startIndex, int visibleCount, double totalPer, double candleBodyWidth, QRect mainChartRect);
    // Emit when shapes or trades are modified
    void shapesChanged();
    // Emit when a shape is selected (index), for updating property panel
    void shapeSelected(int index);

    // Emit when shapes are loaded from file (for Lua script engine to load associated scripts)
    void shapesLoaded();

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
    double m_minPrice;
    double m_maxPrice;
    void updateRange();

    // interaction
    double m_scale;           // zoom scale for candle width
    double m_candleWidth;     // base candle width (before scale)
    double m_gap;             // gap between candles
    int m_startIndex;         // first displayed candle index
    QPoint m_lastMousePos;
    bool m_panning;

    // crosshair
    bool m_crosshairVisible;
    QPoint m_crosshairPos;

    // drawing tools
    ToolMode m_toolMode;
    QVector<Shape> m_shapes;
    int m_selectedShapeIndex;
    bool m_drawing;
    int m_draggingEndpoint; // 0 = none, 1 = p1, 2 = p2
    bool m_movingShape;

    int visibleCount() const;
    void ensureStartIndexVisible();

    // layout
    int m_rightPadding;
    
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
    Timeframe m_timeframe;
    int m_baseMinutes; // base timeframe of m_data in minutes
    int m_nextShapeId; // incremental id for shapes

    void emitCrosshairSignals();
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
