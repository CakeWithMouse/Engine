#pragma once

#include "FileService.h"
#include <QByteArray>
#include <QString>

namespace EditorCore
{

struct ImportSettings
{
    float scale = 1.0f;
};

class ImportSettingsFile
{
public:
    enum class LoadResult
    {
        Success = 0,
        NotFound,
        Error
    };

    static constexpr float MinScale = 1e-4f;
    static constexpr float MaxScale = 1e4f;
    static constexpr float DefaultScale = 1.0f;

    static bool IsValidScale(float scale);
    static float ClampScale(float scale);

    static QByteArray Serialize(const ImportSettings& settings);
    static bool Parse(const QByteArray& data, ImportSettings& outSettings, QString* outError = nullptr);

    static LoadResult Load(FileService& fileService, const QString& relPath, ImportSettings& outSettings, QString* outError = nullptr);
    static bool Save(FileService& fileService, const QString& relPath, const ImportSettings& settings, QString* outError = nullptr);

    static QString SidecarPathForAsset(const QString& assetPath);
};

} // namespace EditorCore

