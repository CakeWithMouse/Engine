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

QByteArray ImportSettingsFile::Serialize(const ImportSettings& settings)
{
    const float validScale = IsValidScale(settings.scale) ? settings.scale : DefaultScale;
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("scale")] = static_cast<double>(validScale);
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
    if (!root.contains(QStringLiteral("scale")))
    {
        const QString errStr = QStringLiteral("ImportSettings: missing 'scale' field in JSON, using default 1.0");
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    const QJsonValue scaleVal = root.value(QStringLiteral("scale"));
    if (!scaleVal.isDouble())
    {
        const QString errStr = QStringLiteral("ImportSettings: 'scale' is not a number, using default 1.0");
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    const double dScale = scaleVal.toDouble();
    const float fScale = static_cast<float>(dScale);
    if (!IsValidScale(fScale))
    {
        const QString errStr = QStringLiteral("ImportSettings: scale %1 is out of allowed range [%2, %3], using default 1.0")
            .arg(dScale, 0, 'g', 6)
            .arg(static_cast<double>(MinScale), 0, 'g', 6)
            .arg(static_cast<double>(MaxScale), 0, 'g', 6);
        Logger::Instance().Error(errStr);
        if (outError)
            *outError = errStr;
        return false;
    }

    outSettings.scale = fScale;
    return true;
}

bool ImportSettingsFile::Load(FileService& fileService, const QString& relPath, ImportSettings& outSettings, QString* outError)
{
    outSettings = ImportSettings{};
    QByteArray data;
    if (!fileService.ReadFile(relPath, data, outError))
        return false;
    return Parse(data, outSettings, outError);
}

bool ImportSettingsFile::Save(FileService& fileService, const QString& relPath, const ImportSettings& settings, QString* outError)
{
    if (!IsValidScale(settings.scale))
    {
        const QString errStr = QStringLiteral("ImportSettings: cannot save invalid scale %1").arg(settings.scale);
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
    return assetPath + QStringLiteral(".importsettings.json");
}

} // namespace EditorCore

