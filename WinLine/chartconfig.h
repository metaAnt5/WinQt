#pragma once
#include <QRect>

class ChartConfig {
public:
    static void setLayout(const QRect &mainRect, double totalPer, int startIndex, int visibleCount, double bodyWidth, int rightPadding);
    static QRect mainRect();
    static double totalPer();
    static int startIndex();
    static int visibleCount();
    static double bodyWidth();
    static int rightPadding();

    // 根据主图布局计算第 idx 根 K 线的中心 X 坐标，用于附图对齐
    static int candleCenterX(int idx);
};
