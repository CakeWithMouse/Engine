#include "ImportSettings.h"
#include "Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cmath>

namespace EditorCore
{

bool ImportSettingsFile::IsValidScale(float scale)
{
    return std::isfinite(scale) && scale >= MinScale && scale <= MaxScale;
}

float ImportSettingsFile::ClampScale(float scale)
{
    if (!std::isfinite(scale))
        return DefaultScale;
    if (scale < MinScale)
        return MinScale;
    if (scale > MaxScale)
        return MaxScale;
    return scale;
}

bool ImportSettingsFile::IsValidSmoothingAngle(float angle)
{
    return std::isfinite(angle) && angle >= MinAngle && angle <= MaxAngle;
}

bool ImportSettingsFile::IsValidNormals(const QString& normals)
{
    ModelNormalsMode mode{};
    return NormalsModeFromString(normals, mode);
}

bool ImportSettingsFile::IsValidNormalsMode(ModelNormalsMode mode)
{
    return mode == ModelNormalsMode::File ||
           mode == ModelNormalsMode::Flat ||
           mode == ModelNormalsMode::Smooth;
}

QString ImportSettingsFile::NormalsModeToString(ModelNormalsMode mode)
{
    switch (mode)
    {
    case ModelNormalsMode::Flat:
        return QStringLiteral("flat");
    case ModelNormalsMode::Smooth:
        return QStringLiteral("smooth");
    case ModelNormalsMode::File:
    default:
        return QStringLiteral("file");
    }
}

bool ImportSettingsFile::NormalsModeFromString(const QString& str, ModelNormalsMode& outMode)
{
    if (str == QStringLiteral("file"))
    {
        outMode = ModelNormalsMode::File;
        return true;
    }
    if (str == QStringLiteral("flat"))
    {
        outMode = ModelNormalsMode::Flat;
        return true;
    }
    if (str == QStringLiteral("smooth"))
    {
        outMode = ModelNormalsMode::Smooth;
        return true;
    }
    return false;
}

QByteArray ImportSettingsFile::Serialize(const ImportSettings& settings)
{
    QJsonObject root = settings.extraFields;
    root[QStringLiteral("version")] = settings.version > 0 ? settings.version : 1;
    root[QStringLiteral("kind")] = settings.kind.isEmpty() ? QStringLiteral("model") : settings.kind;

    QJsonObject modelObj = settings.model.extraFields;
    modelObj[QStringLiteral("scale")] = static_cast<double>(IsValidScale(settings.model.scale) ? settings.model.scale : DefaultScale);
    modelObj[QStringLiteral("useFileUnitScale")] = settings.model.useFileUnitScale;
    modelObj[QStringLiteral("normals")] = NormalsModeToString(IsValidNormalsMode(settings.model.normals) ? settings.model.normals : ModelNormalsMode::File);
    modelObj[QStringLiteral("smoothingAngle")] = static_cast<double>(IsValidSmoothingAngle(settings.model.smoothingAngle) ? settings.model.smoothingAngle : DefaultSmoothingAngle);
    modelObj[QStringLiteral("importMaterials")] = settings.model.importMaterials;
    modelObj[QStringLiteral("importTextures")] = settings.model.importTextures;
    modelObj[QStringLiteral("flipUv")] = settings.model.flipUv;
    modelObj[QStringLiteral("mergeMeshes")] = settings.model.mergeMeshes;

    root[QStringLiteral("model")] = modelObj;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ImportSettingsFile::Parse(const QByteArray& data, ImportSettings& outSettings, QString* outError)
{
    outSettings = ImportSettings{};
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject())
    {
        const QString errStr = QStringLiteral("ImportSettings: JSON parse error: %1").arg(err.errorString());
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    const QJsonObject root = doc.object();

    // Check for Legacy S16 format: {"scale": 1.0} or {"version": 1, "scale": 1.0} without "kind" or "model"
    if (!root.contains(QStringLiteral("kind")) && !root.contains(QStringLiteral("model")) && root.contains(QStringLiteral("scale")))
    {
        const QJsonValue scaleVal = root.value(QStringLiteral("scale"));
        if (!scaleVal.isDouble())
        {
            const QString errStr = QStringLiteral("ImportSettings: legacy 'scale' is not a number, using default 1.0");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }

        const double dScale = scaleVal.toDouble();
        const float fScale = static_cast<float>(dScale);
        if (!IsValidScale(fScale))
        {
            const QString errStr = QStringLiteral("ImportSettings: legacy scale %1 is out of allowed range [%2, %3], using default 1.0")
                .arg(dScale, 0, 'g', 6)
                .arg(static_cast<double>(MinScale), 0, 'g', 6)
                .arg(static_cast<double>(MaxScale), 0, 'g', 6);
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }

        Logger::Instance().Info(QStringLiteral("ImportSettings: migrating legacy sidecar with 'scale' to v1 model format"));
        outSettings.version = 1;
        outSettings.kind = QStringLiteral("model");
        outSettings.model.scale = fScale;

        // Preserve any unknown legacy fields
        for (auto it = root.begin(); it != root.end(); ++it)
        {
            if (it.key() != QStringLiteral("scale") && it.key() != QStringLiteral("version"))
            {
                outSettings.extraFields[it.key()] = it.value();
            }
        }
        return true;
    }

    // Modern v1 format
    // 1. version
    if (root.contains(QStringLiteral("version")))
    {
        if (!root.value(QStringLiteral("version")).isDouble())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'version' must be an integer");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.version = root.value(QStringLiteral("version")).toInt();
    }
    else
    {
        outSettings.version = 1;
    }

    // 2. kind
    if (!root.contains(QStringLiteral("kind")))
    {
        const QString errStr = QStringLiteral("ImportSettings: missing 'kind' field in JSON, using default settings");
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }
    const QJsonValue kindVal = root.value(QStringLiteral("kind"));
    if (!kindVal.isString())
    {
        const QString errStr = QStringLiteral("ImportSettings: 'kind' must be a string");
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }
    outSettings.kind = kindVal.toString();
    if (outSettings.kind != QStringLiteral("model"))
    {
        const QString errStr = QStringLiteral("ImportSettings: unsupported kind '%1'").arg(outSettings.kind);
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    // 3. model section
    if (!root.contains(QStringLiteral("model")) || !root.value(QStringLiteral("model")).isObject())
    {
        const QString errStr = QStringLiteral("ImportSettings: missing or invalid 'model' object");
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    const QJsonObject modelObj = root.value(QStringLiteral("model")).toObject();

    // scale (optional in model, default 1.0)
    if (modelObj.contains(QStringLiteral("scale")))
    {
        const QJsonValue mScaleVal = modelObj.value(QStringLiteral("scale"));
        if (!mScaleVal.isDouble())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'scale' must be a number");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        const double dScale = mScaleVal.toDouble();
        const float fScale = static_cast<float>(dScale);
        if (!IsValidScale(fScale))
        {
            const QString errStr = QStringLiteral("ImportSettings: scale %1 is out of allowed range [%2, %3]")
                .arg(dScale, 0, 'g', 6)
                .arg(static_cast<double>(MinScale), 0, 'g', 6)
                .arg(static_cast<double>(MaxScale), 0, 'g', 6);
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.scale = fScale;
    }
    else
    {
        outSettings.model.scale = 1.0f;
    }

    // useFileUnitScale (optional, bool, default false)
    if (modelObj.contains(QStringLiteral("useFileUnitScale")))
    {
        if (!modelObj.value(QStringLiteral("useFileUnitScale")).isBool())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'useFileUnitScale' must be a boolean");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.useFileUnitScale = modelObj.value(QStringLiteral("useFileUnitScale")).toBool();
    }

    // normals (optional, enum: "file", "flat", "smooth", default "file")
    if (modelObj.contains(QStringLiteral("normals")))
    {
        if (!modelObj.value(QStringLiteral("normals")).isString())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'normals' must be a string");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        const QString normalsStr = modelObj.value(QStringLiteral("normals")).toString();
        ModelNormalsMode mode{};
        if (!NormalsModeFromString(normalsStr, mode))
        {
            const QString errStr = QStringLiteral("ImportSettings: invalid normals mode '%1' (must be 'file', 'flat', or 'smooth')").arg(normalsStr);
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.normals = mode;
    }

    // smoothingAngle (optional, float [0, 175], default 60.0)
    if (modelObj.contains(QStringLiteral("smoothingAngle")))
    {
        if (!modelObj.value(QStringLiteral("smoothingAngle")).isDouble())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'smoothingAngle' must be a number");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        const double dAngle = modelObj.value(QStringLiteral("smoothingAngle")).toDouble();
        const float fAngle = static_cast<float>(dAngle);
        if (!IsValidSmoothingAngle(fAngle))
        {
            const QString errStr = QStringLiteral("ImportSettings: smoothingAngle %1 is out of allowed range [%2, %3]")
                .arg(dAngle, 0, 'g', 6)
                .arg(static_cast<double>(MinAngle), 0, 'g', 6)
                .arg(static_cast<double>(MaxAngle), 0, 'g', 6);
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.smoothingAngle = fAngle;
    }

    // importMaterials (optional, bool, default true)
    if (modelObj.contains(QStringLiteral("importMaterials")))
    {
        if (!modelObj.value(QStringLiteral("importMaterials")).isBool())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'importMaterials' must be a boolean");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.importMaterials = modelObj.value(QStringLiteral("importMaterials")).toBool();
    }

    // importTextures (optional, bool, default true)
    if (modelObj.contains(QStringLiteral("importTextures")))
    {
        if (!modelObj.value(QStringLiteral("importTextures")).isBool())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'importTextures' must be a boolean");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.importTextures = modelObj.value(QStringLiteral("importTextures")).toBool();
    }

    // flipUv (optional, bool, default false)
    if (modelObj.contains(QStringLiteral("flipUv")))
    {
        if (!modelObj.value(QStringLiteral("flipUv")).isBool())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'flipUv' must be a boolean");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.flipUv = modelObj.value(QStringLiteral("flipUv")).toBool();
    }

    // mergeMeshes (optional, bool, default true)
    if (modelObj.contains(QStringLiteral("mergeMeshes")))
    {
        if (!modelObj.value(QStringLiteral("mergeMeshes")).isBool())
        {
            const QString errStr = QStringLiteral("ImportSettings: 'mergeMeshes' must be a boolean");
            Logger::Instance().Error(errStr);
            if (outError)
                *outError = errStr;
            return false;
        }
        outSettings.model.mergeMeshes = modelObj.value(QStringLiteral("mergeMeshes")).toBool();
    }

    // Unknown fields preservation
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        if (it.key() != QStringLiteral("version") && it.key() != QStringLiteral("kind") && it.key() != QStringLiteral("model"))
        {
            outSettings.extraFields[it.key()] = it.value();
        }
    }
    for (auto it = modelObj.begin(); it != modelObj.end(); ++it)
    {
        const QString k = it.key();
        if (k != QStringLiteral("scale") &&
            k != QStringLiteral("useFileUnitScale") &&
            k != QStringLiteral("normals") &&
            k != QStringLiteral("smoothingAngle") &&
            k != QStringLiteral("importMaterials") &&
            k != QStringLiteral("importTextures") &&
            k != QStringLiteral("flipUv") &&
            k != QStringLiteral("mergeMeshes"))
        {
            outSettings.model.extraFields[k] = it.value();
        }
    }

    return true;
}

ImportSettingsFile::LoadResult ImportSettingsFile::Load(FileService& fileService, const QString& relPath, ImportSettings& outSettings, QString* outError)
{
    outSettings = ImportSettings{};

    if (!fileService.FileExists(relPath))
    {
        const QString infoStr = QStringLiteral("ImportSettings: sidecar not found for '%1', using default settings").arg(relPath);
        Logger::Instance().Info(infoStr);
        if (outError)
            *outError = infoStr;
        return LoadResult::NotFound;
    }

    QByteArray data;
    if (!fileService.ReadFile(relPath, data, outError))
        return LoadResult::Error;

    if (!Parse(data, outSettings, outError))
        return LoadResult::Error;

    return LoadResult::Success;
}

bool ImportSettingsFile::Save(FileService& fileService, const QString& relPath, const ImportSettings& settings, QString* outError)
{
    if (!IsValidScale(settings.model.scale))
    {
        const QString errStr = QStringLiteral("ImportSettings: cannot save invalid scale %1").arg(settings.model.scale);
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }
    if (!IsValidSmoothingAngle(settings.model.smoothingAngle))
    {
        const QString errStr = QStringLiteral("ImportSettings: cannot save invalid smoothingAngle %1").arg(settings.model.smoothingAngle);
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }
    if (!IsValidNormalsMode(settings.model.normals))
    {
        const QString errStr = QStringLiteral("ImportSettings: cannot save invalid normals '%1'").arg(NormalsModeToString(settings.model.normals));
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }
    const QByteArray data = Serialize(settings);
    return fileService.WriteFileAtomic(relPath, data, outError);
}

QString ImportSettingsFile::SidecarPathForAsset(const QString& assetPath)
{
    if (assetPath.isEmpty())
        return {};
    const QString suffix = QStringLiteral(".importsettings.json");
    if (assetPath.endsWith(suffix))
        return assetPath;
    return assetPath + suffix;
}

} // namespace EditorCore
