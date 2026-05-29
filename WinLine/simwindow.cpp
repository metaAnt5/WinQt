#include "simwindow.h"
#include "simengine.h"
#include "simreader.h"
#include "klinewidget.h"
#include "volumewidget.h"
#include "indicatorwidget.h"
#include "macdwidget.h"
#include "clickfilter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QToolBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QPen>
#include <QPolygonF>
#include <QFont>
#include <functional>

SimWindow::SimWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(new SimEngine(this))
{
    setWindowTitle(QStringLiteral("模拟回放 - SimWindow"));
    resize(1100, 750);
    setupUi();
    connectSignals();
}

SimWindow::~SimWindow()
{
    if (m_engine) m_engine->stop();
}

// ============================================================
// 构建界面
// ============================================================
void SimWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- 控制栏 ----
    QHBoxLayout *ctrlLayout = new QHBoxLayout;
    ctrlLayout->setSpacing(6);

    m_openBtn = new QPushButton(QStringLiteral("打开文件"));
    m_openBtn->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ctrlLayout->addWidget(m_openBtn);

    ctrlLayout->addWidget(new QLabel(QStringLiteral("读取器:")));

    m_readerCombo = new QComboBox;
    m_readerCombo->addItem(QStringLiteral("通用 CSV 格式"), QVariant::fromValue(0));
    m_readerCombo->addItem(QStringLiteral("东方财富 格式"), QVariant::fromValue(1));
    m_readerCombo->addItem(QStringLiteral("福汇 FXCM 格式"), QVariant::fromValue(2));
    m_readerCombo->addItem(QStringLiteral("MT4 CSV 格式"), QVariant::fromValue(3));
    m_readerCombo->addItem(QStringLiteral("文华财经 WH 格式"), QVariant::fromValue(4));
    m_readerCombo->setCurrentIndex(0);
    ctrlLayout->addWidget(m_readerCombo);

    m_playBtn = new QPushButton(QStringLiteral("▶ 播放"));
    m_playBtn->setEnabled(false);
    m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ctrlLayout->addWidget(m_playBtn);

    m_stopBtn = new QPushButton(QStringLiteral("⏹ 停止"));
    m_stopBtn->setEnabled(false);
    m_stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ctrlLayout->addWidget(m_stopBtn);

    ctrlLayout->addWidget(new QLabel(QStringLiteral("速度:")));

    m_speedCombo = new QComboBox;
    m_speedCombo->addItem(QStringLiteral("1x"), 1);
    m_speedCombo->addItem(QStringLiteral("2x"), 2);
    m_speedCombo->addItem(QStringLiteral("5x"), 5);
    m_speedCombo->addItem(QStringLiteral("10x"), 10);
    m_speedCombo->addItem(QStringLiteral("50x"), 50);
    m_speedCombo->addItem(QStringLiteral("100x"), 100);
    m_speedCombo->setCurrentIndex(0);
    ctrlLayout->addWidget(m_speedCombo);

    ctrlLayout->addStretch();

    m_progressLabel = new QLabel(QStringLiteral("0 / 0"));
    ctrlLayout->addWidget(m_progressLabel);

    mainLayout->addLayout(ctrlLayout);

    // ---- 进度条 ----
    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setValue(0);
    m_seekSlider->setEnabled(false);
    mainLayout->addWidget(m_seekSlider);

    // ---- 图表区域 ----
    m_kline = new KLineWidget;

    // 技术指标堆栈 (Volume / KDJ / MACD)
    m_indicatorStack = new QStackedWidget;
    m_vol  = new VolumeWidget;
    m_ind  = new IndicatorWidget;
    m_macd = new MacdWidget;
    m_indicatorStack->addWidget(m_vol);   // 0
    m_indicatorStack->addWidget(m_ind);   // 1
    m_indicatorStack->addWidget(m_macd);  // 2
    m_indicatorStack->setCurrentIndex(0);

    // 安装双击切换过滤器
    ClickFilter *cf = new ClickFilter(m_indicatorStack, this);
    m_indicatorStack->installEventFilter(cf);

    QSplitter *vSplit = new QSplitter(Qt::Vertical);
    vSplit->addWidget(m_kline);
    vSplit->addWidget(m_indicatorStack);
    vSplit->setStretchFactor(0, 5);
    vSplit->setStretchFactor(1, 2);
    vSplit->setSizes({500, 200});

    mainLayout->addWidget(vSplit, 1);

    // ---- 日志 ----
    m_logText = new QTextEdit;
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(120);
    m_logText->setPlainText(QStringLiteral("模拟回放日志\n"));
    mainLayout->addWidget(m_logText);

    // ---- 画图工具栏 ----
    auto makeIcon = [](std::function<void(QPainter&)> draw) -> QIcon {
        QPixmap px(24, 24);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        draw(p);
        p.end();
        return QIcon(px);
    };

    // Normal: 鼠标指针
    QIcon iconNormal = makeIcon([](QPainter &p){
        p.setPen(QPen(Qt::white, 1.5));
        p.drawLine(4,4, 4,20);
        p.drawLine(4,4, 16,12);
        p.drawLine(4,12, 12,16);
        p.drawLine(16,12, 20,20);
    });

    // Line: 无限延长线
    QIcon iconLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,50), 2));
        p.drawLine(3,21, 21,3);
        p.setBrush(QColor(200,200,50));
        p.drawEllipse(QPoint(3,21), 2,2);
        p.drawEllipse(QPoint(21,3), 2,2);
    });

    // Trend: 射线
    QIcon iconTrend = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(100,200,255), 2));
        p.drawLine(4,20, 18,6);
        p.drawLine(18,6, 12,6);
        p.drawLine(18,6, 18,12);
        p.setBrush(QColor(100,200,255));
        p.drawEllipse(QPoint(4,20), 2,2);
    });

    // HLine: 水平线
    QIcon iconHLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,255), 2));
        p.drawLine(2,12, 22,12);
        p.drawLine(2,10, 2,14);
        p.drawLine(22,10, 22,14);
    });

    // VLine: 垂直线
    QIcon iconVLine = makeIcon([](QPainter &p){
        p.setPen(QPen(QColor(200,200,255), 2));
        p.drawLine(12,2, 12,22);
        p.drawLine(10,2, 14,2);
        p.drawLine(10,22, 14,22);
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
        p.drawRect(5,9, 14,13);
        p.drawLine(3,9, 21,9);
        p.drawLine(9,9, 9,5);
        p.drawLine(15,9, 15,5);
        p.drawLine(9,5, 15,5);
        p.drawLine(8,13, 16,13);
        p.drawLine(8,17, 16,17);
    });

    m_drawToolbar = new QToolBar(this);
    QAction *aNormal = m_drawToolbar->addAction(iconNormal, "");
    QAction *aLine  = m_drawToolbar->addAction(iconLine, "");
    QAction *aTrend = m_drawToolbar->addAction(iconTrend, "");
    QAction *aHLine = m_drawToolbar->addAction(iconHLine, "");
    QAction *aVLine = m_drawToolbar->addAction(iconVLine, "");
    QAction *aText  = m_drawToolbar->addAction(iconText, "");
    QAction *aUp    = m_drawToolbar->addAction(iconUp, "");
    QAction *aDown  = m_drawToolbar->addAction(iconDown, "");
    QAction *aDelete= m_drawToolbar->addAction(iconDelete, "");
    QAction *aClear = m_drawToolbar->addAction(iconClear, "");
    addToolBar(m_drawToolbar);

    // 只连到 m_kline，不碰任何其他窗口的信号
    QObject::connect(aNormal, &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_None); });
    QObject::connect(aLine,   &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_Line); });
    QObject::connect(aTrend,  &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_Trend); });
    QObject::connect(aHLine,  &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_HLine); });
    QObject::connect(aVLine,  &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_VLine); });
    QObject::connect(aText,   &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_Text); });
    QObject::connect(aUp,     &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_GestureUp); });
    QObject::connect(aDown,   &QAction::triggered, [this](){ m_kline->setToolMode(KLineWidget::Tool_GestureDown); });
    QObject::connect(aDelete, &QAction::triggered, [this](){ m_kline->deleteSelectedShape(); });
    QObject::connect(aClear,  &QAction::triggered, [this](){ m_kline->clearShapes(); });

    // ---- 状态栏 ----
    statusBar()->showMessage(QStringLiteral("就绪 - 请打开 CSV 文件"));
}

