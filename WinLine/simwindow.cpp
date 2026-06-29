#include "simwindow.h"
#include "simengine.h"
#include "simreader.h"
#include "klinewidget.h"
#include "volumewidget.h"
#include "indicatorwidget.h"
#include "macdwidget.h"
#include "clickfilter.h"
#include "drawtoolbar.h"
#include "indicatorcalc.h"

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

    // ---- 画图工具栏（共用 drawtoolbar.h 实现）----
    m_drawToolbar = createDrawingToolbar(m_kline, this);
    addToolBar(m_drawToolbar);

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

// 帮助函数：从数据中检测K线周期（分钟）
static int detectBaseMinutes(const QVector<Candle> &data)
{
    if (data.size() < 2) return 1;
    qint64 secDiff = qAbs(data[0].date.secsTo(data[1].date));
    if (secDiff <= 0) return 1;
    int minutes = static_cast<int>(secDiff / 60);
    // 四舍五入到常见周期
    if (minutes <= 1) return 1;      // 1m
    if (minutes <= 3) return 1;      // 实际1m但差值略大
    if (minutes <= 7) return 5;      // 5m
    if (minutes <= 10) return 5;     // 略超
    if (minutes <= 20) return 15;    // 15m
    if (minutes <= 40) return 30;    // 30m
    if (minutes <= 90) return 60;    // 60m
    if (minutes <= 300) return 240;  // 4h
    if (minutes <= 1000) return 1440;// 日线
    return minutes;                  // 其他
}

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

    // **** 自动检测K线周期 ****
    m_baseMinutes = detectBaseMinutes(data);

    // 更新窗口标题（含周期信息）
    QString periodStr;
    if (m_baseMinutes >= 1440) periodStr = QStringLiteral("日线");
    else if (m_baseMinutes >= 240) periodStr = QStringLiteral("%1H").arg(m_baseMinutes / 60);
    else periodStr = QStringLiteral("%1min").arg(m_baseMinutes);
    setWindowTitle(QStringLiteral("模拟回放 - %1 [%2] (%3 根K线)")
        .arg(m_symbol).arg(periodStr).arg(data.size()));

    m_logText->append(QStringLiteral("已加载: %1  周期: %2  共 %3 根 K 线")
        .arg(file).arg(periodStr).arg(data.size()));

    resetSimulation();

    // 启用控件
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    m_seekSlider->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("已加载 %1 根 %2 K线，按 ▶ 开始回放").arg(data.size()).arg(periodStr));
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
    // 清除 IndicatorCalculator 缓存
    if (!m_symbol.isEmpty() && m_baseMinutes > 0) {
        IndicatorCalculator::instance().clearCache(m_symbol, m_baseMinutes);
    }
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
    if (index == 0) {
        // 首次仅初始化品种和周期设置
        if (!m_symbol.isEmpty() && m_baseMinutes > 0) {
            m_kline->setSymbol(m_symbol);
            m_kline->setConnectionStatus(true);
        }

        QVector<Candle> first;
        first.append(candle);
        // !! 必须传入正确周期，避免 setData(data)→setData(data,1) 覆盖 baseMinutes !!
        m_kline->setData(first, m_baseMinutes);

        // 首次推送：预计算指标到 IndicatorCalculator
        QVector<Candle> curData = m_kline->allData();
        if (!curData.isEmpty()) {
            IndicatorCalculator::instance().updateIndicators(m_symbol, m_baseMinutes, curData);
        }
    } else {
        // 后续只用 updateRealtimeCandle 增量更新（不再重复 setTimeframe 干扰）
        m_kline->updateRealtimeCandle(candle);

        // 实时更新 IndicatorCalculator
        QVector<Candle> curData = m_kline->allData();
        if (!curData.isEmpty()) {
            IndicatorCalculator::instance().updateIndicators(m_symbol, m_baseMinutes, curData);
        }
    }
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
    // 清除 IndicatorCalculator 缓存
    if (!m_symbol.isEmpty() && m_baseMinutes > 0) {
        IndicatorCalculator::instance().clearCache(m_symbol, m_baseMinutes);
    }
    m_progressLabel->setText(QStringLiteral("0 / %1").arg(m_allData.size()));
    m_seekSlider->setValue(0);
    m_playBtn->setText(QStringLiteral("▶ 播放"));
}
