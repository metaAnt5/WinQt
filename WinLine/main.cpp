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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 注册 Candle 元类型，支持跨线程 QueuedConnection 信号传递
    qRegisterMetaType<Candle>("Candle");
#ifdef QT_DEBUG
    QMessageBox::information(nullptr, "调试提示", "当前为 Debug 构建，工程可以编译并进行调试。");
    // In debug builds, prefer the current working directory (project code dir) as the data root
    AppPaths::setDataRoot(QDir::currentPath());
#endif

    // create main window UI
    MainWindow mainWindow;

    // 菜单栏：设置
    QAction *settingsAction = mainWindow.menuBar()->addAction("设置");
    QObject::connect(settingsAction, &QAction::triggered, [&mainWindow]() {
        SettingsDialog dlg(&mainWindow);
        dlg.loadFromFile();
        if (dlg.exec() == QDialog::Accepted) {
            dlg.saveToFile();
        }
    });

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

    // ================================================================
    // 图形+脚本 页签（重新设计美化版）
    // ================================================================
    QString tabStyle = R"(
        QGroupBox {
            font: bold 12px;
            border: 1px solid #555555;
            border-radius: 4px;
            margin-top: 14px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 6px;
            color: #cccccc;
        }
        QLabel#shapeInfo {
            color: #e0e0e0;
            font-size: 12px;
            background: #2a2a2a;
            border: 1px solid #444444;
            border-radius: 3px;
            padding: 6px;
        }
        QComboBox, QLineEdit {
            padding: 3px 6px;
            border: 1px solid #555555;
            border-radius: 3px;
            background: #2d2d2d;
            color: #e0e0e0;
        }
        QPushButton#applyBtn {
            background: #2d5a88;
            color: white;
            border: 1px solid #3a7bc8;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 12px;
            font-weight: bold;
            min-height: 20px;
        }
        QPushButton#applyBtn:hover {
            background: #3a7bc8;
        }
        QPushButton#applyBtn:pressed {
            background: #1e4060;
        }
    )";

    QWidget *shapeScriptTab = new QWidget;
    shapeScriptTab->setStyleSheet(tabStyle);
    QVBoxLayout *ssLayout = new QVBoxLayout(shapeScriptTab);
    ssLayout->setContentsMargins(8, 8, 8, 8);
    ssLayout->setSpacing(6);

    // ── 图形属性分组 ──
    QGroupBox *shapeGroup = new QGroupBox("图形属性");
    QVBoxLayout *shapeLayout = new QVBoxLayout(shapeGroup);
    shapeLayout->setContentsMargins(6, 12, 6, 6);
    shapeLayout->setSpacing(4);

    QLabel *shapeInfo = new QLabel("点击图形查看属性");
    shapeInfo->setObjectName("shapeInfo");
    shapeInfo->setWordWrap(true);
    shapeInfo->setMinimumHeight(50);
    shapeLayout->addWidget(shapeInfo);
    ssLayout->addWidget(shapeGroup);

    // ── 脚本配置分组 ──
    QGroupBox *scriptGroup = new QGroupBox("脚本配置");
    QVBoxLayout *scriptLayout = new QVBoxLayout(scriptGroup);
    scriptLayout->setContentsMargins(6, 12, 6, 6);
    scriptLayout->setSpacing(6);

    // 脚本选择
    QHBoxLayout *scriptRow = new QHBoxLayout;
    scriptRow->setSpacing(6);
    QLabel *scriptLabel = new QLabel("脚本文件");
    scriptLabel->setStyleSheet("color: #cccccc; font-size: 12px; font-weight: bold;");
    QComboBox *shapeScriptCombo = new QComboBox;
    shapeScriptCombo->setEditable(true);
    shapeScriptCombo->setPlaceholderText("选择 .lua 脚本...");
    shapeScriptCombo->setMinimumHeight(26);
    scriptRow->addWidget(scriptLabel);
    scriptRow->addWidget(shapeScriptCombo, 1);
    scriptLayout->addLayout(scriptRow);

    // 参数 1-3
    QLineEdit *paramName[3], *paramValue[3];
    QString paramLabels[3] = {"参数 1", "参数 2", "参数 3"};
    for (int i = 0; i < 3; ++i) {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(6);
        QLabel *lbl = new QLabel(paramLabels[i]);
        lbl->setFixedWidth(55);
        lbl->setStyleSheet("color: #aaaaaa; font-size: 11px;");
        QLineEdit *ne = new QLineEdit;
        ne->setPlaceholderText("名称");
        ne->setMinimumHeight(24);
        QLineEdit *ve = new QLineEdit;
        ve->setPlaceholderText("数值");
        ve->setMinimumHeight(24);
        row->addWidget(lbl);
        row->addWidget(ne, 1);
        row->addWidget(ve, 1);
        scriptLayout->addLayout(row);
        paramName[i] = ne;
        paramValue[i] = ve;
    }

    // 应用到图形
    QPushButton *applyShapeScript = new QPushButton("✓ 应用到图形");
    applyShapeScript->setObjectName("applyBtn");
    applyShapeScript->setMinimumHeight(30);
    scriptLayout->addSpacing(4);
    scriptLayout->addWidget(applyShapeScript);
    ssLayout->addWidget(scriptGroup);

    // 弹性空间
    ssLayout->addStretch();

    tabs->addTab(shapeScriptTab, "图形+脚本");

    // 刷新脚本下拉列表
    auto refreshScriptCombo = [shapeScriptCombo]() {
        shapeScriptCombo->clear();
        QString scriptsDir = AppPaths::resolveDataDir("data/scripts");
        QDir dir(scriptsDir);
        auto files = dir.entryList({"*.lua"}, QDir::Files);
        for (const auto &f : files)
            shapeScriptCombo->addItem(f);
    };
    refreshScriptCombo();
    // 切换到该页签时刷新
    QObject::connect(tabs, &QTabWidget::currentChanged, shapeScriptCombo, [refreshScriptCombo, tabs](int index) {
        if (tabs->tabText(index) == "图形+脚本")
            refreshScriptCombo();
    });

    // right vertical splitter containing top chart area and text
    QSplitter *rightSplit = new QSplitter(Qt::Vertical, &mainWindow);
    rightSplit->addWidget(topSplit);
    rightSplit->addWidget(tabs);
    rightSplit->setStretchFactor(0, 8);
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
    LuaScriptEngine *luaEngine = new LuaScriptEngine(&mainWindow);
    luaEngine->setKLineWidget(k);
    if (!luaEngine->initialize()) {
        logText->append("LuaScriptEngine initialize failed: " + luaEngine->lastError());
    } else {
        logText->append("LuaScriptEngine initialized");
    }

    // 连接 K 线更新信号到脚本引擎（通过 requestBarEvent 实现跨线程安全调度）
    QObject::connect(k, &KLineWidget::candleUpdated, k,
        [luaEngine](const Candle &candle, bool isNewBar) {
        if (!luaEngine->klineWidget()) return;
        luaEngine->requestBarEvent(
            luaEngine->klineWidget()->symbol(),
            luaEngine->klineWidget()->baseMinutes(),
            candle, isNewBar);
    });

    // shapesLoaded 信号：当从文件加载完图形后，加载关联的脚本
    QObject::connect(k, &KLineWidget::shapesLoaded, k,
        [k, luaEngine]() {
        const QString &symbol = k->symbol();
        int tf = k->baseMinutes();
        if (symbol.isEmpty() || tf <= 0) return;

        // 先卸载该品种/周期下已绑定的所有旧脚本
        luaEngine->unloadByBinding(symbol, tf);

        // 遍历 shapes，加载关联的脚本
        const auto &shapes = k->shapes();
        for (const auto &shape : shapes) {
            if (!shape.scriptName.isEmpty()) {
                QString scriptFile = shape.scriptName;
                if (scriptFile.endsWith(".lua", Qt::CaseInsensitive)) {
                    scriptFile = scriptFile.left(scriptFile.length() - 4);
                }
                ScriptBinding binding;
                binding.symbol = symbol;
                binding.timeframe = tf;
                luaEngine->loadScript(scriptFile, shape.scriptParams, binding);
            }
        }
    });

    // 应用到图形按钮
    QObject::connect(applyShapeScript, &QPushButton::clicked,
        [k, luaEngine, shapeScriptCombo, paramName, paramValue]() {
        int selIdx = k->selectedShapeIndex();
        if (selIdx < 0) return;
        auto &shapes = const_cast<QVector<KLineWidget::Shape>&>(k->shapes());

        // 如果之前关联了脚本，先卸载旧脚本避免冲突
        const QString &oldScript = shapes[selIdx].scriptName;
        if (!oldScript.isEmpty()) {
            QString oldScriptFile = oldScript;
            if (oldScriptFile.endsWith(".lua", Qt::CaseInsensitive)) {
                oldScriptFile = oldScriptFile.left(oldScriptFile.length() - 4);
            }
            luaEngine->unloadScript(oldScriptFile);
        }

        shapes[selIdx].scriptName = shapeScriptCombo->currentText().trimmed();
        // 拼装参数 JSON
        QStringList params;
        for (int i = 0; i < 3; ++i) {
            QString n = paramName[i]->text().trimmed();
            QString v = paramValue[i]->text().trimmed();
            if (!n.isEmpty() && !v.isEmpty())
                params << QStringLiteral("\"%1\":%2").arg(n, v);
        }
        QString scriptParams = "{" + params.join(",") + "}";
        shapes[selIdx].scriptParams = scriptParams;
        k->setShapes(shapes);

        // 加载新脚本到引擎
        QString scriptFile = shapes[selIdx].scriptName;
        if (!scriptFile.isEmpty()) {
            if (scriptFile.endsWith(".lua", Qt::CaseInsensitive)) {
                scriptFile = scriptFile.left(scriptFile.length() - 4);
            }
            ScriptBinding binding;
            binding.symbol = k->symbol();
            binding.timeframe = k->baseMinutes();
            luaEngine->loadScript(scriptFile, scriptParams, binding);
        }
    });


    // find KLineWidget and create DataLoader
    KLineWidget *klineWidget = k;  // 使用上面已创建的 KLineWidget
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

        // 加载完成或失败 -> 隐藏 Loading，显示"暂无数据"（如果数据为空）
        // 同时加载 shapes 关联的脚本（先清空旧的脚本绑定，再根据新 shapes 加载）
        QObject::connect(loader, &DataLoader::loadFinished, klineWidget,
            [klineWidget, luaEngine](const QString &symbol, int tf, bool success) {
            Q_UNUSED(success)
            klineWidget->hideLoading();

            // 先卸载该品种/周期下已绑定的所有旧脚本
            luaEngine->unloadByBinding(symbol, tf);

            // 遍历 shapes，加载关联的脚本
            const auto &shapes = klineWidget->shapes();
            for (const auto &shape : shapes) {
                if (!shape.scriptName.isEmpty()) {
                    // 提取文件名（去掉路径），例如 "ma_cross.lua" -> "ma_cross"
                    QString scriptFile = shape.scriptName;
                    if (scriptFile.endsWith(".lua", Qt::CaseInsensitive)) {
                        scriptFile = scriptFile.left(scriptFile.length() - 4); // 去掉 .lua 后缀
                    }
                    ScriptBinding binding;
                    binding.symbol = symbol;
                    binding.timeframe = tf;
                    luaEngine->loadScript(scriptFile, shape.scriptParams, binding);
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

        // 推送数据到达 -> 写入 KBarManager + 实时更新 K 线图（跨线程安全）
        // 只有该 (品种, 周期) 状态为 Loaded 时才接受推送
        QObject::connect(loader, &DataLoader::pushDataReady, klineWidget,
            [klineWidget, loader](const QString &symbol, int timeFrame,
                          uint64_t time, double open, double high,
                          double low, double close, double volume)
        {
            // 检查该周期状态是否为 Loaded，未就绪时不处理推送（避免在初始化时混入零散数据）
            if (!loader->canAcceptPush(symbol, timeFrame)) {
                return;
            }

            // 写入 KBarManager（供后续切换周期使用）
            KBar kb;
            kb.symbol = symbol.toStdString();
            kb.timeFrame = timeFrame;
            kb.time = time;
            kb.open = open;
            kb.high = high;
            kb.low = low;
            kb.close = close;
            kb.volume = static_cast<uint64_t>(volume);
            KBarManager::instance().add_kbar(kb);

            // 只处理当前正在显示的品种和周期，更新 K 线图
            if (symbol != klineWidget->symbol() || timeFrame != klineWidget->baseMinutes()) {
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

            // 更新 K 线图
            klineWidget->updateRealtimeCandle(c);
        });

        // 启动预加载：扫描 data/shapes/ 目录，找出所有关联了脚本的品种+周期，按各自周期预加载
        loader->preloadAllScriptSymbols();
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
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, kjw, &IndicatorWidget::setCrosshairIndex);
    QObject::connect(k, &KLineWidget::crosshairIndexChanged, macdw, &MacdWidget::setCrosshairIndex);

    // 连接图形选择信号 -> 更新图形信息页签和脚本下拉/参数
    QObject::connect(k, &KLineWidget::shapeSelected, k, [k, shapeInfo, tabs, shapeScriptCombo, paramName, paramValue, refreshScriptCombo](int index) {
        if (index < 0 || index >= k->shapes().size()) {
            shapeInfo->setText("未选中图形");
            return;
        }
        const auto &s = k->shapes()[index];
        QString info;
        info += QStringLiteral("名称: %1 | ").arg(s.name);
        info += QStringLiteral("类型: %1 | ").arg(s.type);
        info += QStringLiteral("颜色: %1\n").arg(s.color.isValid() ? s.color.name() : "#FFFFFF");
        info += QStringLiteral("坐标: (%1,%2)->(%3,%4)")
                    .arg(s.candleIdx1).arg(s.price1, 0, 'f', 2)
                    .arg(s.candleIdx2).arg(s.price2, 0, 'f', 2);
        // 脚本信息
        if (!s.scriptName.isEmpty()) {
            info += QStringLiteral(" 脚本:%1").arg(s.scriptName);
        }
        // 交易信息
        if (s.tradePrice != 0.0) {
            info += QStringLiteral(" 成交价:%1").arg(s.tradePrice, 0, 'f', 2);
        }
        if (s.profit != 0.0) {
            info += QStringLiteral(" 盈亏:%1%2").arg(s.profit >= 0 ? "+" : "").arg(s.profit, 0, 'f', 2);
        }
        shapeInfo->setText(info);
        // 自动填充脚本下拉和参数
        refreshScriptCombo();
        if (!s.scriptName.isEmpty()) {
            int ci = shapeScriptCombo->findText(s.scriptName);
            if (ci >= 0) shapeScriptCombo->setCurrentIndex(ci);
            else shapeScriptCombo->setCurrentText(s.scriptName);
        } else {
            shapeScriptCombo->setCurrentIndex(-1);
        }
        // 解析 scriptParams JSON 填充参数行
        for (int i = 0; i < 3; ++i) {
            paramName[i]->clear();
            paramValue[i]->clear();
        }
        if (!s.scriptParams.isEmpty() && s.scriptParams.contains(":")) {
            QJsonObject jo = QJsonDocument::fromJson(s.scriptParams.toUtf8()).object();
            int pi = 0;
            for (auto it = jo.begin(); it != jo.end() && pi < 3; ++it, ++pi) {
                paramName[pi]->setText(it.key());
                paramValue[pi]->setText(QString::number((*it).toDouble()));
            }
        }
        // 自动切换到图形+脚本页签
        tabs->setCurrentIndex(1);
    });

    // load data and initialize
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

    // ----- 测试飞书（使用 NetCore 的 FeishuSender）-----
    toolbar->addSeparator();
    QAction *aFeishu = toolbar->addAction(QStringLiteral("测试飞书"));
    QObject::connect(aFeishu, &QAction::triggered, [logText]() {
        // 从 config/server.json 读取 webhook URL
        QString configPath = AppPaths::resolveDataDir("config") + "/server.json";
        QFile f(configPath);
        std::string webhookUrl;
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            webhookUrl = doc.object().value("feishu_webhook").toString().toStdString();
            f.close();
        }
        if (webhookUrl.empty()) {
            logText->append("[飞书] 未配置 Webhook URL，请在 设置→飞书 中配置");
            return;
        }
        QString msg = QStringLiteral("WinLine 测试消息 - %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        logText->append(QStringLiteral("[飞书] 正在发送: %1").arg(msg));
        // 使用 NetCore FeishuSender 同步发送（超时 5 秒）
        auto sender = std::make_shared<FeishuSender>();
        if (!sender->Init(webhookUrl)) {
            logText->append("[飞书] FeishuSender Init 失败");
            return;
        }
        bool ok = sender->SendTextSync(msg.toStdString(), 5000);
        if (ok)
            logText->append("[飞书] 发送成功");
        else
            logText->append("[飞书] 发送失败");
    });

    mainWindow.resize(1000, 700);

    mainWindow.show();
    return a.exec();
}
