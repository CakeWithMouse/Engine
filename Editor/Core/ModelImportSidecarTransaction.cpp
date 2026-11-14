#include "ModelImportSidecarTransaction.h"
#include "ModelImportSidecar.h"
#include "FileService.h"

namespace EditorCore
{

void SidecarTransaction::Begin(FileService& fileService, const QString& assetPath, const QString& contentRoot)
{
    Commit();
    sidecarPath_ = ModelImportSidecar::ResolveSidecarPath(assetPath, fileService.GetProjectRoot(), contentRoot);
    if (sidecarPath_.isEmpty())
    {
        return;
    }
    hadSidecar_ = fileService.FileExists(sidecarPath_);
    if (hadSidecar_ && !fileService.ReadFile(sidecarPath_, backup_))
    {
        // Without a readable backup there is nothing to restore; stay inactive.
        sidecarPath_.clear();
        hadSidecar_ = false;
        return;
    }
    active_ = true;
}

SidecarUpdateResult SidecarTransaction::FinishUpdate(FileService& fileService, const QString& assetPath,
                                                     const QString& contentRoot, bool success, QString* outError)
{
    if (!active_)
    {
        return SidecarUpdateResult::NotMine;
    }
    const QString other = ModelImportSidecar::ResolveSidecarPath(assetPath, fileService.GetProjectRoot(), contentRoot);
    if (other.isEmpty() || other.compare(sidecarPath_, Qt::CaseInsensitive) != 0)
    {
        // A different asset was updated, so our reimport never ran; keep the saved sidecar.
        Commit();
        return SidecarUpdateResult::NotMine;
    }
    if (success)
    {
        Commit();
        return SidecarUpdateResult::Committed;
    }
    return Rollback(fileService, outError) ? SidecarUpdateResult::RolledBack : SidecarUpdateResult::RollbackFailed;
}

void SidecarTransaction::Commit()
{
    active_ = false;
    hadSidecar_ = false;
    backup_.clear();
    sidecarPath_.clear();
}

bool SidecarTransaction::Rollback(FileService& fileService, QString* outError)
{
    if (!active_)
    {
        return false;
    }
    const QString path = sidecarPath_;
    const QByteArray data = backup_;
    const bool existed = hadSidecar_;
    Commit();
    if (existed)
    {
        return fileService.WriteFileAtomic(path, data, outError);
    }
    return fileService.DeleteFile(path, outError);
}

} // namespace EditorCore