// ============================================================
// 信号连接
// ============================================================
void SimWindow::connectSignals()
{
    // 打开文件
    connect(m_openBtn, &QPushButton::clicked, this, &SimWindow::onOpenFile);

    // 播放/暂停
    connect(m_playBtn, &QPushButton::clicked, this, &SimWindow::onPlayPause);

    // 停止
    connect(m_stopBtn, &QPushButton::clicked, this, &SimWindow::onStop);

    // 速度切换
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimWindow::onSpeedChanged);

    // 进度条
    connect(m_seekSlider, &QSlider::sliderPressed, this, &SimWindow::onSeekSliderPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &SimWindow::onSeekSliderReleased);
    connect(m_seekSlider, &QSlider::valueChanged, this, &SimWindow::onSeekSliderValueChanged);

    // 引擎信号
    connect(m_engine, &SimEngine::candleReady, this, &SimWindow::onCandleReady);
    connect(m_engine, &SimEngine::progressChanged, this, &SimWindow::onProgressChanged);
    connect(m_engine, &SimEngine::finished, this, &SimWindow::onPlaybackFinished);
    connect(m_engine, &SimEngine::stateChanged, this, [this](bool playing) {
        m_playBtn->setText(playing ? QStringLiteral("⏸ 暂停") : QStringLiteral("▶ 播放"));
        m_playBtn->setIcon(style()->standardIcon(
            playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    });

    // K线图数据变化 -> 同步指标
    connect(m_kline, &KLineWidget::dataAggregated, m_vol, &VolumeWidget::setData);
    connect(m_kline, &KLineWidget::dataAggregated, m_ind, &IndicatorWidget::setData);
    connect(m_kline, &KLineWidget::dataAggregated, m_macd, &MacdWidget::setData);

    connect(m_kline, &KLineWidget::viewportChanged, m_vol, &VolumeWidget::setViewport);
    connect(m_kline, &KLineWidget::viewportChanged, m_ind, &IndicatorWidget::setViewport);
    connect(m_kline, &KLineWidget::viewportChanged, m_macd, &MacdWidget::setViewport);

    connect(m_kline, &KLineWidget::crosshairIndexChanged, m_vol, &VolumeWidget::setCrosshairIndex);
    connect(m_kline, &KLineWidget::crosshairIndexChanged, m_ind, &IndicatorWidget::setCrosshairIndex);
    connect(m_kline, &KLineWidget::crosshairIndexChanged, m_macd, &MacdWidget::setCrosshairIndex);
}

// ============================================================
// 槽函数
// ============================================================

void SimWindow::onOpenFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 CSV 数据文件"), QString(),
        QStringLiteral("CSV Files (*.csv *.txt);;All Files (*)"));
    if (file.isEmpty()) return;

    // 根据选择的读取器解析文件
    int readerIdx = m_readerCombo->currentIndex();
    QVector<Candle> data;

    bool ok = false;
    if (readerIdx == 0) {
        GenericCsvReader reader;
        ok = reader.readFile(file, data);
    } else if (readerIdx == 1) {
        EastMoneyCsvReader reader;
        ok = reader.readFile(file, data);
    } else if (readerIdx == 2) {
        FxcmCsvReader reader;
        ok = reader.readFile(file, data);
    } else if (readerIdx == 3) {
        Mt4CsvReader reader;
        ok = reader.readFile(file, data);
    } else if (readerIdx == 4) {
        WhCsvReader reader;
        ok = reader.readFile(file, data);
    }

    if (!ok || data.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("读取失败"),
                             QStringLiteral("无法解析文件，请检查格式或选择正确的读取器。"));
        return;
    }

    // 成功读取
    m_allData = data;
    QFileInfo fi(file);
    m_symbol = fi.completeBaseName();

    // 更新窗口标题
    setWindowTitle(QStringLiteral("模拟回放 - %1 (%2 根K线)").arg(m_symbol).arg(data.size()));

    m_logText->append(QStringLiteral("已加载: %1  共 %2 根 K 线").arg(file).arg(data.size()));

    resetSimulation();

    // 启用控件
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    m_seekSlider->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("已加载 %1 根 K 线，按 ▶ 开始回放").arg(data.size()));
}

