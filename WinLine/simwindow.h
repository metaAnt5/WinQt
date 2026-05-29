#pragma once
#include <QMainWindow>
#include <QVector>
#include "klinewidget.h"

class SimEngine;
class QComboBox;
class QPushButton;
class QSlider;
class QLabel;
class QStackedWidget;
class QSplitter;
class QTextEdit;
class VolumeWidget;
class IndicatorWidget;
class MacdWidget;

class SimWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit SimWindow(QWidget *parent = nullptr);
    ~SimWindow();

private slots:
    void onOpenFile();
    void onPlayPause();
    void onStop();
    void onSpeedChanged(int idx);
    void onProgressChanged(int index, int total);
    void onCandleReady(int index, const Candle &candle);
    void onPlaybackFinished();
    void onSeekSliderPressed();
    void onSeekSliderReleased();
    void onSeekSliderValueChanged(int value);

private:
    void setupUi();
    void connectSignals();
    void resetSimulation();

    // UI 控件
    QComboBox *m_readerCombo;
    QPushButton *m_openBtn;
    QPushButton *m_playBtn;
    QPushButton *m_stopBtn;
    QComboBox *m_speedCombo;
    QSlider *m_seekSlider;
    QLabel *m_progressLabel;
    QTextEdit *m_logText;

    // 图表组件（复用）
    KLineWidget *m_kline;
    VolumeWidget *m_vol;
    IndicatorWidget *m_ind;
    MacdWidget *m_macd;
    QStackedWidget *m_indicatorStack;

    // 引擎
    SimEngine *m_engine;

    // 当前数据
    QVector<Candle> m_allData;
    int m_baseMinutes = 1;
    QString m_symbol;
    bool m_seeking = false;
};
