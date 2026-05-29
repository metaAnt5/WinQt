#pragma once

#include <QWidget>
#include <QLabel>
#include <QVector>
#include <QDateTime>
#include <QString>
#include <QColor>


struct Candle {
    QDateTime date;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

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
    enum ToolMode { Tool_None = 0, Tool_Line, Tool_Trend, Tool_GestureUp, Tool_GestureDown, Tool_Text, Tool_HLine, Tool_VLine,
                    Tool_TradeBuy, Tool_TradeSell, Tool_TradeShort, Tool_TradeCover };
    enum ShapeType { Shape_Line = 0, Shape_Trend, Shape_GestureUp, Shape_GestureDown, Shape_Text, Shape_HLine, Shape_VLine,
                     Shape_TradeBuy, Shape_TradeSell, Shape_TradeShort, Shape_TradeCover };
    struct Shape {
        ShapeType type;
        QString text;
        bool selected;
        QString name;   // user-assigned name
        QColor color;   // user-selected color
        int id;         // unique identifier
        // data coordinates: stick to K-line when zooming/panning
        int candleIdx1; // candle index for p1
        double price1;  // price for p1
        int candleIdx2; // candle index for p2
        double price2;  // price for p2
        // trade-specific fields
        double tradePrice = 0.0;   // 成交价
        QDateTime tradeTime;       // 成交时间
        int quantity = 1;          // 数量
        double profit = 0.0;       // 平仓盈亏
    };

    void setToolMode(ToolMode m);
    const QVector<Shape>& shapes() const { return m_shapes; }
    void setShapes(const QVector<Shape> &shapes) { m_shapes = shapes; m_selectedShapeIndex = -1; update(); }
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

    // Realtime data update
    void updateRealtimeCandle(const Candle &c);
    void setSymbol(const QString &s);
    void setConnectionStatus(bool connected);

Q_SIGNALS:
    void crosshairIndexChanged(int index);
    void crosshairPriceChanged(double price, int index);
    void crosshairScreenXChanged(int screenX);
    void dataAggregated(const QVector<Candle> &data);
    void viewportChanged(int startIndex, int visibleCount);
    // Emit when viewport or layout (spacing) changes so indicators can align using the same mapping
    void layoutChanged(int startIndex, int visibleCount, double totalPer, double candleBodyWidth, QRect mainChartRect);

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
    QVector<Candle> m_allData; // original data (lowest timeframe)
    QVector<Candle> m_data;    // displayed (aggregated) data
    double m_minPrice;
    double m_maxPrice;
    void updateRange();
    void aggregateData(int factor);

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
    int m_baseMinutes; // base timeframe of m_allData in minutes
    int m_nextShapeId; // incremental id for shapes

    void emitCrosshairSignals();
    void snapCrosshairTo(const QPointF &pos);

    // Loading overlay
    QLabel *m_loadingLabel = nullptr;

    // Realtime price label
    QLabel *m_realtimeLabel = nullptr;
    double m_lastPrice = 0;
    double m_lastOpen = 0;
    bool m_connected = false;
    QString m_symbol;

    void updateRealtimeLabel();
};
