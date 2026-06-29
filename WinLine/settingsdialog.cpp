#include "settingsdialog.h"
#include "apppaths.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QGroupBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setMinimumSize(480, 400);
    setupUI();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget;
    mainLayout->addWidget(m_tabs);

    // ========== 飞书页签 ==========
    QWidget *feishuTab = new QWidget;
    QVBoxLayout *feishuLayout = new QVBoxLayout(feishuTab);
    m_feishuWebhookEdit = new QLineEdit;
    m_feishuWebhookEdit->setPlaceholderText("https://open.feishu.cn/open-apis/bot/v2/hook/xxx");
    feishuLayout->addWidget(new QLabel("Webhook URL:"));
    feishuLayout->addWidget(m_feishuWebhookEdit);
    feishuLayout->addStretch();
    m_tabs->addTab(feishuTab, "飞书");

    // ========== 图形颜色页签 ==========
    QWidget *colorTab = new QWidget;
    QVBoxLayout *colorLayout = new QVBoxLayout(colorTab);
    colorLayout->setSpacing(6);
    addColorRow(colorTab, colorLayout, "线段", "line");
    addColorRow(colorTab, colorLayout, "趋势线", "trend");
    addColorRow(colorTab, colorLayout, "手势-上涨", "gesture_up");
    addColorRow(colorTab, colorLayout, "手势-下跌", "gesture_down");
    addColorRow(colorTab, colorLayout, "文字", "text");
    addColorRow(colorTab, colorLayout, "水平线", "hline");
    addColorRow(colorTab, colorLayout, "垂直线", "vline");
    addColorRow(colorTab, colorLayout, "买入", "trade_buy");
    addColorRow(colorTab, colorLayout, "卖出", "trade_sell");
    addColorRow(colorTab, colorLayout, "做空", "trade_short");
    addColorRow(colorTab, colorLayout, "平仓", "trade_cover");
    colorLayout->addStretch();
    m_tabs->addTab(colorTab, "图形颜色");

    // ========== 快捷键页签 ==========
    QWidget *shortcutTab = new QWidget;
    QVBoxLayout *shortcutLayout = new QVBoxLayout(shortcutTab);
    shortcutLayout->setSpacing(6);
    addShortcutRow(shortcutTab, shortcutLayout, "线段工具", "tool_line");
    addShortcutRow(shortcutTab, shortcutLayout, "趋势线工具", "tool_trend");
    addShortcutRow(shortcutTab, shortcutLayout, "上涨手势", "tool_gesture_up");
    addShortcutRow(shortcutTab, shortcutLayout, "下跌手势", "tool_gesture_down");
    addShortcutRow(shortcutTab, shortcutLayout, "文字工具", "tool_text");
    addShortcutRow(shortcutTab, shortcutLayout, "水平线", "tool_hline");
    addShortcutRow(shortcutTab, shortcutLayout, "垂直线", "tool_vline");
    addShortcutRow(shortcutTab, shortcutLayout, "买入", "tool_trade_buy");
    addShortcutRow(shortcutTab, shortcutLayout, "卖出", "tool_trade_sell");
    addShortcutRow(shortcutTab, shortcutLayout, "做空", "tool_trade_short");
    addShortcutRow(shortcutTab, shortcutLayout, "平仓", "tool_trade_cover");
    addShortcutRow(shortcutTab, shortcutLayout, "删除图形", "delete_shape");
    shortcutLayout->addStretch();
    m_tabs->addTab(shortcutTab, "快捷键");

    // ========== 连接页签 ==========
    QWidget *connTab = new QWidget;
    QFormLayout *connLayout = new QFormLayout(connTab);
    connLayout->setSpacing(8);
    m_hostEdit = new QLineEdit;
    m_hostEdit->setPlaceholderText("127.0.0.1");
    connLayout->addRow("服务器地址:", m_hostEdit);

    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(9888);
    connLayout->addRow("端口:", m_portSpin);

    m_reconnectSpin = new QSpinBox;
    m_reconnectSpin->setRange(100, 60000);
    m_reconnectSpin->setValue(3000);
    m_reconnectSpin->setSuffix(" ms");
    connLayout->addRow("重连间隔:", m_reconnectSpin);

    m_timeoutSpin = new QSpinBox;
    m_timeoutSpin->setRange(100, 60000);
    m_timeoutSpin->setValue(10000);
    m_timeoutSpin->setSuffix(" ms");
    connLayout->addRow("请求超时:", m_timeoutSpin);

    m_autoReconnectCombo = new QComboBox;
    m_autoReconnectCombo->addItem("是", true);
    m_autoReconnectCombo->addItem("否", false);
    connLayout->addRow("自动重连:", m_autoReconnectCombo);

    connLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    m_tabs->addTab(connTab, "连接");

    // ========== 按钮 ==========
    QDialogButtonBox *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(btnBox);
    QObject::connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::addColorRow(QWidget *parent, QVBoxLayout *layout,
                                  const QString &label, const QString &shapeType)
{
    QHBoxLayout *row = new QHBoxLayout;
    row->addWidget(new QLabel(label));
    row->addStretch();

    QPushButton *btn = new QPushButton;
    btn->setFixedSize(60, 24);
    btn->setStyleSheet("background-color: #FFFFFF; border: 1px solid #888;");
    QObject::connect(btn, &QPushButton::clicked, this, [this, btn, shapeType]() {
        QColor cur = m_colors.value(shapeType, Qt::white);
        QColor c = QColorDialog::getColor(cur, this,
                    QStringLiteral("选择 %1 颜色").arg(shapeType));
        if (c.isValid()) {
            m_colors[shapeType] = c;
            btn->setStyleSheet(QString("background-color: %1; border: 1px solid #888;").arg(c.name()));
        }
    });
    m_colorButtons[shapeType] = btn;
    row->addWidget(btn);
    layout->addLayout(row);
}

