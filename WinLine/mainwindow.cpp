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

    // create and set KLineWidget as central widget
    KLineWidget *kw = new KLineWidget(this);
    setCentralWidget(kw);

    // create drawing tools toolbar
    QToolBar *drawToolBar = addToolBar("Drawing Tools");
    
    if (kw) {
        // normal mode button (mouse cursor) - disable all drawing tools
        QAction *normalAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_ArrowUp), "", this);
        normalAct->setToolTip(tr("Switch to normal mode - pan and view (no drawing)"));
        normalAct->setCheckable(true);
        normalAct->setChecked(true);  // start in normal mode
        drawToolBar->addAction(normalAct);
        
        drawToolBar->addSeparator();
        
        // line tool
        QAction *lineAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_FileDialogDetailedView), "", this);
        lineAct->setToolTip(tr("Draw a line"));
        lineAct->setCheckable(true);
        drawToolBar->addAction(lineAct);
        
        // trend line tool
        QAction *trendAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_ArrowForward), "", this);
        trendAct->setToolTip(tr("Draw a trend line (ray)"));
        trendAct->setCheckable(true);
        drawToolBar->addAction(trendAct);
        
        // gesture up tool
        QAction *gestureUpAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_ArrowUp), "", this);
        gestureUpAct->setToolTip(tr("Draw gesture up"));
        gestureUpAct->setCheckable(true);
        drawToolBar->addAction(gestureUpAct);
        
        // gesture down tool
        QAction *gestureDownAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_ArrowDown), "", this);
        gestureDownAct->setToolTip(tr("Draw gesture down"));
        gestureDownAct->setCheckable(true);
        drawToolBar->addAction(gestureDownAct);
        
        // text tool
        QAction *textAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_FileIcon), "", this);
        textAct->setToolTip(tr("Add text annotation"));
        textAct->setCheckable(true);
        drawToolBar->addAction(textAct);
        
        // collect all drawing tool actions
        QList<QAction*> drawingTools = {lineAct, trendAct, gestureUpAct, gestureDownAct, textAct};
        
        // Normal Mode: disable drawing when clicked
        connect(normalAct, &QAction::triggered, this, [kw, normalAct, drawingTools]() {
            kw->setToolMode(KLineWidget::Tool_None);
            normalAct->setChecked(true);
            for (QAction *act : drawingTools) act->setChecked(false);
        });
        
        // Line tool
        connect(lineAct, &QAction::triggered, this, [kw, normalAct, drawingTools, lineAct]() {
            kw->setToolMode(KLineWidget::Tool_Line);
            normalAct->setChecked(false);
            lineAct->setChecked(true);
            for (QAction *act : drawingTools) {
                if (act != lineAct) act->setChecked(false);
            }
        });
        
        // Trend tool
        connect(trendAct, &QAction::triggered, this, [kw, normalAct, drawingTools, trendAct]() {
            kw->setToolMode(KLineWidget::Tool_Trend);
            normalAct->setChecked(false);
            trendAct->setChecked(true);
            for (QAction *act : drawingTools) {
                if (act != trendAct) act->setChecked(false);
            }
        });
        
        // Gesture Up tool
        connect(gestureUpAct, &QAction::triggered, this, [kw, normalAct, drawingTools, gestureUpAct]() {
            kw->setToolMode(KLineWidget::Tool_GestureUp);
            normalAct->setChecked(false);
            gestureUpAct->setChecked(true);
            for (QAction *act : drawingTools) {
                if (act != gestureUpAct) act->setChecked(false);
            }
        });
        
        // Gesture Down tool
        connect(gestureDownAct, &QAction::triggered, this, [kw, normalAct, drawingTools, gestureDownAct]() {
            kw->setToolMode(KLineWidget::Tool_GestureDown);
            normalAct->setChecked(false);
            gestureDownAct->setChecked(true);
            for (QAction *act : drawingTools) {
                if (act != gestureDownAct) act->setChecked(false);
            }
        });
        
        // Text tool
        connect(textAct, &QAction::triggered, this, [kw, normalAct, drawingTools, textAct]() {
            kw->setToolMode(KLineWidget::Tool_Text);
            normalAct->setChecked(false);
            textAct->setChecked(true);
            for (QAction *act : drawingTools) {
                if (act != textAct) act->setChecked(false);
            }
        });
        
        drawToolBar->addSeparator();
        
        // clear shapes
        QAction *clearAct = new QAction(tr("Clear All"), this);
        clearAct->setToolTip(tr("Clear all drawing objects"));
        connect(clearAct, &QAction::triggered, this, [kw]() { kw->clearShapes(); });
        drawToolBar->addAction(clearAct);
        
        // delete selected shape
        QAction *deleteAct = new QAction(drawToolBar->style()->standardIcon(QStyle::SP_TrashIcon), tr(""), this);
        deleteAct->setToolTip(tr("Delete selected drawing object"));
        connect(deleteAct, &QAction::triggered, this, [kw]() { kw->deleteSelectedShape(); });
        drawToolBar->addAction(deleteAct);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
