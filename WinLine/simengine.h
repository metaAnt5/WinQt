#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include "klinewidget.h"

// ============================================================
// SimEngine - 模拟引擎
// 定时器驱动，按设定的倍速逐条播放 K 线数据
// ============================================================
class SimEngine : public QObject {
    Q_OBJECT
public:
    explicit SimEngine(QObject *parent = nullptr);

    // 加载数据
    void setData(const QVector<Candle> &data);
    bool hasData() const { return !m_data.isEmpty(); }
    int totalCount() const { return m_data.size(); }
    int currentIndex() const { return m_pos; }

    // 播放控制
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return m_playing; }

    // 跳转
    void seekTo(int index);
    void seekPercent(double percent); // 0.0 ~ 1.0

    // 速度控制（倍数，如 1, 2, 5, 10, 50, 100）
    void setSpeed(int multiplier);
    int speed() const { return m_speed; }

    // 底层时间粒度（毫秒），默认 1000ms
    void setTickInterval(int ms);
    int tickInterval() const { return m_tickMs; }

signals:
    // 每 tick 推送一根 K 线
    void candleReady(int index, const Candle &candle);
    // 进度变化（当前索引/总数）
    void progressChanged(int index, int total);
    // 播放状态变化
    void stateChanged(bool playing);
    // 播放结束
    void finished();

private slots:
    void onTick();

private:
    QVector<Candle> m_data;
    int m_pos = 0;
    int m_speed = 1;
    int m_tickMs = 1000;
    bool m_playing = false;
    QTimer *m_timer;
};
