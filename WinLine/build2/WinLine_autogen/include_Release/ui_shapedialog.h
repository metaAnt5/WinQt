/********************************************************************************
** Form generated from reading UI file 'shapedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.7.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SHAPEDIALOG_H
#define UI_SHAPEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ShapeDialog
{
public:
    QVBoxLayout *mainLayout;
    QLabel *titleLabel;
    QHBoxLayout *nameLayout;
    QLabel *nameLabel;
    QLineEdit *nameEdit;
    QHBoxLayout *colorLayout;
    QLabel *colorLabel;
    QLabel *colorPreview;
    QPushButton *colorButton;
    QSpacerItem *colorSpacer;
    QGroupBox *infoGroup;
    QVBoxLayout *infoLayout;
    QLabel *infoLabel;
    QVBoxLayout *textLayout;
    QLabel *textLabel;
    QTextEdit *textEdit;
    QGroupBox *scriptGroup;
    QVBoxLayout *scriptLayout;
    QHBoxLayout *scriptRow;
    QLabel *scriptLabel;
    QComboBox *scriptCombo;
    QLabel *descLabel;
    QTextEdit *descView;
    QHBoxLayout *p1Row;
    QLabel *p1Label;
    QLineEdit *p1Name;
    QLineEdit *p1Value;
    QHBoxLayout *p2Row;
    QLabel *p2Label;
    QLineEdit *p2Name;
    QLineEdit *p2Value;
    QHBoxLayout *p3Row;
    QLabel *p3Label;
    QLineEdit *p3Name;
    QLineEdit *p3Value;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *buttonLayout;
    QSpacerItem *buttonSpacer;
    QPushButton *okBtn;
    QPushButton *cancelBtn;

    void setupUi(QDialog *ShapeDialog)
    {
        if (ShapeDialog->objectName().isEmpty())
            ShapeDialog->setObjectName("ShapeDialog");
        ShapeDialog->resize(500, 580);
        mainLayout = new QVBoxLayout(ShapeDialog);
        mainLayout->setSpacing(15);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(20, 20, 20, 20);
        titleLabel = new QLabel(ShapeDialog);
        titleLabel->setObjectName("titleLabel");

        mainLayout->addWidget(titleLabel);

        nameLayout = new QHBoxLayout();
        nameLayout->setSpacing(10);
        nameLayout->setObjectName("nameLayout");
        nameLabel = new QLabel(ShapeDialog);
        nameLabel->setObjectName("nameLabel");
        nameLabel->setMinimumWidth(80);

        nameLayout->addWidget(nameLabel);

        nameEdit = new QLineEdit(ShapeDialog);
        nameEdit->setObjectName("nameEdit");
        nameEdit->setMinimumHeight(32);

        nameLayout->addWidget(nameEdit);


        mainLayout->addLayout(nameLayout);

        colorLayout = new QHBoxLayout();
        colorLayout->setSpacing(10);
        colorLayout->setObjectName("colorLayout");
        colorLabel = new QLabel(ShapeDialog);
        colorLabel->setObjectName("colorLabel");
        colorLabel->setMinimumWidth(80);

        colorLayout->addWidget(colorLabel);

        colorPreview = new QLabel(ShapeDialog);
        colorPreview->setObjectName("colorPreview");
        colorPreview->setMinimumSize(QSize(50, 32));
        colorPreview->setMaximumSize(QSize(50, 32));

        colorLayout->addWidget(colorPreview);

        colorButton = new QPushButton(ShapeDialog);
        colorButton->setObjectName("colorButton");
        colorButton->setMinimumHeight(32);

        colorLayout->addWidget(colorButton);

        colorSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        colorLayout->addItem(colorSpacer);


        mainLayout->addLayout(colorLayout);

        infoGroup = new QGroupBox(ShapeDialog);
        infoGroup->setObjectName("infoGroup");
        infoLayout = new QVBoxLayout(infoGroup);
        infoLayout->setObjectName("infoLayout");
        infoLabel = new QLabel(infoGroup);
        infoLabel->setObjectName("infoLabel");
        infoLabel->setWordWrap(true);

        infoLayout->addWidget(infoLabel);


        mainLayout->addWidget(infoGroup);

        textLayout = new QVBoxLayout();
        textLayout->setObjectName("textLayout");
        textLabel = new QLabel(ShapeDialog);
        textLabel->setObjectName("textLabel");

        textLayout->addWidget(textLabel);

        textEdit = new QTextEdit(ShapeDialog);
        textEdit->setObjectName("textEdit");
        textEdit->setMaximumHeight(60);

        textLayout->addWidget(textEdit);


        mainLayout->addLayout(textLayout);

        scriptGroup = new QGroupBox(ShapeDialog);
        scriptGroup->setObjectName("scriptGroup");
        scriptLayout = new QVBoxLayout(scriptGroup);
        scriptLayout->setObjectName("scriptLayout");
        scriptRow = new QHBoxLayout();
        scriptRow->setObjectName("scriptRow");
        scriptLabel = new QLabel(scriptGroup);
        scriptLabel->setObjectName("scriptLabel");

        scriptRow->addWidget(scriptLabel);

        scriptCombo = new QComboBox(scriptGroup);
        scriptCombo->setObjectName("scriptCombo");
        scriptCombo->setEditable(true);

        scriptRow->addWidget(scriptCombo);


        scriptLayout->addLayout(scriptRow);

        descLabel = new QLabel(scriptGroup);
        descLabel->setObjectName("descLabel");

        scriptLayout->addWidget(descLabel);

        descView = new QTextEdit(scriptGroup);
        descView->setObjectName("descView");
        descView->setReadOnly(true);
        descView->setMaximumHeight(80);

        scriptLayout->addWidget(descView);

        p1Row = new QHBoxLayout();
        p1Row->setObjectName("p1Row");
        p1Label = new QLabel(scriptGroup);
        p1Label->setObjectName("p1Label");
        p1Label->setFixedWidth(55);

        p1Row->addWidget(p1Label);

        p1Name = new QLineEdit(scriptGroup);
        p1Name->setObjectName("p1Name");

        p1Row->addWidget(p1Name);

        p1Value = new QLineEdit(scriptGroup);
        p1Value->setObjectName("p1Value");

        p1Row->addWidget(p1Value);


        scriptLayout->addLayout(p1Row);

        p2Row = new QHBoxLayout();
        p2Row->setObjectName("p2Row");
        p2Label = new QLabel(scriptGroup);
        p2Label->setObjectName("p2Label");
        p2Label->setFixedWidth(55);

        p2Row->addWidget(p2Label);

        p2Name = new QLineEdit(scriptGroup);
        p2Name->setObjectName("p2Name");

        p2Row->addWidget(p2Name);

        p2Value = new QLineEdit(scriptGroup);
        p2Value->setObjectName("p2Value");

        p2Row->addWidget(p2Value);


        scriptLayout->addLayout(p2Row);

        p3Row = new QHBoxLayout();
        p3Row->setObjectName("p3Row");
        p3Label = new QLabel(scriptGroup);
        p3Label->setObjectName("p3Label");
        p3Label->setFixedWidth(55);

        p3Row->addWidget(p3Label);

        p3Name = new QLineEdit(scriptGroup);
        p3Name->setObjectName("p3Name");

        p3Row->addWidget(p3Name);

        p3Value = new QLineEdit(scriptGroup);
        p3Value->setObjectName("p3Value");

        p3Row->addWidget(p3Value);


        scriptLayout->addLayout(p3Row);


        mainLayout->addWidget(scriptGroup);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        mainLayout->addItem(verticalSpacer);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setSpacing(10);
        buttonLayout->setObjectName("buttonLayout");
        buttonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(buttonSpacer);

        okBtn = new QPushButton(ShapeDialog);
        okBtn->setObjectName("okBtn");
        okBtn->setMinimumWidth(100);
        okBtn->setMinimumHeight(36);

        buttonLayout->addWidget(okBtn);

        cancelBtn = new QPushButton(ShapeDialog);
        cancelBtn->setObjectName("cancelBtn");
        cancelBtn->setMinimumWidth(100);
        cancelBtn->setMinimumHeight(36);

        buttonLayout->addWidget(cancelBtn);


        mainLayout->addLayout(buttonLayout);


        retranslateUi(ShapeDialog);
        QObject::connect(okBtn, &QPushButton::clicked, ShapeDialog, qOverload<>(&QDialog::accept));
        QObject::connect(cancelBtn, &QPushButton::clicked, ShapeDialog, qOverload<>(&QDialog::reject));

        okBtn->setDefault(true);


        QMetaObject::connectSlotsByName(ShapeDialog);
    } // setupUi

    void retranslateUi(QDialog *ShapeDialog)
    {
        ShapeDialog->setWindowTitle(QCoreApplication::translate("ShapeDialog", "Shape Properties", nullptr));
        ShapeDialog->setStyleSheet(QCoreApplication::translate("ShapeDialog", "QDialog {\n"
"    background-color: #f5f5f5;\n"
"}\n"
"QLabel {\n"
"    color: #333333;\n"
"    font-weight: bold;\n"
"}\n"
"QLineEdit {\n"
"    border: 1px solid #cccccc;\n"
"    border-radius: 4px;\n"
"    padding: 6px;\n"
"    background-color: white;\n"
"    selection-background-color: #0078d4;\n"
"}\n"
"QPushButton {\n"
"    background-color: #0078d4;\n"
"    color: white;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    padding: 6px 12px;\n"
"    font-weight: bold;\n"
"}\n"
"QPushButton:hover {\n"
"    background-color: #1084d7;\n"
"}\n"
"QPushButton:pressed {\n"
"    background-color: #005a9e;\n"
"}\n"
"QPushButton#cancelBtn {\n"
"    background-color: #cccccc;\n"
"    color: #333333;\n"
"}\n"
"QPushButton#cancelBtn:hover {\n"
"    background-color: #d9d9d9;\n"
"}", nullptr));
        titleLabel->setText(QCoreApplication::translate("ShapeDialog", "Edit Shape Properties", nullptr));
        titleLabel->setStyleSheet(QCoreApplication::translate("ShapeDialog", "font-size: 14px;\n"
"font-weight: bold;\n"
"color: #000000;", nullptr));
        nameLabel->setText(QCoreApplication::translate("ShapeDialog", "Name:", nullptr));
        colorLabel->setText(QCoreApplication::translate("ShapeDialog", "Color:", nullptr));
        colorPreview->setText(QString());
        colorPreview->setStyleSheet(QCoreApplication::translate("ShapeDialog", "border: 2px solid #999999;\n"
"border-radius: 4px;\n"
"background-color: white;", nullptr));
        colorButton->setText(QCoreApplication::translate("ShapeDialog", "Choose Color", nullptr));
        infoGroup->setTitle(QCoreApplication::translate("ShapeDialog", "\345\237\272\346\234\254\344\277\241\346\201\257", nullptr));
        infoLabel->setStyleSheet(QCoreApplication::translate("ShapeDialog", "color: #666; font-size: 11px; font-weight: normal;", nullptr));
        infoLabel->setText(QString());
        textLabel->setText(QCoreApplication::translate("ShapeDialog", "\346\226\207\346\234\254\345\206\205\345\256\271:", nullptr));
        textEdit->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\350\276\223\345\205\245\350\246\201\346\230\276\347\244\272\347\232\204\346\226\207\345\255\227\342\200\246", nullptr));
        scriptGroup->setTitle(QCoreApplication::translate("ShapeDialog", "\350\204\232\346\234\254\351\205\215\347\275\256", nullptr));
        scriptLabel->setText(QCoreApplication::translate("ShapeDialog", "\350\204\232\346\234\254\346\226\207\344\273\266:", nullptr));
        scriptCombo->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\351\200\211\346\213\251 .lua \350\204\232\346\234\254...", nullptr));
        descLabel->setText(QCoreApplication::translate("ShapeDialog", "\350\204\232\346\234\254\350\257\264\346\230\216:", nullptr));
        descView->setStyleSheet(QCoreApplication::translate("ShapeDialog", "background: #f0f0f0; color: #2d7d2d; font-size: 11px;", nullptr));
        p1Label->setText(QCoreApplication::translate("ShapeDialog", "\345\217\202\346\225\2601:", nullptr));
        p1Name->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\345\220\215\347\247\260", nullptr));
        p1Value->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\346\225\260\345\200\274", nullptr));
        p2Label->setText(QCoreApplication::translate("ShapeDialog", "\345\217\202\346\225\2602:", nullptr));
        p2Name->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\345\220\215\347\247\260", nullptr));
        p2Value->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\346\225\260\345\200\274", nullptr));
        p3Label->setText(QCoreApplication::translate("ShapeDialog", "\345\217\202\346\225\2603:", nullptr));
        p3Name->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\345\220\215\347\247\260", nullptr));
        p3Value->setPlaceholderText(QCoreApplication::translate("ShapeDialog", "\346\225\260\345\200\274", nullptr));
        okBtn->setText(QCoreApplication::translate("ShapeDialog", "OK", nullptr));
        cancelBtn->setText(QCoreApplication::translate("ShapeDialog", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ShapeDialog: public Ui_ShapeDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHAPEDIALOG_H
