#pragma once

#include "FileService.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace EditorCore
{

enum class ModelNormalsMode
{
    File = 0,
    Flat,
    Smooth
};

struct ModelImportSettings
{
    float scale = 1.0f;
    bool useFileUnitScale = false;
    ModelNormalsMode normals = ModelNormalsMode::File;
    float smoothingAngle = 60.0f;             // [0.0, 175.0]
    bool importMaterials = true;
    bool importTextures = true;
    bool flipUv = false;
    bool mergeMeshes = true;
    QJsonObject extraFields;
};

struct ImportSettings
{
    int version = 1;
    QString kind = QStringLiteral("model");
    ModelImportSettings model;
    QJsonObject extraFields;

    ImportSettings() = default;
    explicit ImportSettings(float s) { model.scale = s; }
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

    static constexpr float MinAngle = 0.0f;
    static constexpr float MaxAngle = 175.0f;
    static constexpr float DefaultSmoothingAngle = 60.0f;

    static bool IsValidScale(float scale);
    static float ClampScale(float scale);

    static bool IsValidSmoothingAngle(float angle);
    static bool IsValidNormals(const QString& normals);
    static bool IsValidNormalsMode(ModelNormalsMode mode);
    static QString NormalsModeToString(ModelNormalsMode mode);
    static bool NormalsModeFromString(const QString& str, ModelNormalsMode& outMode);

    static QByteArray Serialize(const ImportSettings& settings);
    static bool Parse(const QByteArray& data, ImportSettings& outSettings, QString* outError = nullptr);

    static LoadResult Load(FileService& fileService, const QString& relPath, ImportSettings& outSettings, QString* outError = nullptr);
    static bool Save(FileService& fileService, const QString& relPath, const ImportSettings& settings, QString* outError = nullptr);

    static QString SidecarPathForAsset(const QString& assetPath);
};

} // namespace EditorCore
