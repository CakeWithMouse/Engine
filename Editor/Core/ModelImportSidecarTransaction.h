#pragma once

#include <QByteArray>
#include <QString>

class FileService;

namespace EditorCore
{

enum class SidecarUpdateResult
{
    NotMine,
    Committed,
    RolledBack,
    RollbackFailed
};

// R4: keeps the raw sidecar bytes captured before "Import Settings..." writes new ones.
// If the following UpdateAsset fails, the previous file is restored (or a newly created
// one is removed); Commit discards the backup.
class SidecarTransaction
{
public:
    void Begin(FileService& fileService, const QString& assetPath, const QString& contentRoot);
    bool active() const { return active_; }

    SidecarUpdateResult FinishUpdate(FileService& fileService, const QString& assetPath,
                                     const QString& contentRoot, bool success, QString* outError = nullptr);
    void Commit();
    bool Rollback(FileService& fileService, QString* outError = nullptr);

private:
    QString sidecarPath_;
    QByteArray backup_;
    bool hadSidecar_ = false;
    bool active_ = false;
};

} // namespace EditorCore