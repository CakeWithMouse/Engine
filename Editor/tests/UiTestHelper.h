#pragma once

#include <QObject>
#include <QWidget>
#include <QAction>
#include <QAbstractButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QString>
#include <QVariant>
#include <QCoreApplication>

#include <chrono>
#include <functional>
#include <string>
#include <stdexcept>
#include <thread>

namespace UiTestHelper
{

template <typename T>
T* Find(QObject* parent, const QString& objectName)
{
    if (!parent || objectName.isEmpty())
        return nullptr;

    for (auto* child : parent->findChildren<T*>())
    {
        if (child && child->objectName() == objectName)
            return child;
    }
    return nullptr;
}

template <typename T>
T* Require(QObject* parent, const QString& objectName, const char* context = nullptr)
{
    T* obj = Find<T>(parent, objectName);
    if (!obj)
    {
        std::string msg = "Required object with objectName '";
        msg += objectName.toStdString();
        msg += "' was not found";
        if (context)
        {
            msg += " (context: ";
            msg += context;
            msg += ")";
        }
        throw std::runtime_error(msg);
    }
    return obj;
}

inline void ClickButton(QObject* parent, const QString& objectName)
{
    auto* button = Require<QAbstractButton>(parent, objectName, "ClickButton");
    if (!button->isEnabled())
    {
        throw std::runtime_error("Button '" + objectName.toStdString() + "' is disabled and cannot be clicked");
    }
    button->click();
}

inline void TriggerAction(QObject* parent, const QString& objectName)
{
    auto* action = Require<QAction>(parent, objectName, "TriggerAction");
    if (!action->isEnabled())
    {
        throw std::runtime_error("Action '" + objectName.toStdString() + "' is disabled and cannot be triggered");
    }
    action->trigger();
}

inline void SetLineEdit(QObject* parent, const QString& objectName, const QString& text)
{
    auto* edit = Require<QLineEdit>(parent, objectName, "SetLineEdit");
    edit->setText(text);
}

inline void SetSpinBox(QObject* parent, const QString& objectName, double value)
{
    auto* spin = Require<QDoubleSpinBox>(parent, objectName, "SetSpinBox");
    spin->setValue(value);
}

inline void SelectComboBox(QObject* parent, const QString& objectName, int index)
{
    auto* combo = Require<QComboBox>(parent, objectName, "SelectComboBox");
    if (index < 0 || index >= combo->count())
    {
        throw std::runtime_error("SelectComboBox: index " + std::to_string(index) +
                                 " out of range [0, " + std::to_string(combo->count()) +
                                 ") for '" + objectName.toStdString() + "'");
    }
    combo->setCurrentIndex(index);
}

inline void SelectComboBoxData(QObject* parent, const QString& objectName, const QVariant& data)
{
    auto* combo = Require<QComboBox>(parent, objectName, "SelectComboBoxData");
    int index = combo->findData(data);
    if (index < 0)
    {
        throw std::runtime_error("SelectComboBoxData: value not found in combo '" + objectName.toStdString() + "'");
    }
    combo->setCurrentIndex(index);
}

inline void WaitUntil(const std::function<bool()>& predicate,
                      int timeoutMs = 8000,
                      int intervalMs = 10,
                      const std::function<void()>& onPump = nullptr,
                      const char* timeoutMessage = "WaitUntil condition timed out")
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            throw std::runtime_error(timeoutMessage ? timeoutMessage : "WaitUntil timed out");
        }

        if (onPump)
        {
            onPump();
        }
        else
        {
            QCoreApplication::processEvents();
        }

        if (intervalMs > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }
}

} // namespace UiTestHelper

