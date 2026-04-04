#pragma once

#include <QWidget>
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
    enum ToolMode { Tool_None = 0, Tool_Line, Tool_Trend, Tool_GestureUp, Tool_GestureDown, Tool_Text };
    enum ShapeType { Shape_Line = 0, Shape_Trend, Shape_GestureUp, Shape_GestureDown, Shape_Text };
    struct Shape {
        ShapeType type;
        QPointF p1;     // screen coords (for temp use during drawing)
        QPointF p2;     // screen coords (for temp use during drawing)
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
    };

    void setToolMode(ToolMode m);
    void deleteSelectedShape();
    void clearShapes();
    void editShapeProperties(int index);
    void screenToDataCoord(const QPointF &screenPt, int &candleIdx, double &price);
    void dataCoordToScreen(int candleIdx, double price, QPointF &screenPt);
    double pointToLineDist(const QPointF &p, const QPointF &a, const QPointF &b);
    double pointToRayDist(const QPointF &p, const QPointF &a, const QPointF &b);

signals:
    void dataAggregated(const QVector<Candle> &agg);
    void viewportChanged(int startIndex, int count);
    void crosshairPriceChanged(double price, int index);
    void crosshairIndexChanged(int index);

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
};
