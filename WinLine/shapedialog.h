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

    void setShapeName(const QString &name);
    void setShapeColor(const QColor &color);
    
    QString getShapeName() const;
    QColor getShapeColor() const;

private slots:
    void onColorButtonClicked();
    void updateColorPreview();

private:
    Ui::ShapeDialog *ui;
    QColor m_currentColor;
};

#endif // SHAPEDIALOG_H
