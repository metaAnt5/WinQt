#include "shapedialog.h"
#include "ui_shapedialog.h"
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>

ShapeDialog::ShapeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShapeDialog)
    , m_currentColor(Qt::white)
{
    ui->setupUi(this);

    // 颜色按钮
    connect(ui->colorButton, &QPushButton::clicked, this, &ShapeDialog::onColorButtonClicked);

    // 脚本下拉变化 → 更新说明
    connect(ui->scriptCombo, &QComboBox::currentTextChanged, this, &ShapeDialog::onScriptChanged);

    updateColorPreview();
}

ShapeDialog::~ShapeDialog()
{
    delete ui;
}

// ---- 基本信息 ----
void ShapeDialog::setShapeInfo(const QString &info)
{
    ui->infoLabel->setText(info);
}

// ---- 名称 ----
void ShapeDialog::setShapeName(const QString &name)
{
    ui->nameEdit->setText(name);
}

QString ShapeDialog::getShapeName() const
{
    return ui->nameEdit->text().trimmed();
}

// ---- 颜色 ----
void ShapeDialog::setShapeColor(const QColor &color)
{
    m_currentColor = color;
    updateColorPreview();
}

QColor ShapeDialog::getShapeColor() const
{
    return m_currentColor;
}

void ShapeDialog::onColorButtonClicked()
{
    QColor c = QColorDialog::getColor(m_currentColor, this, tr("选择颜色"));
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

// ---- 文本 ----
void ShapeDialog::setShapeText(const QString &text)
{
    ui->textEdit->setPlainText(text);
}

QString ShapeDialog::getShapeText() const
{
    return ui->textEdit->toPlainText().trimmed();
}

// ---- 脚本 ----
void ShapeDialog::setScriptList(const QStringList &scripts)
{
    ui->scriptCombo->clear();
    ui->scriptCombo->addItems(scripts);
}

void ShapeDialog::setScriptName(const QString &name)
{
    int ci = ui->scriptCombo->findText(name);
    if (ci >= 0)
        ui->scriptCombo->setCurrentIndex(ci);
    else
        ui->scriptCombo->setCurrentText(name);
}

QString ShapeDialog::getScriptName() const
{
    return ui->scriptCombo->currentText().trimmed();
}

void ShapeDialog::setScriptDescription(const QString &desc)
{
    ui->descView->setPlainText(desc.isEmpty() ? QStringLiteral("（无注释）") : desc);
}

void ShapeDialog::onScriptChanged(const QString &text)
{
    QString name = text.trimmed();
    if (name.endsWith(".lua", Qt::CaseInsensitive))
        name = name.left(name.length() - 4);

    // 尝试读脚本文件头部注释作为说明
    QString scriptsDir = QCoreApplication::applicationDirPath() + "/data/scripts";
    QDir dir(scriptsDir);
    QString filePath = dir.filePath(name + ".lua");
    QFile f(filePath);
    QString desc;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("--")) {
                if (!desc.isEmpty()) desc += "\n";
                desc += line.mid(2).trimmed();
            } else if (!line.isEmpty() && !line.startsWith("--")) {
                break;
            }
        }
        f.close();
    }
    setScriptDescription(desc);
}

// ---- 参数 ----
void ShapeDialog::setParam(int index, const QString &name, const QString &value)
{
    QLineEdit *nameEdits[] = {ui->p1Name, ui->p2Name, ui->p3Name};
    QLineEdit *valueEdits[] = {ui->p1Value, ui->p2Value, ui->p3Value};
    if (index >= 0 && index < 3) {
        nameEdits[index]->setText(name);
        valueEdits[index]->setText(value);
    }
}

QString ShapeDialog::getParamName(int index) const
{
    QLineEdit *edits[] = {ui->p1Name, ui->p2Name, ui->p3Name};
    return (index >= 0 && index < 3) ? edits[index]->text().trimmed() : QString();
}

QString ShapeDialog::getParamValue(int index) const
{
    QLineEdit *edits[] = {ui->p1Value, ui->p2Value, ui->p3Value};
    return (index >= 0 && index < 3) ? edits[index]->text().trimmed() : QString();
}
