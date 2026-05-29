#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>

class WelcomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget *parent = nullptr);

private:
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_timeLabel;
    QLabel *m_hintLabel;
    QLabel *m_statusLabel;
    QTimer *m_clockTimer;

    void updateTime();
    void paintEvent(QPaintEvent *event) override;
};
