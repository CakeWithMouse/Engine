#pragma once

#include "GraphSchema.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

namespace EditorCore
{

// Что редактор знает о графе одной схемы, не открывая его окна: имя для человека, метка буфера обмена,
// расширение файла. По реестру ядро узнаёт чужие ноды в буфере обмена и чужой граф в файле.
struct GraphSchemaEntry
{
    QString id;              // как в заголовке V1; у графа материала пустой (формат Legacy)
    QString nameRu, nameEn;  // «Материал», «Скрипт», «Дерево поведения»
    QString clipboardMarker; // GraphSchema::ClipboardMarker()
    QString fileSuffix;      // ".vscript"; пустой — у графа нет своего файла
    // Новая схема (каталог схемы кода загружается отдельно). Пустая: схема-одиночка, окно создаёт её само.
    std::function<std::unique_ptr<GraphSchema>()> create;
};

// Реестр схем графов. Материал и код регистрируются сразу; новые схемы (ИИ) — вызовом Register.
class GraphSchemaRegistry
{
public:
    static GraphSchemaRegistry& Instance();

    // Схема с тем же id заменяется.
    void Register(GraphSchemaEntry entry);
    const QVector<GraphSchemaEntry>& Entries() const { return entries_; }

    const GraphSchemaEntry* Find(const QString& id) const;
    // Имя схемы на языке редактора; для незнакомого id — сам id.
    QString DisplayName(const QString& id) const;
    // Схема нод в тексте буфера обмена (по метке); nullptr — это не ноды графа.
    const GraphSchemaEntry* ForClipboard(const QByteArray& text) const;
    // Схема графа в тексте файла (по заголовку; без заголовка — материал); nullptr — не граф или схема незнакома.
    const GraphSchemaEntry* ForFile(const QByteArray& text) const;

private:
    GraphSchemaRegistry();
    QVector<GraphSchemaEntry> entries_;
};

} // namespace EditorCore
