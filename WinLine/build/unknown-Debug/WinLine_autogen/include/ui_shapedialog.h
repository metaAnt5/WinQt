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
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
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
    QSpacerItem *verticalSpacer;
    QHBoxLayout *buttonLayout;
    QSpacerItem *buttonSpacer;
    QPushButton *okBtn;
    QPushButton *cancelBtn;

    void setupUi(QDialog *ShapeDialog)
    {
        if (ShapeDialog->objectName().isEmpty())
            ShapeDialog->setObjectName("ShapeDialog");
        ShapeDialog->resize(500, 250);
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
        okBtn->setText(QCoreApplication::translate("ShapeDialog", "OK", nullptr));
        cancelBtn->setText(QCoreApplication::translate("ShapeDialog", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ShapeDialog: public Ui_ShapeDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHAPEDIALOG_H
