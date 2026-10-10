#include "ViewportWidget.h"
#include "QtHost.h"

#include <QPainter>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
    if (host_->HasViewport())
    {
        // Qt не закрашивает и не буферизует: сюда Present'ит движок
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    setMinimumSize(320, 200);
}

QPaintEngine* ViewportWidget::paintEngine() const
{
    // nullptr только когда контент нативный; в режиме заглушки нужен обычный paintEngine
    return host_->HasViewport() ? nullptr : QWidget::paintEngine();
}

void ViewportWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (host_->HasViewport())
        SendAttach();
}

void ViewportWidget::hideEvent(QHideEvent* e)
{
    if (attached_)
    {
        EditorCommand c; c.type = EditorCommand::ViewportDetach;
        host_->PostCommand(c);
        attached_ = false;
    }
    QWidget::hideEvent(e);
}

void ViewportWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (attached_)
        SendResize();
}

void ViewportWidget::paintEvent(QPaintEvent*)
{
    if (host_->HasViewport())
        return;                                  // нативный контент, не трогаем
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setPen(QColor(200, 200, 200));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("Viewport unavailable:\nengine without ModuleApi v2 viewport API"));
}

void ViewportWidget::SendAttach()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportAttach;
    c.hwnd = (void*)winId();                     // создаёт нативное окно виджета
    c.w = PhysW(); c.h = PhysH();
    host_->PostCommand(c);
    attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportResize;
    c.w = PhysW(); c.h = PhysH();
    host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return (uint32_t)std::lround(width()  * devicePixelRatioF()); }
uint32_t ViewportWidget::PhysH() const { return (uint32_t)std::lround(height() * devicePixelRatioF()); }