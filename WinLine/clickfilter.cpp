#include "clickfilter.h"
#include <QStackedWidget>
#include <QMouseEvent>

ClickFilter::ClickFilter(QStackedWidget *stack, QObject *parent)
    : QObject(parent), m_stack(stack) {}

bool ClickFilter::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    if (!m_stack) return false;
    if (event->type() == QEvent::MouseButtonDblClick) {
        // cycle to next page on double-click
        int idx = (m_stack->currentIndex() + 1) % m_stack->count();
        m_stack->setCurrentIndex(idx);
        return true;
    }
    return false;
}
