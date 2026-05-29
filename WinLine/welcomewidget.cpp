#include "welcomewidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QDateTime>
#include <QPainter>
#include <QLinearGradient>
#include <QApplication>

WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
{
    // ============================================================
    // 整体布局：垂直居中，上下留白
    // ============================================================
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(40, 40, 40, 40);

    // 顶部弹性空间
    outerLayout->addStretch(2);

    // ---- 中间内容区域 ----
    auto *centerWidget = new QWidget;
    centerWidget->setObjectName("welcomeCenter");
    centerWidget->setStyleSheet(
        "#welcomeCenter { background: transparent; }"
    );
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setAlignment(Qt::AlignCenter);
    centerLayout->setSpacing(20);

    // Logo / 标题
    m_titleLabel = new QLabel(QStringLiteral("📊 WinLine"));
    QFont titleFont = QFont("Microsoft YaHei UI", 36, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: #FFD700; background: transparent;");

    // 副标题 - 用描述文字
    m_subtitleLabel = new QLabel(QStringLiteral("专业级 K 线图表分析工具"));
    QFont subFont = QFont("Microsoft YaHei UI", 16);
    m_subtitleLabel->setFont(subFont);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet("color: #B0C4DE; background: transparent;");

    // 实时时钟
    m_timeLabel = new QLabel;
    QFont clockFont = QFont("Consolas", 24, QFont::Bold);
    m_timeLabel->setFont(clockFont);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet("color: #87CEEB; background: transparent;");

    // 提示文字
    m_hintLabel = new QLabel(QStringLiteral("💡 双击左侧品种开始加载数据  |  切换周期查看不同时间框架"));
    QFont hintFont = QFont("Microsoft YaHei UI", 11);
    m_hintLabel->setFont(hintFont);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: #A9A9A9; background: transparent; padding: 8px;");

    // 行情状态指示
    m_statusLabel = new QLabel(QStringLiteral("● 等待连接"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "color: #FF8C00; background: rgba(255,140,0,0.12); "
        "border: 1px solid rgba(255,140,0,0.3); border-radius: 12px; "
        "padding: 6px 20px; font-size: 13px;"
    );
    m_statusLabel->setFixedHeight(32);

    // 组装中间内容
    centerLayout->addStretch(1);
    centerLayout->addWidget(m_titleLabel);
    centerLayout->addSpacing(4);
    centerLayout->addWidget(m_subtitleLabel);
    centerLayout->addSpacing(16);
    centerLayout->addWidget(m_timeLabel);
    centerLayout->addSpacing(24);
    centerLayout->addWidget(m_hintLabel);
    centerLayout->addSpacing(8);
    // 状态标签放到一个水平居中容器
    auto *statusRow = new QHBoxLayout;
    statusRow->setAlignment(Qt::AlignCenter);
    statusRow->addWidget(m_statusLabel);
    centerLayout->addLayout(statusRow);
    centerLayout->addStretch(1);

    outerLayout->addWidget(centerWidget, 0, Qt::AlignCenter);

    // 底部预留空间
    outerLayout->addStretch(3);

    // ============================================================
    // 时钟定时器
    // ============================================================
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &WelcomeWidget::updateTime);
    m_clockTimer->start(1000);
    updateTime(); // 立即显示
}

void WelcomeWidget::updateTime()
{
    QDateTime now = QDateTime::currentDateTime();
    QString timeStr = now.toString("yyyy-MM-dd  HH:mm:ss  dddd");
    // 替换星期几为中文
    timeStr.replace("Monday",  "星期一");
    timeStr.replace("Tuesday",  "星期二");
    timeStr.replace("Wednesday","星期三");
    timeStr.replace("Thursday", "星期四");
    timeStr.replace("Friday",   "星期五");
    timeStr.replace("Saturday", "星期六");
    timeStr.replace("Sunday",   "星期日");
    m_timeLabel->setText(timeStr);
}

void WelcomeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 深色渐变背景
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor("#0D1117"));
    bg.setColorAt(0.5, QColor("#161B22"));
    bg.setColorAt(1.0, QColor("#0D1117"));
    p.fillRect(rect(), bg);

    // 装饰：右上角网格线
    p.setPen(QPen(QColor(255, 255, 255, 12), 1));
    for (int x = 0; x < width(); x += 60) {
        p.drawLine(x, 0, x, height());
    }
    for (int y = 0; y < height(); y += 60) {
        p.drawLine(0, y, width(), y);
    }

    // 底部装饰文字（水印风格）
    p.setPen(QColor(255, 255, 255, 25));
    QFont watermarkFont = QFont("Consolas", 10);
    p.setFont(watermarkFont);
    p.drawText(rect().adjusted(12, 0, -12, -8), Qt::AlignBottom | Qt::AlignRight,
               "WinLine v0.1  |  Powered by Qt & NetCore");
}
