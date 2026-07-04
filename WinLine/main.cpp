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
#include "volumewidget.h"
#include "macdwidget.h"
#include "clickfilter.h"
#include "marketsconfig.h"
#include "apppaths.h"

#include <QStyle>
#include <QPainter>
#include <QPolygonF>
#include <QTreeWidget>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>
#include <QTextEdit>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include "dataloader.h"
#include "simwindow.h"
#include "drawtoolbar.h"
#include "shapedialog.h"
#include "luascriptengine.h"
#include "settingsdialog.h"
#include "indicatorcalc.h"
#include "mt4rpc/KBarManager.h"
#include <NetCore/FeishuSender.h>

#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QGroupBox>
#include <QDateTime>

// 日志写入文件（Release 版没有控制台窗口，WIN32_EXECUTABLE TRUE）
// 使用 OutputDebugStringA 输出到 DebugView，同时写入日志文件
#include <io.h>
#include <fcntl.h>
#include <fstream>

static std::ofstream s_logFile;
static bool s_logFileOpened = false;

static void openLogFile()
{
    if (s_logFileOpened) return;
    s_logFileOpened = true;
    QString logPath = QCoreApplication::applicationDirPath() + "/WinLine_debug.log";
    s_logFile.open(logPath.toStdString(), std::ios::out | std::ios::app);
    if (s_logFile.is_open()) {
        s_logFile << "=== WinLine Debug Log Started ===" << std::endl;
        s_logFile.flush();
    }
}

