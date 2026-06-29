#include "chartgroup.h"
#include "klinewidget.h"
#include "volumewidget.h"
#include "indicatorwidget.h"
#include "macdwidget.h"
#include "clickfilter.h"

#include <QStackedWidget>

ChartGroup ChartGroup::create(QWidget *parent)
{
    ChartGroup g;
    g.kline   = new KLineWidget;
    g.volume  = new VolumeWidget;
    g.kdj     = new IndicatorWidget;
    g.macd    = new MacdWidget;

    // ── 容器（垂直排列：主图在上，副图在下） ──
    g.container = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(g.container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 副图 QStackedWidget（Volume / KDJ / MACD 可双击切换）
    QStackedWidget *stack = new QStackedWidget;
    stack->addWidget(g.volume);
    stack->addWidget(g.kdj);
    stack->addWidget(g.macd);

    // 垂直 splitter
    QSplitter *split = new QSplitter(Qt::Vertical);
    split->addWidget(g.kline);
    split->addWidget(stack);
    split->setStretchFactor(0, 5);
    split->setStretchFactor(1, 2);

    layout->addWidget(split);

    // ── ClickFilter 装在该 stack 上，实现双击切换副图 ──
    ClickFilter *cf = new ClickFilter(stack, nullptr);
    stack->installEventFilter(cf);

    return g;
}

void ChartGroup::destroy()
{
    delete container;
    container = nullptr;
    kline   = nullptr;
    volume  = nullptr;
    kdj     = nullptr;
    macd    = nullptr;
}
