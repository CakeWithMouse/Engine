#pragma once

#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QObject>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>

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

    static bool IsGuarded(const QWidget* widget)
    {
        if (!widget) return false;
        // Explicitly exclude scrollbars: scrollbars must handle wheel events to scroll views
        if (qobject_cast<const QScrollBar*>(widget)) return false;

        if (qobject_cast<const QAbstractSpinBox*>(widget)) return true;
        if (qobject_cast<const QComboBox*>(widget)) return true;
        if (qobject_cast<const QAbstractSlider*>(widget)) return true;
        return false;
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
                name == QStringLiteral("ViewportContainer") ||
                name == QStringLiteral("MaterialGraphWidget") ||
                name == QStringLiteral("GraphCanvas") ||
                name == QStringLiteral("GraphEditorWidget") ||
                name == QStringLiteral("UIEditorCanvas") ||
                name == QStringLiteral("UIEditorWindow"))
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

        if (guarding_ && watched == guardedTarget_)
            return false;

        if (IsGuarded(widget) && !IsExempt(widget))
        {
            if (!HasFocus(widget))
            {
                event->ignore();
                if (QWidget* parent = widget->parentWidget())
                {
                    guarding_ = true;
                    guardedTarget_ = parent;
                    QApplication::sendEvent(parent, event);
                    guarding_ = false;
                    guardedTarget_ = nullptr;
                }
                return true; // Suppress value change on unfocused control
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    WheelGuard() = default;
    ~WheelGuard() override = default;

    bool guarding_{false};
    QWidget* guardedTarget_{nullptr};
};