static void debugLog(const char *msg)
{
    qDebug() << "[DBG]" << msg;
    openLogFile();
    if (s_logFile.is_open()) {
        s_logFile << "[DBG] " << msg << std::endl;
        s_logFile.flush();
    }
    OutputDebugStringA("[DBG] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

static void debugLog2(const char *tag, const QString &val)
{
    QString full = QString("[DBG] %1: %2").arg(tag).arg(val);
    qDebug().noquote() << full;
    openLogFile();
    if (s_logFile.is_open()) {
        s_logFile << full.toStdString() << std::endl;
        s_logFile.flush();
    }
    OutputDebugStringA(full.toUtf8().constData());
    OutputDebugStringA("\n");
}

int main(int argc, char *argv[])
{
    debugLog("main() started");

    QApplication a(argc, argv);

    // 注册 Candle 元类型，支持跨线程 QueuedConnection 信号传递
    qRegisterMetaType<Candle>("Candle");
    debugLog("qRegisterMetaType done");

#ifdef QT_DEBUG
    QMessageBox::information(nullptr, "调试提示", "当前为 Debug 构建，工程可以编译并进行调试。");
    // In debug builds, prefer the current working directory (project code dir) as the data root
    AppPaths::setDataRoot(QDir::currentPath());
    debugLog("Debug build, AppPaths::setDataRoot set");
#else
    debugLog("Release build");
#endif

    // create main window UI
    debugLog("Creating MainWindow...");
    MainWindow mainWindow;
    debugLog("MainWindow created");

    // 菜单栏：设置
    QAction *settingsAction = mainWindow.menuBar()->addAction("设置");
    QObject::connect(settingsAction, &QAction::triggered, [&mainWindow]() {
        SettingsDialog dlg(&mainWindow);
        dlg.loadFromFile();
        if (dlg.exec() == QDialog::Accepted) {
            dlg.saveToFile();
        }
    });

    debugLog("Creating toolbar...");
    // toolbar (use QMainWindow's toolbar)
    QToolBar *toolbar = new QToolBar(&mainWindow);
    mainWindow.addToolBar(toolbar);

    QActionGroup *periodGroup = new QActionGroup(toolbar);
    periodGroup->setExclusive(true);

    QAction *a5 = toolbar->addAction("5m"); a5->setCheckable(true); periodGroup->addAction(a5);
    QAction *a15 = toolbar->addAction("15m"); a15->setCheckable(true); periodGroup->addAction(a15);
    QAction *a30 = toolbar->addAction("30m"); a30->setCheckable(true); periodGroup->addAction(a30);
    QAction *a60 = toolbar->addAction("60m"); a60->setCheckable(true); periodGroup->addAction(a60);
    QAction *ah4 = toolbar->addAction("H4"); ah4->setCheckable(true); periodGroup->addAction(ah4);
    QAction *ad = toolbar->addAction("Daily"); ad->setCheckable(true); periodGroup->addAction(ad);
    QAction *aw = toolbar->addAction("Weekly"); aw->setCheckable(true); periodGroup->addAction(aw);
    debugLog("Creating widgets...");
    // ensure main chart and indicator widgets exist
    KLineWidget *k = new KLineWidget;
    debugLog("KLineWidget created");
    IndicatorWidget *ind = new IndicatorWidget;
    debugLog("IndicatorWidget created");

    // create stacked widget with Volume, KDJ, MACD and install ClickFilter to handle double-click switching
    QStackedWidget *stack = new QStackedWidget;
    VolumeWidget *volw = new VolumeWidget;
    debugLog("VolumeWidget created");
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
    rightSplit->setStretchFactor(0, 8);
    rightSplit->setStretchFactor(1, 1);

    // list on the left
    debugLog("Creating tree widget...");
    QTreeWidget *tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    MarketsConfig cfg;
    // resolve config directory using AppPaths so debug/run paths are unified
    debugLog("Resolving config directory via AppPaths...");
    QString configDir = AppPaths::resolveDataDir("config");
    debugLog2("configDir resolved to", configDir);
    QString cfgPath = QDir(configDir).filePath("markets.xml");
    debugLog2("cfgPath", cfgPath);
    debugLog("Loading markets.xml...");
    bool loaded = cfg.loadFromFile(cfgPath);
    debugLog2("markets.xml loaded = ", loaded ? "true" : "false");
    if (!loaded) {
        // failed to load configuration -> show error and quit
        debugLog("markets.xml loading FAILED, will show error dialog and exit");
        QMessageBox::critical(&mainWindow, QStringLiteral("配置加载失败"),
                              QStringLiteral("未能在 %1 找到或解析 markets.xml 。程序将退出。").arg(cfgPath));
        return 0;
    }
    debugLog("Populating tree...");
    cfg.populateTree(tree);
    debugLog("Tree populated");

    // ============================================================
    // Welcome widget (shown initially on the right side)
    // ============================================================
    WelcomeWidget *welcome = new WelcomeWidget;

    // Right-side stacked widget: 0 = welcome, 1 = chart area
    QStackedWidget *rightStack = new QStackedWidget;
    rightStack->addWidget(welcome);   // index 0
    rightStack->addWidget(rightSplit); // index 1
    rightStack->setCurrentIndex(0);   // show welcome first

    // 支持的周期: 5, 15, 30, 60, 240, 1440, 10080
    int currentTf = 5;
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
    makeTfHandler(a5, 5);
    makeTfHandler(a15, 15);
    makeTfHandler(a30, 30);
    makeTfHandler(a60, 60);
    makeTfHandler(ah4, 240);
    makeTfHandler(ad, 1440);
    makeTfHandler(aw, 10080);

    // ensure initial checked action for default timeframe (5m)
    a5->setChecked(true);

    // ================================================================
    // LuaScriptEngine 初始化
    // ================================================================
    debugLog("STEP: Creating LuaScriptEngine...");
    LuaScriptEngine *luaEngine = new LuaScriptEngine(&mainWindow);
    debugLog("STEP: LuaScriptEngine created, setting KLineWidget...");
    luaEngine->setKLineWidget(k);
    debugLog("STEP: Initializing LuaScriptEngine...");
    bool luaInitOk = luaEngine->initialize();
    debugLog2("STEP: initialize() returned", luaInitOk ? "true" : "false");
    if (!luaInitOk) {
        logText->append("LuaScriptEngine initialize failed: " + luaEngine->lastError());
        debugLog2("LuaScriptEngine initialize failed", luaEngine->lastError());
    } else {
        logText->append("LuaScriptEngine initialized");
        debugLog("LuaScriptEngine initialized OK");
    }

    debugLog("STEP: Connecting script signals...");
    // 连接脚本日志/错误到日志输出
    QObject::connect(luaEngine, &LuaScriptEngine::scriptLog,
        logText, [logText](const QString &msg) {
        logText->append(QStringLiteral("[Lua] %1").arg(msg));
    });
    QObject::connect(luaEngine, &LuaScriptEngine::scriptError,
        logText, [logText](const QString &scriptName, const QString &error) {
        logText->append(QStringLiteral("[Lua Error] %1: %2").arg(scriptName, error));
    });

    debugLog("STEP: registerKLineWidget...");
    // 注册主 KLineWidget 到引擎
    luaEngine->registerKLineWidget(k);
    debugLog("STEP: registerKLineWidget done");

    debugLog("STEP: Connecting candleUpdated signal...");
    // 连接 K 线更新信号到脚本引擎（通过 requestBarEvent 实现跨线程安全调度）
    QObject::connect(k, &KLineWidget::candleUpdated, k,
        [k, luaEngine](const Candle &candle, bool isNewBar) {
        const QString &sym = k->symbol();
        int tf = k->baseMinutes();
        if (sym.isEmpty() || tf <= 0) return;
        luaEngine->requestBarEvent(sym, tf, candle, isNewBar);
    });
    debugLog("STEP: candleUpdated signal connected");

    debugLog("STEP: Creating DataLoader...");
    // find KLineWidget and create DataLoader
    KLineWidget *klineWidget = k;  // 使用上面已创建的 KLineWidget
    DataLoader *loader = nullptr;
    if (klineWidget) {
        debugLog("STEP: klineWidget is not null, creating DataLoader...");
        loader = new DataLoader(klineWidget, &mainWindow);
        debugLog("STEP: DataLoader created, starting RPC client...");
        // 启动 RPC 客户端
        if (loader->startRpcClient()) {
            logText->append(QStringLiteral("KBarRPC client initialized, waiting for data request... (127.0.0.1:9888)"));
            debugLog("KBarRPC client started successfully");
        } else {
            logText->append(QStringLiteral("KBarRPC client failed to start"));
            debugLog("KBarRPC client FAILED to start");
        }
        debugLog("STEP: RPC client start done");

        // ================================================================
        // 连接 DataLoader 信号，实现 Loading 覆盖层和推送数据更新
        // ================================================================

        // 加载开始 -> 显示 Loading
        QObject::connect(loader, &DataLoader::loadStarted, klineWidget, [klineWidget](const QString &symbol, int tf) {
            klineWidget->showLoading(QStringLiteral("正在加载 %1 %2min...").arg(symbol).arg(tf));
        });

        // 加载完成或失败 -> 隐藏 Loading，显示"暂无数据"（如果数据为空）
        // 同时加载 shapes 关联的脚本（先清空旧的脚本绑定，再根据新 shapes 加载）
        // 并回放历史 K 线，使脚本有机会扫描数据创建子 shape
        QObject::connect(loader, &DataLoader::loadFinished, klineWidget,
            [klineWidget, luaEngine, logText](const QString &symbol, int tf, bool success) {
            Q_UNUSED(success)
            klineWidget->hideLoading();

            // 先卸载该品种/周期下已绑定的所有旧脚本
            luaEngine->unloadByBinding(symbol, tf);

            bool scriptLoaded = false;

            // 遍历 shapes（先从当前 KLineWidget，再从引擎磁盘缓存），加载关联的脚本
            QVector<QSharedPointer<Shape>> shapes = klineWidget->shapes();
            if (shapes.isEmpty()) {
                // 检查引擎的磁盘缓存（自动加载的场景，shapes 在缓存中不在 KLineWidget 上）
                QString key = symbol + "|" + QString::number(tf);
                for (const auto &sp : luaEngine->shapesDiskCache(key)) {
                    if (!sp->scriptName.isEmpty())
                        shapes.append(sp);
                }
            }
            for (const auto &shape : shapes) {
                if (!shape->scriptName.isEmpty()) {
                    // 提取文件名（去掉路径），例如 "ma_cross.lua" -> "ma_cross"
                    QString scriptFile = shape->scriptName;
                    if (scriptFile.endsWith(".lua", Qt::CaseInsensitive)) {
                        scriptFile = scriptFile.left(scriptFile.length() - 4); // 去掉 .lua 后缀
                    }
                    ScriptBinding binding;
                    binding.symbol = symbol;
                    binding.timeframe = tf;
                    luaEngine->loadScript(scriptFile, shape->scriptParams, binding);
                    scriptLoaded = true;
                }
            }

            // ★ 回放历史 K 线：使已加载的脚本能扫描历史数据并创建子 shape
            if (scriptLoaded) {
                const auto &allData = klineWidget->allData();
                if (!allData.isEmpty()) {
                    logText->append(QStringLiteral("[Lua] 开始回放 %1 条历史 K 线...").arg(allData.size()));
                    luaEngine->replayBars(symbol, tf, allData);
                    logText->append(QStringLiteral("[Lua] 历史 K 线回放完成"));
                    klineWidget->update();  // 刷新界面显示脚本创建的子 shape
                }
            }
        });



        QObject::connect(loader, &DataLoader::loadFailed, klineWidget,
            [klineWidget](const QString &symbol, int tf, const QString &reason) {
            klineWidget->hideLoading();
            Q_UNUSED(symbol) Q_UNUSED(tf) Q_UNUSED(reason)
        });

        // 连接状态变化 -> 更新实时价格标签的圆点颜色
        QObject::connect(loader, &DataLoader::connectionStatusChanged, klineWidget,
            &KLineWidget::setConnectionStatus);

        // 推送数据到达 -> 触发脚本引擎 + 实时更新 K 线图（跨线程安全）
        // 只有该 (品种, 周期) 状态为 Loaded 时才接受推送
        QObject::connect(loader, &DataLoader::pushDataReady, klineWidget,
            [klineWidget, loader, luaEngine](const QString &symbol, int timeFrame,
                          uint64_t time, double open, double high,
                          double low, double close, double volume)
        {
            // 检查该周期状态是否为 Loaded，未就绪时不处理推送（避免在初始化时混入零散数据）
            if (!loader->canAcceptPush(symbol, timeFrame)) {
                return;
            }

            // ★ DataLoader::Impl::on_kbar_pushed 已写入 KBarManager，此处不再重复写入

            // 收到实时数据推送，退出回放模式（确保飞书消息、信号等能正常发送）
            if (luaEngine) {
                luaEngine->setReplayMode(false);
            }

            // 构造单根 Candle
            Candle c;
            c.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time));
            c.open = open;
            c.high = high;
            c.low = low;
            c.close = close;
            c.volume = volume;

            // ── 更新当前显示的 K 线图 ──
            if (symbol == klineWidget->symbol() && timeFrame == klineWidget->baseMinutes()) {
                // updateRealtimeCandle 内部会发射 candleUpdated -> 触发 luaEngine->requestBarEvent + dataAggregated -> 指标计算
                klineWidget->updateRealtimeCandle(c);
            } else {
                // ── 非当前显示周期：手动提交到脚本引擎 + 触发指标计算 ──
                // 这样即使主图显示的是 15m，关联了 5m 图形/脚本的收线提醒等依然能正常运行
                // ── 1) 更新 KBarManager（由回调已写入） ──
                // ── 2) 更新指标缓存 ──
                auto &calc = IndicatorCalculator::instance();
                auto allBars = KBarManager::instance().get_kbars(symbol.toStdString(), timeFrame);
                QVector<Candle> allCandles;
                allCandles.reserve(static_cast<int>(allBars.size()));
                for (const auto &kb : allBars) {
                    Candle tmp;
                    tmp.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(kb.time));
                    tmp.open = kb.open; tmp.high = kb.high; tmp.low = kb.low;
                    tmp.close = kb.close; tmp.volume = static_cast<double>(kb.volume);
                    allCandles.append(tmp);
                }
                if (allCandles.size() >= 2) {
                    calc.updateIndicators(symbol, timeFrame, allCandles);
                }
                // ── 3) 更新脚本引擎 ──
                if (luaEngine) {
                    // 从 KBarManager 判断是否为新 K 线（最后一条 vs 倒数第二条的时间）
                    bool isNew = true;
                    if (allBars.size() >= 2) {
                        const KBar &prev = allBars[allBars.size() - 2];
                        isNew = (QDateTime::fromSecsSinceEpoch(static_cast<qint64>(prev.time)) < c.date);
                    }
                    luaEngine->requestBarEvent(symbol, timeFrame, c, isNew);
                }
            }
        });

        // 启动预加载：扫描 data/shapes/ 目录，找出所有关联了脚本的品种+周期，按各自周期预加载
        // 当 shapes 保存后，刷新 Lua 引擎的 shapes 缓存
        QObject::connect(k, &KLineWidget::shapesSaved, luaEngine, &LuaScriptEngine::reloadShapesForSymbol);
        QObject::connect(luaEngine, &LuaScriptEngine::scriptsInitialized,
            loader, [loader, &logText](const QList<QPair<QString,int>> &syms) {
            for (const auto &pair : syms) {
                // 自动加载带脚本的 (symbol,tf) 数据（symItem=nullptr 表示后台自动加载）
                logText->append(QStringLiteral("[预加载] 品种 %1 周期 %2min 有形状+脚本，自动加载数据...").arg(pair.first).arg(pair.second));
                loader->requestLoad(pair.first, pair.second, nullptr);
            }
        });
        luaEngine->loadShapesFromDisk();
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
    tabs->setMinimumHeight(50);
    // set reasonable initial splitter sizes: left, rightTOP, rightBOTTOM
    split->setSizes({200, 800});
    rightSplit->setSizes({750, 70});
    topSplit->setSizes({580, 120});

    // install click filter to allow double-click on the indicator area to cycle indicators
    ClickFilter *cf = new ClickFilter(stack, &mainWindow);
    stack->installEventFilter(cf);

    // connect aggregated data and viewport to all indicator widgets
    QObject::connect(k, &KLineWidget::dataAggregated, volw, &VolumeWidget::setData);
    QObject::connect(k, &KLineWidget::dataAggregated, macdw, &MacdWidget::setData);

    // 数据就绪后：更新 IndicatorCalculator 指标缓存 + 刷新 KDJ 指示器
    QObject::connect(k, &KLineWidget::dataAggregated, k,
        [k, kjw](const QVector<Candle> &data) {
        QString sym = k->symbol();
        int tf = k->baseMinutes();
        if (sym.isEmpty() || tf <= 0) return;
        // 更新指标缓存
        IndicatorCalculator::instance().updateIndicators(sym, tf, data);
        // 更新 KDJ 指示器
        kjw->setData(data);
        kjw->setSymbol(sym);
        kjw->setTimeframe(tf);
        kjw->loadKDJ();
        kjw->update();
    });

    QObject::connect(k, &KLineWidget::viewportChanged, volw, &VolumeWidget::setViewport);
    QObject::connect(k, &KLineWidget::viewportChanged, kjw, &IndicatorWidget::setViewport);
    QObject::connect(k, &KLineWidget::viewportChanged, macdw, &MacdWidget::setViewport);

    QObject::connect(k, &KLineWidget::crosshairIndexChanged, volw, &VolumeWidget::setCrosshairIndex);
    QObject::connect(k, &KLineWidget::layoutChanged, kjw, &IndicatorWidget::setLayout);
    QObject::connect(k, &KLineWidget::layoutChanged, volw, &VolumeWidget::setLayout);
    QObject::connect(k, &KLineWidget::layoutChanged, macdw, &MacdWidget::setLayout);
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, kjw, &IndicatorWidget::setCrosshairIndex);
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, macdw, &MacdWidget::setCrosshairIndex);

    // ================================================================
    // 双击 shape -> 弹出完整属性对话框（使用 ShapeDialog）
    // ================================================================
    QObject::connect(k, &KLineWidget::shapeDoubleClicked, k,
        [k, luaEngine, logText](int index) {
        // 使用引用，这样 setShapes 时能写回
        auto shapes = const_cast<QVector<QSharedPointer<Shape>>&>(k->shapes());
        if (index < 0 || index >= shapes.size()) return;
        auto &s = shapes[index];

        // ★ 如果是子 shape（ownerShapeId > 0），跳转到父 shape 的对话框
        int targetIndex = index;
        if (s->ownerShapeId > 0) {
            for (int i = 0; i < shapes.size(); ++i) {
                if (shapes[i]->id == s->ownerShapeId) {
                    targetIndex = i;
                    break;
                }
            }
            if (targetIndex == index) return; // 找不到父 shape，不弹窗
        }
        auto target = shapes[targetIndex];

        ShapeDialog dlg(k);
        dlg.setWindowTitle(QStringLiteral("图形属性 - %1").arg(target->name));

        // 基本信息
        QString info;
        QString typeStr;
        switch (target->type()) {
        case ShapeType::Line: typeStr = "水平线"; break;
        case ShapeType::Trend: typeStr = "趋势线"; break;
        case ShapeType::UpTriangle: typeStr = "上三角"; break;
        case ShapeType::DownTriangle: typeStr = "下三角"; break;
        case ShapeType::Fixed: typeStr = "文本标注"; break;
        default: typeStr = "未知"; break;
        }
        info += QStringLiteral("类型: %1\n").arg(typeStr);
        info += QStringLiteral("坐标: (%1, %2) → (%3, %4)")
            .arg(target->x1).arg(target->y1, 0, 'f', 2)
            .arg(target->x2).arg(target->y2, 0, 'f', 2);
        if (target->tradePrice != 0.0)
            info += QStringLiteral("\n成交价: %1").arg(target->tradePrice, 0, 'f', 2);
        if (target->profit != 0.0)
            info += QStringLiteral("\n盈亏: %1%2").arg(target->profit >= 0 ? "+" : "").arg(target->profit, 0, 'f', 2);
        dlg.setShapeInfo(info);

        dlg.setShapeName(target->name);
        dlg.setShapeColor(target->color.isValid() ? target->color : Qt::white);
        dlg.setShapeText(target->text);

        // 脚本列表
        QString scriptsDir = AppPaths::resolveDataDir("data/scripts");
        QDir dir(scriptsDir);
        QStringList files = dir.entryList({"*.lua"}, QDir::Files);
        dlg.setScriptList(files);
        dlg.setScriptName(target->scriptName);

        // 脚本说明
        {
            QString curScript = target->scriptName;
            if (curScript.endsWith(".lua", Qt::CaseInsensitive))
                curScript = curScript.left(curScript.length() - 4);
            dlg.setScriptDescription(luaEngine->getScriptDescription(curScript));
        }

        // 参数
        if (!target->scriptParams.isEmpty()) {
            QJsonObject jo = QJsonDocument::fromJson(target->scriptParams.toUtf8()).object();
            int pi = 0;
            for (auto it = jo.begin(); it != jo.end() && pi < 3; ++it, ++pi) {
                dlg.setParam(pi, it.key(), QString::number((*it).toDouble()));
            }
        }

        if (dlg.exec() == QDialog::Accepted) {
            target->name = dlg.getShapeName();
            target->color = dlg.getShapeColor();
            target->text = dlg.getShapeText();

            // 如果之前关联了脚本，先卸载旧脚本
            if (!target->scriptName.isEmpty()) {
                QString oldFile = target->scriptName;
                if (oldFile.endsWith(".lua", Qt::CaseInsensitive))
                    oldFile = oldFile.left(oldFile.length() - 4);
                luaEngine->unloadScript(oldFile);
            }
            // 更新脚本
            QString newScript = dlg.getScriptName();
            // 拼装参数 JSON
            QStringList paramList;
            for (int i = 0; i < 3; ++i) {
                QString n = dlg.getParamName(i);
                QString v = dlg.getParamValue(i);
                if (!n.isEmpty() && !v.isEmpty())
                    paramList << QStringLiteral("\"%1\":%2").arg(n, v);
            }
            QString newParams = "{" + paramList.join(",") + "}";
            target->scriptName = newScript;
            target->scriptParams = newParams;
            k->setShapes(shapes);
            // ★ 立即保存到磁盘！避免切换周期后脚本关联丢失
            k->saveShapes();
            // 加载新脚本
            if (!newScript.isEmpty()) {
                QString scriptFile = newScript;
                if (scriptFile.endsWith(".lua", Qt::CaseInsensitive))
                    scriptFile = scriptFile.left(scriptFile.length() - 4);
                ScriptBinding binding;
                binding.symbol = k->symbol();
                binding.timeframe = k->baseMinutes();
                luaEngine->loadScript(scriptFile, newParams, binding);

                // ★ 回放历史 K 线：遍历所有已加载的 K 线逐根调用脚本
                //    使脚本有机会扫描历史数据并创建子 shape（如金叉/死叉标记）
                const auto &allData = k->allData();
                if (!allData.isEmpty()) {
                    logText->append(QStringLiteral("[Lua] 开始回放 %1 条历史 K 线...").arg(allData.size()));
                    luaEngine->replayBars(k->symbol(), k->baseMinutes(), allData);
                    logText->append(QStringLiteral("[Lua] 历史 K 线回放完成"));
                    k->update();  // 刷新界面显示脚本创建的子 shape
                }
            }
            k->update();
        }
    });

    // load data and initialize
    k->setSymbol(QStringLiteral("SAMPLE"));
    k->setData(sampleKLineData());
    k->setTimeframe(KLineWidget::TF_5m);

    // ---- 画图工具栏（共用 drawtoolbar.h 实现）----
    QToolBar *tb = createDrawingToolbar(k, &mainWindow);
    mainWindow.addToolBar(tb);

    // ----- 模拟回放按钮 -----
    QAction *aSim = toolbar->addAction(QStringLiteral("模拟"));
    QObject::connect(aSim, &QAction::triggered, [&mainWindow]() {
        SimWindow *sim = new SimWindow(&mainWindow);
        sim->setAttribute(Qt::WA_DeleteOnClose);
        sim->show();
    });

    // ----- 测试飞书（使用 LuaScriptEngine 内部的 FeishuSender 异步发送）-----
    toolbar->addSeparator();
    QAction *aFeishu = toolbar->addAction(QStringLiteral("测试飞书"));
    QObject::connect(aFeishu, &QAction::triggered, [logText, luaEngine]() {
        if (!luaEngine || !luaEngine->feishuSender()) {
            logText->append("[飞书] FeishuSender 未初始化，请在 设置→飞书 中配置 Webhook URL");
            return;
        }
        QString msg = QStringLiteral("WinLine 测试消息 - %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        logText->append(QStringLiteral("[飞书] 正在发送: %1").arg(msg));
        // 异步发送飞书消息，发送完通过回调记录结果（空回调 = 发送完不用管）
        luaEngine->feishuSender()->SendMarkdown(msg.toStdString(), [logText](const NetCore::HttpResponse &resp) {
            if (resp.status_code >= 200 && resp.status_code < 300)
                logText->append(QStringLiteral("[飞书] 发送成功（HTTP %1）").arg(resp.status_code));
            else
                logText->append(QStringLiteral("[飞书] 发送失败: HTTP %1 %2").arg(resp.status_code).arg(QString::fromStdString(resp.status_text)));
        });
    });

    mainWindow.resize(1000, 700);

    mainWindow.show();
    return a.exec();
}
