#include "mainwindow.h"
#include "welcomewidget.h"

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
#include "marketsconfig.h"
#include "providerfactory.h"
#include "dataprovider.h"
#include "apppaths.h"

#include <QStyle>
#include <QPainter>
#include <QPolygonF>
#include <QTreeWidget>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>
#include <QTextEdit>
#include "apppaths.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include "dataloader.h"
#include "simwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#ifdef QT_DEBUG
    QMessageBox::information(nullptr, "调试提示", "当前为 Debug 构建，工程可以编译并进行调试。");
    // In debug builds, prefer the current working directory (project code dir) as the data root
    AppPaths::setDataRoot(QDir::currentPath());
#endif

    // create main window UI
    MainWindow mainWindow;

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
            // try to integrate the loaded CSV into configured market structure
            QTreeWidget *tree = mainWindow.findChild<QTreeWidget*>();
            QTextEdit *logText = mainWindow.findChild<QTextEdit*>();
            if (tree) {
                // search for a symbol item matching the loaded symbol
                QList<QTreeWidgetItem*> matches = tree->findItems(symbol, Qt::MatchRecursive | Qt::MatchExactly, 0);
                if (!matches.isEmpty()) {
                    QTreeWidgetItem *symItem = matches.first();
                    QString dataDir = symItem->data(0, Qt::UserRole + 1).toString();
                    QString filenamePattern = symItem->data(0, Qt::UserRole + 6).toString();
                    QString readerType = symItem->data(0, Qt::UserRole + 7).toString();
                    QString targetDir = AppPaths::resolveDataDir(dataDir);
                    QString destName;
                    if (!filenamePattern.isEmpty()) {
                        destName = filenamePattern;
                        destName.replace("%{symbol}", symbol);
                        destName.replace("%{tf}", QString::number(baseMin));
                    } else {
                        destName = QString("%1_%2.csv").arg(symbol).arg(baseMin);
                    }
                    QString destPath = QDir(targetDir).filePath(destName);
                    if (QFile::exists(destPath)) {
                        if (logText) logText->append(QStringLiteral("Target file already exists: %1").arg(destPath));
                    } else {
                        // ensure directory exists
                        QDir d(targetDir);
                        if (!d.exists()) d.mkpath(targetDir);
                        if (QFile::copy(file, destPath)) {
                            if (logText) logText->append(QStringLiteral("Copied %1 to %2").arg(file).arg(destPath));
                        } else {
                            if (logText) logText->append(QStringLiteral("Failed to copy %1 to %2").arg(file).arg(destPath));
                        }
                    }
                } else {
                    if (logText) logText->append(QStringLiteral("Loaded symbol %1 not found in markets config; file loaded ad-hoc: %2").arg(symbol).arg(file));
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

    // log area at bottom-right as a tab widget
    QTabWidget *tabs = new QTabWidget;
    QTextEdit *logText = new QTextEdit;
    logText->setReadOnly(true);
    logText->setPlainText("Log / Info\n");
    tabs->addTab(logText, "Log");

    // right vertical splitter containing top chart area and text
    QSplitter *rightSplit = new QSplitter(Qt::Vertical, &mainWindow);
    rightSplit->addWidget(topSplit);
    rightSplit->addWidget(tabs);
    rightSplit->setStretchFactor(0, 5);
    rightSplit->setStretchFactor(1, 1);

    // list on the left
    QTreeWidget *tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    MarketsConfig cfg;
    // resolve config directory using AppPaths so debug/run paths are unified
    QString configDir = AppPaths::resolveDataDir("config");
    QString cfgPath = QDir(configDir).filePath("markets.xml");
    bool loaded = cfg.loadFromFile(cfgPath);
    if (!loaded) {
        // failed to load configuration -> show error and quit
        QMessageBox::critical(&mainWindow, QStringLiteral("配置加载失败"),
                              QStringLiteral("未能在 %1 找到或解析 markets.xml 。程序将退出。").arg(cfgPath));
        return 0;
    }
    cfg.populateTree(tree);

    // ============================================================
    // Welcome widget (shown initially on the right side)
    // ============================================================
    WelcomeWidget *welcome = new WelcomeWidget;

    // Right-side stacked widget: 0 = welcome, 1 = chart area
    QStackedWidget *rightStack = new QStackedWidget;
    rightStack->addWidget(welcome);   // index 0
    rightStack->addWidget(rightSplit); // index 1
    rightStack->setCurrentIndex(0);   // show welcome first

    // global current timeframe; default 1 minute
    int currentTf = 1;
    QString currentSymbol;

    // helper to update window title
    auto updateTitle = [&mainWindow,&currentSymbol,&currentTf]() {
        QString title = QString("WinLine");
        if (!currentSymbol.isEmpty()) title += QString(" - %1").arg(currentSymbol);
        title += QString(" [%1m]").arg(currentTf);
        mainWindow.setWindowTitle(title);
    };

    // connect timeframe actions to update currentTf and keep button checked; perform async loadRecent if a symbol is selected
    auto makeTfHandler = [&](QAction *act, int tf){
        return QObject::connect(act, &QAction::triggered, [&mainWindow, &logText, &currentTf, &currentSymbol, act, tf, &updateTitle](){
            currentTf = tf; act->setChecked(true); updateTitle();
            if (currentSymbol.isEmpty()) {
                QMessageBox::information(&mainWindow, QStringLiteral("未选择品种"), QStringLiteral("请先在左侧选择一个品种，然后再切换周期。"));
                return;
            }
            logText->append(QStringLiteral("切换到周期: %1min  品种: %2").arg(tf).arg(currentSymbol));
            QTreeWidget *tree = mainWindow.findChild<QTreeWidget*>();
            if (!tree) return;
            QList<QTreeWidgetItem*> matches = tree->findItems(currentSymbol, Qt::MatchRecursive | Qt::MatchExactly, 0);
            if (matches.isEmpty()) return;
            QTreeWidgetItem *symItem = matches.first();
            DataLoader *loader = mainWindow.findChild<DataLoader*>();
            if (!loader) {
                QMessageBox::warning(&mainWindow, QStringLiteral("加载器不存在"), QStringLiteral("数据加载器未初始化。"));
                return;
            }
            loader->requestLoad(currentSymbol, currentTf, symItem);
        });
    };
    makeTfHandler(a1, 1);
    makeTfHandler(a5, 5);
    makeTfHandler(a15, 15);
    makeTfHandler(a30, 30);
    makeTfHandler(a60, 60);
    makeTfHandler(ah4, 240);
    makeTfHandler(ad, 1440);
    makeTfHandler(aw1, 10080);
    makeTfHandler(amn, 43200);

    // ensure initial checked action for default timeframe
    a1->setChecked(true);

    // find KLineWidget and create DataLoader
    KLineWidget *klineWidget = mainWindow.findChild<KLineWidget*>();
    DataLoader *loader = nullptr;
    if (klineWidget) {
        loader = new DataLoader(klineWidget, &mainWindow);
        // 启动 RPC 客户端
        if (loader->startRpcClient()) {
            logText->append(QStringLiteral("KBarRPC client initialized, waiting for data request... (127.0.0.1:9888)"));
        } else {
            logText->append(QStringLiteral("KBarRPC client failed to start"));
        }

        // ================================================================
        // 连接 DataLoader 信号，实现 Loading 覆盖层和推送数据更新
        // ================================================================

        // 加载开始 -> 显示 Loading
        QObject::connect(loader, &DataLoader::loadStarted, klineWidget, [klineWidget](const QString &symbol, int tf) {
            klineWidget->showLoading(QStringLiteral("正在加载 %1 %2min...").arg(symbol).arg(tf));
        });

        // 加载完成或失败 -> 隐藏 Loading
        QObject::connect(loader, &DataLoader::loadFinished, klineWidget, [klineWidget]() {
            klineWidget->hideLoading();
        });
        QObject::connect(loader, &DataLoader::loadFailed, klineWidget, [klineWidget]() {
            klineWidget->hideLoading();
        });

        // 连接状态变化 -> 更新实时价格标签的圆点颜色
        QObject::connect(loader, &DataLoader::connectionStatusChanged, klineWidget,
            &KLineWidget::setConnectionStatus);

        // 推送数据到达 -> 实时更新 K 线图（跨线程安全，信号会自动切换到主线程）
        QObject::connect(loader, &DataLoader::pushDataReady, klineWidget,
            [klineWidget, loader](const QString &symbol, int timeFrame,
                                  uint64_t time, double open, double high,
                                  double low, double close, double volume)
        {
            // 只处理当前正在显示的品种和周期
            if (symbol != loader->currentSymbol() || timeFrame != loader->currentTimeframe()) {
                return;
            }

            // 构造单根 Candle
            Candle c;
            c.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time));
            c.open = open;
            c.high = high;
            c.low = low;
            c.close = close;
            c.volume = volume;

            // 更新 K 线图（内部判断是同根更新还是新 K 线追加）
            klineWidget->updateRealtimeCandle(c);
        });
    }


    // double-click symbol loads data for currentTf, switches to chart view and marks tree selection
    QObject::connect(tree, &QTreeWidget::itemDoubleClicked, [&mainWindow, &logText, &currentTf, &currentSymbol, &updateTitle, rightStack](QTreeWidgetItem *item, int){
        if (!item) return;
        if (item->childCount() > 0) return;
        QString symbol = item->text(0);
        logText->append(QStringLiteral("切换到品种: %1  周期: %2min").arg(symbol).arg(currentTf));
        currentSymbol = symbol;
        updateTitle();
        item->setSelected(true);

        // Switch from welcome to chart view
        rightStack->setCurrentIndex(1);
        logText->append(QStringLiteral("切换到图表视图"));

        DataLoader *loader = mainWindow.findChild<DataLoader*>();
        if (!loader) {
            QMessageBox::warning(&mainWindow, QStringLiteral("加载器不存在"), QStringLiteral("数据加载器未初始化。"));
            return;
        }
        loader->requestLoad(symbol, currentTf, item);
    });

    // main horizontal splitter: left list, right area
    QSplitter *split = new QSplitter(Qt::Horizontal, &mainWindow);
    split->addWidget(tree);
    split->addWidget(rightStack);
    mainWindow.setCentralWidget(split);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 3);
    // ensure visible initial sizes so layout change is obvious
    tree->setMinimumWidth(180);
    tabs->setMinimumHeight(100);
    // set reasonable initial splitter sizes: left, rightTOP, rightBOTTOM
    split->setSizes({200, 800});
    rightSplit->setSizes({600, 200});
    topSplit->setSizes({500, 200});

    // install click filter to allow double-click on the indicator area to cycle indicators
    ClickFilter *cf = new ClickFilter(stack, &mainWindow);
    stack->installEventFilter(cf);

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

    // ---- 画图工具栏图标（QPainter 手绘，只画一次，不影响性能）----
    auto makeIcon = [](std::function<void(QPainter&)> draw) -> QIcon {
        QPixmap px(24, 24);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        draw(p);
        p.end();
        return QIcon(px);
    };

    // Normal: 鼠标指针（退出画图模式）
    QIcon iconNormal = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 1.5));
        p.drawLine(4,4, 4,20);
        p.drawLine(4,4, 16,12);
        p.drawLine(4,12, 12,16);
        p.drawLine(16,12, 20,20);
    });

    // Line: 一条贯穿的斜线，两端带小点表示无限延伸
    QIcon iconLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,50), 2));
        p.drawLine(3,21, 21,3);
        p.setBrush(QColor(200,200,50));
        p.drawEllipse(QPoint(3,21), 2,2);
        p.drawEllipse(QPoint(21,3), 2,2);
    });

    // Trend: 射线 —— 左侧圆点起点，右侧延伸带箭头
    QIcon iconTrend = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,200,255), 2));
        p.drawLine(4,20, 18,6);
        // 箭头
        p.drawLine(18,6, 12,6);
        p.drawLine(18,6, 18,12);
        // 起点圆点
        p.setBrush(QColor(100,200,255));
        p.drawEllipse(QPoint(4,20), 2,2);
    });

    // Text: 字母 A
    QIcon iconText = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 2));
        QFont f = p.font(); f.setPixelSize(18); f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0,0,24,24), Qt::AlignCenter, "A");
    });

    // 上箭头
    QIcon iconUp = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,255,100), 2));
        p.setBrush(QColor(100,255,100));
        QPolygonF arrow;
        arrow << QPointF(12,2) << QPointF(4,12) << QPointF(9,12)
              << QPointF(9,22) << QPointF(15,22) << QPointF(15,12)
              << QPointF(20,12);
        p.drawPolygon(arrow);
    });

    // 下箭头
    QIcon iconDown = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,100,100), 2));
        p.setBrush(QColor(255,100,100));
        QPolygonF arrow;
        arrow << QPointF(12,22) << QPointF(4,12) << QPointF(9,12)
              << QPointF(9,2) << QPointF(15,2) << QPointF(15,12)
              << QPointF(20,12);
        p.drawPolygon(arrow);
    });

    // Delete: 红叉
    QIcon iconDelete = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(255,80,80), 3));
        p.drawLine(4,4, 20,20);
        p.drawLine(20,4, 4,20);
    });

    // Clear: 垃圾桶
    QIcon iconClear = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,200), 1.5));
        // 桶身
        p.drawRect(5,9, 14,13);
        // 桶口
        p.drawLine(3,9, 21,9);
        // 把手
        p.drawLine(9,9, 9,5);
        p.drawLine(15,9, 15,5);
        p.drawLine(9,5, 15,5);
        // 桶身条纹
        p.drawLine(8,13, 16,13);
        p.drawLine(8,17, 16,17);
    });

    QToolBar *tb = new QToolBar(&mainWindow);
    QAction *aNormal = tb->addAction(iconNormal, "");
    QAction *aLine = tb->addAction(iconLine, "");
    QAction *aTrend = tb->addAction(iconTrend, "");
    QAction *aText = tb->addAction(iconText, "");
    QAction *aUp = tb->addAction(iconUp, "");
    QAction *aDown = tb->addAction(iconDown, "");
    QAction *aDelete = tb->addAction(iconDelete, "");
    QAction *aClear = tb->addAction(iconClear, "");

    mainWindow.addToolBar(tb);

    QObject::connect(aLine, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Line); });
    QObject::connect(aTrend, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Trend); });
    QObject::connect(aUp, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureUp); });
    QObject::connect(aDown, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_GestureDown); });
    QObject::connect(aText, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_Text); });
    QObject::connect(aDelete, &QAction::triggered, [k](){ k->deleteSelectedShape(); });
    QObject::connect(aClear, &QAction::triggered, [k](){ k->clearShapes(); });
    QObject::connect(aNormal, &QAction::triggered, [k](){ k->setToolMode(KLineWidget::Tool_None); });

    // ----- 模拟回放按钮 -----
    QAction *aSim = toolbar->addAction(QStringLiteral("模拟"));
    QObject::connect(aSim, &QAction::triggered, [&mainWindow]() {
        SimWindow *sim = new SimWindow(&mainWindow);
        sim->setAttribute(Qt::WA_DeleteOnClose);
        sim->show();
    });

    mainWindow.resize(1000, 700);

    mainWindow.show();
    return a.exec();
}
