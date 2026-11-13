#pragma once

#include "EditorLocale.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace EditorCore
{

enum class AssetKind
{
    Model,
    Texture,
    Audio,
    Material,
    Unknown
};

// Списки должны совпадать с Engine/Source/Scene/SceneAssets.cpp и AssetUpdate.cpp; расширения без точки.
inline const QStringList& ModelExtensions()
{
    static const QStringList value{
        QStringLiteral("glb"), QStringLiteral("gltf"), QStringLiteral("obj"), QStringLiteral("fbx"), QStringLiteral("blend")};
    return value;
}

inline const QStringList& TextureExtensions()
{
    static const QStringList value{
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("tga")};
    return value;
}

inline const QStringList& AudioExtensions()
{
    static const QStringList value{
        QStringLiteral("wav"), QStringLiteral("ogg"), QStringLiteral("mp3"), QStringLiteral("flac")};
    return value;
}

inline const QStringList& MaterialExtensions()
{
    static const QStringList value{QStringLiteral("mat")};
    return value;
}

inline AssetKind KindForFile(const QString& path)
{
    const QString extension = QFileInfo(path).suffix().toLower();
    if (ModelExtensions().contains(extension))
        return AssetKind::Model;
    if (TextureExtensions().contains(extension))
        return AssetKind::Texture;
    if (AudioExtensions().contains(extension))
        return AssetKind::Audio;
    if (MaterialExtensions().contains(extension))
        return AssetKind::Material;
    return AssetKind::Unknown;
}

inline QString DialogFilter(AssetKind kind)
{
    const auto patterns = [](const QStringList& extensions) {
        QStringList result;
        for (const QString& extension : extensions)
            result << QStringLiteral("*.") + extension;
        return result.join(QStringLiteral(" "));
    };

    switch (kind)
    {
    case AssetKind::Model:
        return Tr("Модели", "Models") + QStringLiteral(" (") + patterns(ModelExtensions()) + QStringLiteral(")");
    case AssetKind::Texture:
        return Tr("Текстуры", "Textures") + QStringLiteral(" (") + patterns(TextureExtensions()) + QStringLiteral(")");
    case AssetKind::Audio:
        return Tr("Аудио", "Audio") + QStringLiteral(" (") + patterns(AudioExtensions()) + QStringLiteral(")");
    case AssetKind::Material:
        return Tr("Материалы", "Materials") + QStringLiteral(" (") + patterns(MaterialExtensions()) + QStringLiteral(")");
    case AssetKind::Unknown:
        break;
    }

    return Tr("Ресурсы", "Assets") + QStringLiteral(" (") + patterns(ModelExtensions() + TextureExtensions()) + QStringLiteral(")");
}

inline bool ReimportSourceMatches(const QString& assetPath, const QString& sourcePath)
{
    const AssetKind kind = KindForFile(assetPath);
    return kind != AssetKind::Unknown && KindForFile(sourcePath) == kind;
}

} // namespace EditorCore