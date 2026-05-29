#include "chartconfig.h"

static QRect g_mainRect;
static double g_totalPer = 1.0;
static int g_startIndex = 0;
static int g_visibleCount = 0;
static double g_bodyWidth = 6.0;
static int g_rightPadding = 80;

void ChartConfig::setLayout(const QRect &mainRect, double totalPer, int startIndex, int visibleCnt, double bodyWidth, int rightPad)
{
    g_mainRect = mainRect;
    g_totalPer = totalPer;
    g_startIndex = startIndex;
    g_visibleCount = visibleCnt;
    g_bodyWidth = bodyWidth;
    g_rightPadding = rightPad;
}

QRect ChartConfig::mainRect() { return g_mainRect; }
double ChartConfig::totalPer() { return g_totalPer; }
int ChartConfig::startIndex() { return g_startIndex; }
int ChartConfig::visibleCount() { return g_visibleCount; }
double ChartConfig::bodyWidth() { return g_bodyWidth; }
int ChartConfig::rightPadding() { return g_rightPadding; }

int ChartConfig::candleCenterX(int idx) {
    return static_cast<int>(g_mainRect.left() + (idx - g_startIndex) * g_totalPer + g_totalPer / 2.0);
}
