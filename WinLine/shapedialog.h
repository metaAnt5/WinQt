#ifndef SHAPEDIALOG_H
#define SHAPEDIALOG_H

#include <QDialog>
#include <QColor>

namespace Ui {
class ShapeDialog;
}

class ShapeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShapeDialog(QWidget *parent = nullptr);
    ~ShapeDialog();

    // 基本信息
    void setShapeInfo(const QString &info);
    // 名称
    void setShapeName(const QString &name);
    QString getShapeName() const;
    // 颜色
    void setShapeColor(const QColor &color);
    QColor getShapeColor() const;
    // 文本
    void setShapeText(const QString &text);
    QString getShapeText() const;
    // 脚本
    void setScriptList(const QStringList &scripts);
    void setScriptName(const QString &name);
    QString getScriptName() const;
    void setScriptDescription(const QString &desc);
    // 参数
    void setParam(int index, const QString &name, const QString &value);
    QString getParamName(int index) const;
    QString getParamValue(int index) const;

private slots:
    void onColorButtonClicked();
    void updateColorPreview();
    void onScriptChanged(const QString &text);

private:
    Ui::ShapeDialog *ui;
    QColor m_currentColor;
};

#endif // SHAPEDIALOG_H
