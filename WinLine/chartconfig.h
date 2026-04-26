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
};