void SimWindow::onPlayPause()
{
    if (m_engine->isPlaying()) {
        m_engine->pause();
        statusBar()->showMessage(QStringLiteral("已暂停"));
    } else {
        m_engine->play();
        statusBar()->showMessage(QStringLiteral("播放中..."));
    }
}

void SimWindow::onStop()
{
    m_engine->stop();
    // 清空 K 线图显示
    m_kline->setData(QVector<Candle>());
    statusBar()->showMessage(QStringLiteral("已停止"));
    m_logText->append(QStringLiteral("模拟已停止"));
}

void SimWindow::onSpeedChanged(int idx)
{
    int speed = m_speedCombo->itemData(idx).toInt();
    m_engine->setSpeed(speed);
    statusBar()->showMessage(
        QStringLiteral("速度: %1x").arg(speed));
}

void SimWindow::onProgressChanged(int index, int total)
{
    m_progressLabel->setText(QStringLiteral("%1 / %2").arg(index).arg(total));
    if (!m_seeking) {
        // 只在非拖拽状态下更新滑块
        int percent = (total > 0) ? (index * 1000 / total) : 0;
        m_seekSlider->setValue(percent);
    }
}

void SimWindow::onCandleReady(int index, const Candle &candle)
{
    Q_UNUSED(candle)
    // 每次进一根新的 K 线，就把它追加到 K 线图里
    // 构建从 0 到当前索引的数据子集
    QVector<Candle> subset = m_allData.mid(0, index + 1);
    m_kline->setData(subset);
    // 自动滚动到最新
    // KLineWidget 的 paintEvent 会自动显示最新数据
}

