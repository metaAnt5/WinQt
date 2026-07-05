#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <QColor>
#include <QMap>
#include <QKeySequenceEdit>
#include <QSpinBox>
#include <QVBoxLayout>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 图形颜色
    QColor shapeColor(const QString &shapeType) const;
    void setShapeColor(const QString &shapeType, const QColor &color);

    // 快捷键
    QKeySequence shortcut(const QString &actionName) const;
    void setShortcut(const QString &actionName, const QKeySequence &key);

    // 连接
    QString serverHost() const;
    int serverPort() const;
    int reconnectInterval() const;
    int requestTimeout() const;
    bool autoReconnect() const;

    void setServerHost(const QString &host);
    void setServerPort(int port);
    void setReconnectInterval(int ms);
    void setRequestTimeout(int ms);
    void setAutoReconnect(bool on);

    // 加载/保存到 server.json
    void loadFromFile();
    void saveToFile();

private:
    void setupUI();

    QTabWidget *m_tabs;

    // 图形颜色页签
    QMap<QString, QPushButton*> m_colorButtons;
    QMap<QString, QColor> m_colors;

    // 快捷键页签
    QMap<QString, QKeySequenceEdit*> m_shortcutEdits;

    // 连接页签
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QSpinBox *m_reconnectSpin;
    QSpinBox *m_timeoutSpin;
    QComboBox *m_autoReconnectCombo;

    void addColorRow(QWidget *parent, QVBoxLayout *layout,
                     const QString &label, const QString &shapeType);
    void addShortcutRow(QWidget *parent, QVBoxLayout *layout,
                        const QString &label, const QString &actionName);

    QString configFilePath() const;
};
