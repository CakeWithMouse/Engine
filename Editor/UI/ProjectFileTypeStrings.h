#pragma once

#include "AssetExtensions.h"

namespace ProjectFileTypeStrings
{
inline QString Header() { return Tr("Тип", "Type"); }

inline QString Type(const QString& path, bool directory)
{
    if (directory) return Tr("Папка", "Folder");
    switch (EditorCore::KindForFile(path)) {
    case EditorCore::AssetKind::Model: return Tr("Модель", "Model");
    case EditorCore::AssetKind::Texture: return Tr("Текстура", "Texture");
    case EditorCore::AssetKind::Audio: return Tr("Аудио", "Audio");
    case EditorCore::AssetKind::Material: return Tr("Материал", "Material");
    case EditorCore::AssetKind::Unknown: return Tr("Файл", "File");
    }
    return {};
}
}
