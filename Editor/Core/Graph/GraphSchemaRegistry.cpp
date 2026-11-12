#include "GraphSchemaRegistry.h"

#include "../CodeGraphSchema.h"
#include "../EditorLocale.h"
#include "../MaterialGraphSchema.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EditorCore
{

GraphSchemaRegistry& GraphSchemaRegistry::Instance()
{
    static GraphSchemaRegistry registry;
    return registry;
}

GraphSchemaRegistry::GraphSchemaRegistry()
{
    Register({QString(), QStringLiteral("Материал"), QStringLiteral("Material"),
              MaterialGraphSchema::Instance().ClipboardMarker(), QStringLiteral(".mat.graph.json"), {}});
    const CodeGraphSchema code;
    Register({code.Id(), QStringLiteral("Скрипт"), QStringLiteral("Script"), code.ClipboardMarker(), QStringLiteral(".vscript"),
              [] { return std::make_unique<CodeGraphSchema>(); }});
}

void GraphSchemaRegistry::Register(GraphSchemaEntry entry)
{
    for (auto& existing : entries_)
        if (existing.id == entry.id)
        {
            existing = std::move(entry);
            return;
        }
    entries_.push_back(std::move(entry));
}

const GraphSchemaEntry* GraphSchemaRegistry::Find(const QString& id) const
{
    for (const auto& entry : entries_)
        if (entry.id == id) return &entry;
    return nullptr;
}

QString GraphSchemaRegistry::DisplayName(const QString& id) const
{
    const GraphSchemaEntry* entry = Find(id);
    if (!entry) return id;
    return EditorLocale::Instance().Language() == EditorLanguage::Ru ? entry->nameRu : entry->nameEn;
}

const GraphSchemaEntry* GraphSchemaRegistry::ForClipboard(const QByteArray& text) const
{
    const QJsonDocument document = QJsonDocument::fromJson(text);
    if (!document.isObject()) return nullptr;
    const QJsonObject root = document.object();
    for (const auto& entry : entries_)
        if (!entry.clipboardMarker.isEmpty() && root.contains(entry.clipboardMarker)) return &entry;
    return nullptr;
}

const GraphSchemaEntry* GraphSchemaRegistry::ForFile(const QByteArray& text) const
{
    const QJsonDocument json = QJsonDocument::fromJson(text);
    if (!json.isObject()) return nullptr;
    const QJsonObject root = json.object();
    if (root.value(QStringLiteral("format")).toString() == QLatin1String(GraphDocument::FormatName))
        return Find(root.value(QStringLiteral("schema")).toString());
    // Без заголовка — граф материала, если в нём есть ноды; иначе это какой-то другой JSON.
    return root.value(QStringLiteral("nodes")).isArray() ? Find(QString()) : nullptr;
}

} // namespace EditorCore
