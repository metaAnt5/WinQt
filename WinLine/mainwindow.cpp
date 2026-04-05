#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "klinewidget.h"
#include <QToolBar>
#include <QAction>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Note: central widget is managed by main.cpp, not here
}

MainWindow::~MainWindow()
{
    delete ui;
}
