#include "ModelImportSidecar.h"
#include "Logger.h"
#include <QDir>
#include <QFileInfo>

namespace EditorCore
{

QString ModelImportSidecar::ResolveSidecarPath(const QString& assetPath,
                                              const QString& projectRoot,
                                              const QString& contentRoot)
{
    if (assetPath.isEmpty() || projectRoot.isEmpty() || contentRoot.isEmpty())
    {
        return {};
    }

    QString fullAssetPath;
    if (assetPath.startsWith(QStringLiteral("/Game/"), Qt::CaseInsensitive))
    {
        const QString subPath = assetPath.mid(6);
        if (subPath.contains(QStringLiteral("..")))
        {
            return {};
        }
        fullAssetPath = QDir(contentRoot).filePath(subPath);
    }
    else
    {
        QFileInfo fi(assetPath);
        if (fi.isAbsolute())
        {
            fullAssetPath = fi.absoluteFilePath();
        }
        else
        {
            if (assetPath.contains(QStringLiteral("..")))
            {
                return {};
            }
            fullAssetPath = QDir(contentRoot).filePath(assetPath);
        }
    }

    const QString cleanAssetPath = QDir::cleanPath(fullAssetPath);
    const QString cleanProjectRoot = QDir::cleanPath(projectRoot);

    if (cleanAssetPath != cleanProjectRoot &&
        !cleanAssetPath.startsWith(cleanProjectRoot + QStringLiteral("/"), Qt::CaseInsensitive))
    {
        return {};
    }

    return ImportSettingsFile::SidecarPathForAsset(cleanAssetPath);
}

bool ModelImportSidecar::WriteSidecar(FileService& fileService,
                                     const QString& assetPath,
                                     float scale,
                                     const QString& contentRoot,
                                     QString* outError)
{
    ImportSettings settings;
    settings.model.scale = scale;
    return WriteSidecar(fileService, assetPath, settings, contentRoot, outError);
}

bool ModelImportSidecar::WriteSidecar(FileService& fileService,
                                     const QString& assetPath,
                                     const ImportSettings& settings,
                                     const QString& contentRoot,
                                     QString* outError)
{
    const QString projectRoot = fileService.GetProjectRoot();
    const QString sidecarPath = ResolveSidecarPath(assetPath, projectRoot, contentRoot);
    if (sidecarPath.isEmpty())
    {
        const QString err = QStringLiteral("ModelImportSidecar: asset path '%1' is outside project sandbox '%2'")
                                .arg(assetPath, projectRoot);
        Logger::Instance().Error(err);
        if (outError)
            *outError = err;
        return false;
    }

    return ImportSettingsFile::Save(fileService, sidecarPath, settings, outError);
}

float ModelImportSidecar::ReadSpawnScale(FileService& fileService,
                                        const QString& assetPath,
                                        const QString& contentRoot,
                                        ImportSettingsFile::LoadResult* outResult,
                                        QString* outError)
{
    const ImportSettings settings = ReadSettings(fileService, assetPath, contentRoot, outResult, outError);
    return settings.model.scale;
}

ImportSettings ModelImportSidecar::ReadSettings(FileService& fileService,
                                               const QString& assetPath,
                                               const QString& contentRoot,
                                               ImportSettingsFile::LoadResult* outResult,
                                               QString* outError)
{
    const QString projectRoot = fileService.GetProjectRoot();
    const QString sidecarPath = ResolveSidecarPath(assetPath, projectRoot, contentRoot);
    if (sidecarPath.isEmpty())
    {
        const QString err = QStringLiteral("ModelImportSidecar: asset path '%1' is outside project sandbox '%2'")
                                .arg(assetPath, projectRoot);
        Logger::Instance().Error(err);
        if (outError)
            *outError = err;
        if (outResult)
            *outResult = ImportSettingsFile::LoadResult::Error;
        return ImportSettings{};
    }

    ImportSettings settings;
    const auto res = ImportSettingsFile::Load(fileService, sidecarPath, settings, outError);
    if (outResult)
        *outResult = res;

    if (res == ImportSettingsFile::LoadResult::Success)
    {
        return settings;
    }

    return ImportSettings{};
}

} // namespace EditorCore