void SettingsDialog::addShortcutRow(QWidget *parent, QVBoxLayout *layout,
                                     const QString &label, const QString &actionName)
{
    QHBoxLayout *row = new QHBoxLayout;
    row->addWidget(new QLabel(label));
    row->addStretch();

    QKeySequenceEdit *edit = new QKeySequenceEdit;
    edit->setFixedWidth(180);
    m_shortcutEdits[actionName] = edit;
    row->addWidget(edit);
    layout->addLayout(row);
}

// ========== Getter / Setter ==========

QString SettingsDialog::feishuWebhook() const { return m_feishuWebhookEdit->text().trimmed(); }
void SettingsDialog::setFeishuWebhook(const QString &url) { m_feishuWebhookEdit->setText(url); }

QColor SettingsDialog::shapeColor(const QString &shapeType) const {
    return m_colors.value(shapeType, Qt::white);
}
void SettingsDialog::setShapeColor(const QString &shapeType, const QColor &color) {
    m_colors[shapeType] = color;
    if (auto *btn = m_colorButtons.value(shapeType)) {
        btn->setStyleSheet(QString("background-color: %1; border: 1px solid #888;").arg(color.name()));
    }
}

QKeySequence SettingsDialog::shortcut(const QString &actionName) const {
    if (auto *edit = m_shortcutEdits.value(actionName))
        return edit->keySequence();
    return QKeySequence();
}
void SettingsDialog::setShortcut(const QString &actionName, const QKeySequence &key) {
    if (auto *edit = m_shortcutEdits.value(actionName))
        edit->setKeySequence(key);
}

QString SettingsDialog::serverHost() const { return m_hostEdit->text().trimmed(); }
int SettingsDialog::serverPort() const { return m_portSpin->value(); }
int SettingsDialog::reconnectInterval() const { return m_reconnectSpin->value(); }
int SettingsDialog::requestTimeout() const { return m_timeoutSpin->value(); }
bool SettingsDialog::autoReconnect() const {
    return m_autoReconnectCombo->currentData().toBool();
}

