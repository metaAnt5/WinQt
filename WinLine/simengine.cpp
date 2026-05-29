#include "simengine.h"
#include <cmath>

SimEngine::SimEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &SimEngine::onTick);
}

void SimEngine::setData(const QVector<Candle> &data)
{
    stop();
    m_data = data;
    m_pos = 0;
    emit progressChanged(m_pos, totalCount());
}

void SimEngine::play()
{
    if (m_data.isEmpty() || m_pos >= m_data.size()) return;
    m_playing = true;
    int interval = std::max(1, m_tickMs / m_speed);
    m_timer->setInterval(interval);
    m_timer->start();
    emit stateChanged(true);
}

void SimEngine::pause()
{
    m_playing = false;
    m_timer->stop();
    emit stateChanged(false);
}

void SimEngine::stop()
{
    pause();
    m_pos = 0;
    emit progressChanged(m_pos, totalCount());
    emit stateChanged(false);
}

void SimEngine::seekTo(int index)
{
    if (index < 0) index = 0;
    if (index >= m_data.size()) index = m_data.size() - 1;
    m_pos = index;
    emit progressChanged(m_pos, totalCount());
    // push the candle at the seek position
    if (!m_data.isEmpty() && m_pos < m_data.size()) {
        emit candleReady(m_pos, m_data[m_pos]);
    }
}

void SimEngine::seekPercent(double percent)
{
    if (percent < 0.0) percent = 0.0;
    if (percent > 1.0) percent = 1.0;
    int idx = static_cast<int>(percent * (m_data.size() - 1));
    seekTo(idx);
}

void SimEngine::setSpeed(int multiplier)
{
    m_speed = std::max(1, multiplier);
    // if currently playing, restart timer with new interval
    if (m_playing) {
        m_timer->stop();
        int interval = std::max(1, m_tickMs / m_speed);
        m_timer->setInterval(interval);
        m_timer->start();
    }
}

void SimEngine::setTickInterval(int ms)
{
    m_tickMs = std::max(1, ms);
    if (m_playing) {
        m_timer->stop();
        int interval = std::max(1, m_tickMs / m_speed);
        m_timer->setInterval(interval);
        m_timer->start();
    }
}

void SimEngine::onTick()
{
    if (m_data.isEmpty()) {
        pause();
        return;
    }

    if (m_pos >= m_data.size()) {
        // finished
        pause();
        emit finished();
        return;
    }

    // emit current candle
    emit candleReady(m_pos, m_data[m_pos]);
    emit progressChanged(m_pos, totalCount());

    m_pos++;
}
