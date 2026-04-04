#pragma once
#include <QObject>
#include <QEvent>

class QStackedWidget;

class ClickFilter : public QObject {
    Q_OBJECT
public:
    ClickFilter(QStackedWidget *stack, QObject *parent=nullptr);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QStackedWidget *m_stack;
};
