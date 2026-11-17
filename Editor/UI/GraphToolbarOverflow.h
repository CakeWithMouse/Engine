#pragma once

#include "EditorTheme.h"
#include "IconProvider.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QSize>
#include <QString>
#include <QToolButton>
#include <QWidget>

// Адаптивный тулбар графа: когда строка не помещается по ширине, наименее важные
// кнопки сворачиваются в меню «…», при расширении возвращаются. Порядок регистрации
// задаёт приоритет: первый сворачивается первым. Владелец — тулбар (QObject-родитель).
class GraphToolbarOverflow : public QObject
{
public:
    GraphToolbarOverflow(QHBoxLayout* toolbar, QWidget* host)
        : QObject(host), toolbar_(toolbar), host_(host)
    {
        more_ = new QToolButton(host);
        more_->setObjectName(QStringLiteral("GraphToolbarMore"));
        more_->setIconSize(QSize(16, 16));
        more_->setAutoRaise(true);
        more_->setPopupMode(QToolButton::InstantPopup);
        more_->setToolTip(QStringLiteral("…"));
        menu_ = new QMenu(more_);
        more_->setMenu(menu_);
        more_->hide();
        toolbar_->addWidget(more_);
        RefreshMoreIcon();
        QObject::connect(menu_, &QMenu::aboutToShow, this, [this] { SyncActions(); });
        themeSub_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
            RefreshMoreIcon();
            SyncActions();
        });
        host_->installEventFilter(this);
    }

    ~GraphToolbarOverflow() override
    {
        if (themeSub_) EditorTheme::Instance().UnsubscribeThemeChanged(themeSub_);
    }

    // Сворачиваемый виджет без пункта меню (например, строка-подсказка).
    void AddWidget(QWidget* widget)
    {
        if (widget) items_.append({widget, nullptr});
    }

    // Кнопка с пунктом меню; menuText — короткая подпись (кнопки icon-only).
    void AddButton(QToolButton* button, const QString& menuText)
    {
        if (!button) return;
        auto* action = new QAction(menuText, menu_);
        action->setIcon(button->icon());
        action->setEnabled(button->isEnabled());
        QObject::connect(action, &QAction::triggered, this, [button] { button->click(); });
        menu_->addAction(action);
        action->setVisible(false);
        items_.append({button, action});
    }

    void Relayout()
    {
        if (!host_ || host_->width() <= 0) return;
        for (const auto& item : items_)
        {
            item.widget->show();
            if (item.action) item.action->setVisible(false);
        }
        more_->hide();
        const int available = host_->width();
        for (const auto& item : items_)
        {
            if (toolbar_->sizeHint().width() <= available) break;
            item.widget->hide();
            if (item.action)
            {
                item.action->setVisible(true);
                more_->show();
            }
        }
        SyncActions();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == host_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
            Relayout();
        return QObject::eventFilter(watched, event);
    }

private:
    void RefreshMoreIcon()
    {
        more_->setIcon(IconProvider::MoreIcon(EditorTheme::Instance().Palette().textSecondary));
    }

    void SyncActions()
    {
        for (const auto& item : items_)
            if (item.action)
            {
                auto* button = static_cast<QToolButton*>(item.widget);
                item.action->setIcon(button->icon());
                item.action->setEnabled(button->isEnabled());
            }
    }

    struct Item { QWidget* widget; QAction* action; };

    QHBoxLayout* toolbar_;
    QWidget* host_;
    QToolButton* more_;
    QMenu* menu_;
    QList<Item> items_;
    int themeSub_ = 0;
};