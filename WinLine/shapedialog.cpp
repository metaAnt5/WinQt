#include "shapedialog.h"
#include "ui_shapedialog.h"
#include <QColorDialog>

ShapeDialog::ShapeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShapeDialog)
    , m_currentColor(Qt::white)
{
    ui->setupUi(this);
    
    connect(ui->colorButton, &QPushButton::clicked, this, &ShapeDialog::onColorButtonClicked);
    
    updateColorPreview();
}

ShapeDialog::~ShapeDialog()
{
    delete ui;
}

void ShapeDialog::setShapeName(const QString &name)
{
    ui->nameEdit->setText(name);
}

void ShapeDialog::setShapeColor(const QColor &color)
{
    m_currentColor = color;
    updateColorPreview();
}

QString ShapeDialog::getShapeName() const
{
    return ui->nameEdit->text();
}

QColor ShapeDialog::getShapeColor() const
{
    return m_currentColor;
}

void ShapeDialog::onColorButtonClicked()
{
    QColor c = QColorDialog::getColor(m_currentColor, this, tr("Choose Shape Color"));
    if (c.isValid()) {
        m_currentColor = c;
        updateColorPreview();
    }
}

void ShapeDialog::updateColorPreview()
{
    QPalette pal = ui->colorPreview->palette();
    pal.setColor(QPalette::Window, m_currentColor);
    ui->colorPreview->setAutoFillBackground(true);
    ui->colorPreview->setPalette(pal);
}
