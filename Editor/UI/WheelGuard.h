#pragma once

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QObject>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>
#include <cstring>

class WheelGuard : public QObject
{
public:
    static WheelGuard& Instance()
    {
        static WheelGuard instance;
        return instance;
    }

    void Install(QObject* target = nullptr)
    {
        QObject* obj = target ? target : qApp;
        if (obj)
        {
            obj->removeEventFilter(this);
            obj->installEventFilter(this);
        }
    }

    void Uninstall(QObject* target = nullptr)
    {
        QObject* obj = target ? target : qApp;
        if (obj)
        {
            obj->removeEventFilter(this);
        }
    }

    static void SetExempt(QWidget* widget, bool exempt = true)
    {
        if (widget)
        {
            widget->setProperty("wheelGuardExempt", exempt);
        }
    }

    static QWidget* FindGuardedControl(QWidget* widget)
    {
        if (!widget) return nullptr;
        // Scrollbars are never guarded (they must scroll views)
        if (qobject_cast<QScrollBar*>(widget)) return nullptr;

        for (QWidget* w = widget; w; w = w->parentWidget())
        {
            if (qobject_cast<QScrollBar*>(w)) return nullptr;

            // Direct check for spin/combo/slider
            if (qobject_cast<QAbstractSpinBox*>(w) ||
                qobject_cast<QComboBox*>(w) ||
                qobject_cast<QAbstractSlider*>(w))
            {
                if (w->property("wheelGuardExempt").toBool())
                    return nullptr;
                return w;
            }

            // Exception: popup list view of an open QComboBox must be able to scroll its items
            if (qobject_cast<QAbstractItemView*>(w)) return nullptr;

            if (w->property("wheelGuardExempt").toBool())
                return nullptr;
        }
        return nullptr;
    }

    static bool IsGuarded(const QWidget* widget)
    {
        if (!widget) return false;
        return FindGuardedControl(const_cast<QWidget*>(widget)) != nullptr;
    }

    static bool IsExempt(const QWidget* widget)
    {
        if (!widget) return false;
        for (const QWidget* w = widget; w; w = w->parentWidget())
        {
            if (w->property("wheelGuardExempt").toBool())
                return true;

            const QString name = w->objectName();
            if (name == QStringLiteral("Viewport") ||
                name == QStringLiteral("ViewportWidget") ||
                name == QStringLiteral("MaterialGraphWidget") ||
                name == QStringLiteral("GraphCanvas") ||
                name == QStringLiteral("GraphEditorWidget") ||
                name == QStringLiteral("UIEditorCanvas"))
            {
                return true;
            }

            const char* className = w->metaObject() ? w->metaObject()->className() : "";
            if (std::strcmp(className, "ViewportWidget") == 0 ||
                std::strcmp(className, "MaterialGraphWidget") == 0 ||
                std::strcmp(className, "GraphEditorWidget") == 0)
            {
                return true;
            }
        }
        return false;
    }

    static bool HasFocus(const QWidget* widget)
    {
        if (!widget) return false;
        if (widget->hasFocus()) return true;

        if (auto* spin = qobject_cast<const QAbstractSpinBox*>(widget))
        {
            if (auto* lineEdit = spin->findChild<QLineEdit*>())
            {
                if (lineEdit->hasFocus()) return true;
            }
        }
        return false;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Wheel)
            return QObject::eventFilter(watched, event);

        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget)
            return QObject::eventFilter(watched, event);

        // Avoid recursion during event forwarding
        if (guarding_)
            return false;

        QWidget* guarded = FindGuardedControl(widget);
        if (guarded && !IsExempt(guarded))
        {
            // Focus policy: wheel should never grant focus (ClickFocus / TabFocus only)
            if (guarded->focusPolicy() == Qt::WheelFocus)
            {
                guarded->setFocusPolicy(Qt::StrongFocus);
            }

            event->ignore();

            // Forward event to parent of guarded control (e.g. scroll area)
            if (QWidget* parent = guarded->parentWidget())
            {
                guarding_ = true;
                QApplication::sendEvent(parent, event);
                guarding_ = false;
            }
            return true; // Suppress value mutation on guarded control
        }

        return QObject::eventFilter(watched, event);
    }

private:
    WheelGuard() = default;
    ~WheelGuard() override = default;

    bool guarding_{false};
};