void SettingsDialog::setServerHost(const QString &host) { m_hostEdit->setText(host); }
void SettingsDialog::setServerPort(int port) { m_portSpin->setValue(port); }
void SettingsDialog::setReconnectInterval(int ms) { m_reconnectSpin->setValue(ms); }
void SettingsDialog::setRequestTimeout(int ms) { m_timeoutSpin->setValue(ms); }
void SettingsDialog::setAutoReconnect(bool on) {
    m_autoReconnectCombo->setCurrentIndex(on ? 0 : 1);
}

// ========== 配置文件读写 ==========

QString SettingsDialog::configFilePath() const
{
    QString p = AppPaths::resolveDataDir("config");
    QString f = QDir(p).filePath("server.json");
    if (!QFile::exists(f)) {
        f = QDir(QCoreApplication::applicationDirPath()).filePath("config/server.json");
    }
    return f;
}

void SettingsDialog::loadFromFile()
{
    QString path = configFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();

    // 连接
    setServerHost(obj.value("host").toString("127.0.0.1"));
    setServerPort(obj.value("port").toInt(9888));
    setReconnectInterval(obj.value("reconnect_interval_ms").toInt(3000));
    setRequestTimeout(obj.value("request_timeout_ms").toInt(10000));
    setAutoReconnect(obj.value("auto_reconnect").toBool(true));

    // 飞书
    setFeishuWebhook(obj.value("feishu_webhook").toString());

    // 图形颜色
    QJsonObject colors = obj.value("shape_colors").toObject();
    QStringList shapeTypes = {"line","trend","gesture_up","gesture_down",
                              "text","hline","vline",
                              "trade_buy","trade_sell","trade_short","trade_cover"};
    for (const auto &st : shapeTypes) {
        QString hex = colors.value(st).toString("#FF0000");
        setShapeColor(st, QColor(hex));
    }

    // 快捷键
    QJsonObject shortcuts = obj.value("shortcuts").toObject();
    QStringList actionNames = {
        "tool_line","tool_trend","tool_gesture_up","tool_gesture_down",
        "tool_text","tool_hline","tool_vline",
        "tool_trade_buy","tool_trade_sell","tool_trade_short","tool_trade_cover",
        "delete_shape"
    };
    for (const auto &an : actionNames) {
        QString keyStr = shortcuts.value(an).toString();
        if (!keyStr.isEmpty()) {
            setShortcut(an, QKeySequence(keyStr));
        }
    }
}

void SettingsDialog::saveToFile()
{
    QString path = configFilePath();

    // 先读取现有配置，保留未知字段
    QJsonObject obj;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isObject()) obj = doc.object();
    }

    // 连接
    obj["host"] = serverHost();
    obj["port"] = serverPort();
    obj["reconnect_interval_ms"] = reconnectInterval();
    obj["request_timeout_ms"] = requestTimeout();
    obj["auto_reconnect"] = autoReconnect();

    // 飞书
    obj["feishu_webhook"] = feishuWebhook();

    // 图形颜色
    QJsonObject colors;
    QStringList shapeTypes = {"line","trend","gesture_up","gesture_down",
                              "text","hline","vline",
                              "trade_buy","trade_sell","trade_short","trade_cover"};
    for (const auto &st : shapeTypes) {
        colors[st] = m_colors.value(st, Qt::white).name();
    }
    obj["shape_colors"] = colors;

    // 快捷键
    QJsonObject shortcuts;
    QStringList actionNames = {
        "tool_line","tool_trend","tool_gesture_up","tool_gesture_down",
        "tool_text","tool_hline","tool_vline",
        "tool_trade_buy","tool_trade_sell","tool_trade_short","tool_trade_cover",
        "delete_shape"
    };
    for (const auto &an : actionNames) {
        QKeySequence ks = shortcut(an);
        if (!ks.isEmpty()) {
            shortcuts[an] = ks.toString();
        }
    }
    obj["shortcuts"] = shortcuts;

    // 写入
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
    }
}
