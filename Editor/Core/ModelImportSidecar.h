#pragma once

#include "FileService.h"
#include "ImportSettings.h"
#include <QString>

namespace EditorCore
{

class ModelImportSidecar
{
public:
    static QString ResolveSidecarPath(const QString& assetPath,
                                      const QString& projectRoot,
                                      const QString& contentRoot);

    static bool WriteSidecar(FileService& fileService,
                             const QString& assetPath,
                             float scale,
                             const QString& contentRoot,
                             QString* outError = nullptr);

    static bool WriteSidecar(FileService& fileService,
                             const QString& assetPath,
                             const ImportSettings& settings,
                             const QString& contentRoot,
                             QString* outError = nullptr);

    static float ReadSpawnScale(FileService& fileService,
                                const QString& assetPath,
                                const QString& contentRoot,
                                ImportSettingsFile::LoadResult* outResult = nullptr,
                                QString* outError = nullptr);

    static ImportSettings ReadSettings(FileService& fileService,
                                       const QString& assetPath,
                                       const QString& contentRoot,
                                       ImportSettingsFile::LoadResult* outResult = nullptr,
                                       QString* outError = nullptr);
};

} // namespace EditorCore
