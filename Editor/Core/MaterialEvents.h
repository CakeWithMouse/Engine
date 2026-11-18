#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <functional>

namespace EditorCore
{
// Уведомление «.mat изменился на диске» (публичные параметры, значения после сохранения):
// карточка материала в инспекторе объекта перестраивается без повторного выбора объекта.
// Только поток UI; подписчик обязан отписаться в деструкторе.
class MaterialEvents
{
public:
    static MaterialEvents& Instance()
    {
        static MaterialEvents events;
        return events;
    }

    int Subscribe(std::function<void(const QString& material)> callback)
    {
        callbacks_.insert(++nextId_, std::move(callback));
        return nextId_;
    }

    void Unsubscribe(int id) { callbacks_.remove(id); }

    void NotifyChanged(const QString& material)
    {
        // Копия: колбэк вправе отписаться прямо во время уведомления.
        const QList<std::function<void(const QString&)>> callbacks = callbacks_.values();
        for (const auto& callback : callbacks)
            callback(material);
    }

private:
    QHash<int, std::function<void(const QString&)>> callbacks_;
    int nextId_ = 0;
};
} // namespace EditorCore