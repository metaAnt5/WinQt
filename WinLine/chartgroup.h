#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>

class KLineWidget;
class VolumeWidget;
class IndicatorWidget;
class MacdWidget;

// ============================================================
// ChartGroup - 一个品种/周期对应的完整图表组
//
// 包含：
//   - KLineWidget（主 K 线图）
//   - VolumeWidget（成交量）
//   - IndicatorWidget（KDJ）
//   - MacdWidget（MACD）
//   - container（垂直布局的容器 widget，用于 QStackedWidget 切换）
// ============================================================
struct ChartGroup {
    KLineWidget *kline = nullptr;
    VolumeWidget *volume = nullptr;
    IndicatorWidget *kdj = nullptr;
    MacdWidget *macd = nullptr;
    QWidget *container = nullptr;

    // 构造完整的图表组（含垂直布局）
    static ChartGroup create(QWidget *parent = nullptr);

    // 销毁并释放所有子控件
    void destroy();
};