void SimWindow::onPlaybackFinished()
{
    statusBar()->showMessage(QStringLiteral("回放完成"));
    m_logText->append(QStringLiteral("回放结束"));
    m_playBtn->setText(QStringLiteral("▶ 重播"));
}

void SimWindow::onSeekSliderPressed()
{
    m_seeking = true;
    if (m_engine->isPlaying()) {
        m_engine->pause();
    }
}

void SimWindow::onSeekSliderReleased()
{
    m_seeking = false;
    int val = m_seekSlider->value();
    double percent = val / 1000.0;
    m_engine->seekPercent(percent);
    statusBar()->showMessage(
        QStringLiteral("跳转到 %1%").arg(static_cast<int>(percent * 100)));
}

void SimWindow::onSeekSliderValueChanged(int value)
{
    if (m_seeking) {
        // 拖拽时显示位置
        double percent = value / 1000.0;
        int total = m_engine->totalCount();
        int idx = static_cast<int>(percent * total);
        m_progressLabel->setText(QStringLiteral("%1 / %2").arg(idx).arg(total));
    }
}

void SimWindow::resetSimulation()
{
    m_engine->setData(m_allData);
    m_kline->setData(QVector<Candle>());
    m_progressLabel->setText(QStringLiteral("0 / %1").arg(m_allData.size()));
    m_seekSlider->setValue(0);
    m_playBtn->setText(QStringLiteral("▶ 播放"));
}
