#pragma once

#include "EditorLocale.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <vector>

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

struct ProjectAssetItem
{
    QString virtualPath;  // "/Game/Models/Hero.glb"
    QString relativePath; // "Models/Hero.glb"
    QString fileName;     // "Hero.glb"
};

inline std::vector<ProjectAssetItem> ListProjectAssets(const QString& contentRoot, AssetKind kind)
{
    if (contentRoot.isEmpty() || !QDir(contentRoot).exists())
        return {};

    const auto& extensions = (kind == AssetKind::Model) ? ModelExtensions()
        : (kind == AssetKind::Texture) ? TextureExtensions()
        : (kind == AssetKind::Audio) ? AudioExtensions()
        : (kind == AssetKind::Material) ? MaterialExtensions()
        : QStringList{};

    if (extensions.isEmpty())
        return {};

    QStringList nameFilters;
    for (const auto& ext : extensions)
        nameFilters << QStringLiteral("*.") + ext;

    std::vector<ProjectAssetItem> items;
    const QDir root(contentRoot);
    for (QDirIterator it(contentRoot, nameFilters, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
    {
        const QString fullPath = it.next();
        if (fullPath.endsWith(QStringLiteral(".importsettings.json"), Qt::CaseInsensitive) ||
            fullPath.endsWith(QStringLiteral(".source.json"), Qt::CaseInsensitive) ||
            fullPath.endsWith(QStringLiteral(".import.json"), Qt::CaseInsensitive) ||
            fullPath.contains(QStringLiteral("/.painengine/")) ||
            fullPath.contains(QStringLiteral("\\.painengine\\")))
        {
            continue;
        }

        const QString rel = root.relativeFilePath(fullPath);
        ProjectAssetItem item;
        item.relativePath = QDir::fromNativeSeparators(rel);
        item.virtualPath = QStringLiteral("/Game/") + item.relativePath;
        item.fileName = QFileInfo(fullPath).fileName();
        items.push_back(std::move(item));
    }

    std::sort(items.begin(), items.end(), [](const ProjectAssetItem& a, const ProjectAssetItem& b) {
        return a.relativePath.compare(b.relativePath, Qt::CaseInsensitive) < 0;
    });

    return items;
}

} // namespace EditorCore