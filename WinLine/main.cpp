#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QToolBar>
#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QActionGroup>
#include <QFileDialog>
#include <QStackedWidget>
#include "klinewidget.h"
#include "indicatorwidget.h"
#include "testdata.h"
#include "csvloader.h"
#include "volumewidget.h"
#include "macdwidget.h"
#include "clickfilter.h"

#include <QStyle>
#include <QListWidget>
#include <QTextEdit>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#ifdef QT_DEBUG
    QMessageBox::information(nullptr, "调试提示", "当前为 Debug 构建，工程可以编译并进行调试。");
#endif

    QMainWindow mainWindow;

    // toolbar (use QMainWindow's toolbar)
    QToolBar *toolbar = new QToolBar(&mainWindow);
    mainWindow.addToolBar(toolbar);

    QActionGroup *periodGroup = new QActionGroup(toolbar);
    periodGroup->setExclusive(true);

    QAction *a1 = toolbar->addAction("1m"); a1->setCheckable(true); periodGroup->addAction(a1);
    QAction *a5 = toolbar->addAction("5m"); a5->setCheckable(true); periodGroup->addAction(a5);
    QAction *a15 = toolbar->addAction("15m"); a15->setCheckable(true); periodGroup->addAction(a15);
    QAction *a30 = toolbar->addAction("30m"); a30->setCheckable(true); periodGroup->addAction(a30);
    QAction *a60 = toolbar->addAction("60m"); a60->setCheckable(true); periodGroup->addAction(a60);
    QAction *ah4 = toolbar->addAction("H4"); ah4->setCheckable(true); periodGroup->addAction(ah4);
    QAction *ad = toolbar->addAction("Daily"); ad->setCheckable(true); periodGroup->addAction(ad);
    QAction *aw1 = toolbar->addAction("W1"); aw1->setCheckable(true); periodGroup->addAction(aw1);
    QAction *amn = toolbar->addAction("MN"); amn->setCheckable(true); periodGroup->addAction(amn);

    // Open CSV action
    QAction *openAct = toolbar->addAction("Open CSV");
    QObject::connect(openAct, &QAction::triggered, [&mainWindow]() {
        QString file = QFileDialog::getOpenFileName(nullptr, "Open CSV", QString(), "CSV Files (*.csv *.txt);;All Files (*)");
        if (file.isEmpty()) return;
        QVector<Candle> loaded;
        QString symbol;
        int baseMin = 1;
        if (loadCsvFile(file, loaded, symbol, baseMin)) {
            // find KLineWidget in mainWindow
            KLineWidget *k = mainWindow.findChild<KLineWidget*>();
            if (k) k->setData(loaded, baseMin);
            // update window title
            mainWindow.setWindowTitle(symbol + QString(" (%1)").arg(baseMin));
            // find toolbar and set matching action checked state
            QToolBar *tb = mainWindow.findChild<QToolBar*>();
            if (tb) {
                for (QAction *act : tb->actions()) {
                    QString t = act->text();
                    bool shouldCheck = (baseMin == 1 && t == "1m") || (baseMin == 5 && t == "5m") || (baseMin == 15 && t == "15m")
                            || (baseMin == 30 && t == "30m") || (baseMin == 60 && t == "60m") || (baseMin == 240 && t == "H4")
                            || (baseMin == 1440 && t == "Daily") || (baseMin == 10080 && t == "W1") || (baseMin == 43200 && t == "MN");
                    if (act->isCheckable()) act->setChecked(shouldCheck);
                }
            }
        }
    });

    // ensure main chart and indicator widgets exist
    KLineWidget *k = new KLineWidget;
    IndicatorWidget *ind = new IndicatorWidget;

    // create stacked widget with Volume, KDJ, MACD and install ClickFilter to handle double-click switching
    QStackedWidget *stack = new QStackedWidget;
    VolumeWidget *volw = new VolumeWidget;
    IndicatorWidget *kjw = ind; // reuse existing
    MacdWidget *macdw = new MacdWidget;
    stack->addWidget(volw);
    stack->addWidget(kjw);
    stack->addWidget(macdw);

    // top splitter for main chart and indicators (right-top)
    QSplitter *topSplit = new QSplitter(Qt::Vertical, &mainWindow);
    topSplit->addWidget(k);
    topSplit->addWidget(stack);
    topSplit->setStretchFactor(0, 5);
    topSplit->setStretchFactor(1, 2);

    // text area at bottom-right
    QTextEdit *text = new QTextEdit;
    text->setPlainText("Log / Info");

    // right vertical splitter containing top chart area and text
    QSplitter *rightSplit = new QSplitter(Qt::Vertical, &mainWindow);
    rightSplit->addWidget(topSplit);
    rightSplit->addWidget(text);
    rightSplit->setStretchFactor(0, 5);
    rightSplit->setStretchFactor(1, 1);

    // list on the left
    QListWidget *list = new QListWidget;
    list->addItem("Item 1");
    list->addItem("Item 2");

    // main horizontal splitter: left list, right area
    QSplitter *split = new QSplitter(Qt::Horizontal, &mainWindow);
    split->addWidget(list);
    split->addWidget(rightSplit);
    mainWindow.setCentralWidget(split);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 3);
    // ensure visible initial sizes so layout change is obvious
    list->setMinimumWidth(180);
    text->setMinimumHeight(100);
    // set reasonable initial splitter sizes: left, rightTOP, rightBOTTOM
    split->setSizes({200, 800});
    rightSplit->setSizes({600, 200});
    topSplit->setSizes({500, 200});

    // install click filter to allow double-click on the indicator area to cycle indicators
    ClickFilter *cf = new ClickFilter(stack, &mainWindow);
    stack->installEventFilter(cf);

    QObject::connect(a1, &QAction::triggered, [k,a1](){ k->setTimeframe(KLineWidget::TF_1m); a1->setChecked(true); });
    QObject::connect(a5, &QAction::triggered, [k,a5](){ k->setTimeframe(KLineWidget::TF_5m); a5->setChecked(true); });
    QObject::connect(a15, &QAction::triggered, [k,a15](){ k->setTimeframe(KLineWidget::TF_15m); a15->setChecked(true); });
    QObject::connect(a30, &QAction::triggered, [k,a30](){ k->setTimeframe(KLineWidget::TF_30m); a30->setChecked(true); });
    QObject::connect(a60, &QAction::triggered, [k,a60](){ k->setTimeframe(KLineWidget::TF_60m); a60->setChecked(true); });
    QObject::connect(ah4, &QAction::triggered, [k,ah4](){ k->setTimeframe(KLineWidget::TF_H4); ah4->setChecked(true); });
    QObject::connect(ad, &QAction::triggered, [k,ad](){ k->setTimeframe(KLineWidget::TF_DAILY); ad->setChecked(true); });
    QObject::connect(aw1, &QAction::triggered, [k,aw1](){ k->setTimeframe(KLineWidget::TF_W1); aw1->setChecked(true); });
    QObject::connect(amn, &QAction::triggered, [k,amn](){ k->setTimeframe(KLineWidget::TF_MN); amn->setChecked(true); });

    // set initial checked action for default timeframe
    a1->setChecked(true);

    // connect aggregated data and viewport to all indicator widgets
    QObject::connect(k, &KLineWidget::dataAggregated, volw, &VolumeWidget::setData);
    QObject::connect(k, &KLineWidget::dataAggregated, kjw, &IndicatorWidget::setData);
    QObject::connect(k, &KLineWidget::dataAggregated, macdw, &MacdWidget::setData);

    QObject::connect(k, &KLineWidget::viewportChanged, volw, &VolumeWidget::setViewport);
    QObject::connect(k, &KLineWidget::viewportChanged, kjw, &IndicatorWidget::setViewport);
    QObject::connect(k, &KLineWidget::viewportChanged, macdw, &MacdWidget::setViewport);

    QObject::connect(k, &KLineWidget::crosshairIndexChanged, volw, &VolumeWidget::setCrosshairIndex);
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, kjw, &IndicatorWidget::setCrosshairIndex);
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, macdw, &MacdWidget::setCrosshairIndex);

    // load data and initialize
    k->setData(sampleKLineData());
    k->setTimeframe(KLineWidget::TF_1m);

    // toolbar for drawing tools
    QToolBar *tb = new QToolBar(&mainWindow);
    QAction *aNormal = tb->addAction(tb->style()->standardIcon(QStyle::SP_CommandLink), "");
    QAction *aLine = tb->addAction("Line");
    QAction *aTrend = tb->addAction("Trend");
    QAction *aText = tb->addAction("Text");
    QAction *aClear = tb->addAction("Clear");
    QAction *aUp = tb->addAction(tb->style()->standardIcon(QStyle::SP_ArrowUp), "");
    QAction *aDown = tb->addAction(tb->style()->standardIcon(QStyle::SP_ArrowDown), "");
    QAction *aDelete = tb->addAction(tb->style()->standardIcon(QStyle::SP_TrashIcon), "");

    mainWindow.addToolBar(tb);

    QObject::connect(aLine, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Line); });
    QObject::connect(aTrend, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Trend); });
    QObject::connect(aUp, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureUp); });
    QObject::connect(aDown, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureDown); });
    QObject::connect(aText, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Text); });
    QObject::connect(aDelete, &QAction::triggered, [k](){ k->deleteSelectedShape(); });
    QObject::connect(aClear, &QAction::triggered, [k](){ k->clearShapes(); });
    QObject::connect(aNormal, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_None); });
    mainWindow.resize(1000, 700);
    mainWindow.show();
    return a.exec();
}
