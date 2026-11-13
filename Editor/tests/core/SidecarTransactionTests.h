#pragma once

#include "../../Core/ModelImportSidecarTransaction.h"
#include "../../Core/ModelImportSidecar.h"
#include "../../Core/FileService.h"
#include "../../Core/Logger.h"

#include <QDir>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

namespace SidecarTransactionTests
{

inline void Check(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (SidecarTransaction): %s\n", msg);
        std::exit(1);
    }
}

// R4/T4: a failed UpdateAsset after "Import Settings..." must restore the previous
// sidecar bytes. FinishUpdate(success=false) is the mock of the UpdateAsset error.
inline void Run(int& outChecks)
{
    using namespace EditorCore;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        Check(cond, msg);
        ++checks;
    };

    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory created for sidecar transaction tests");
    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());
    const QString projectRoot = tempDir.path();
    const QString contentRoot = QDir(projectRoot).filePath(QStringLiteral("Content"));
    fs.CreateDirectoryPath(QStringLiteral("Content/Models"));

    const QString assetPath = QDir(contentRoot).filePath(QStringLiteral("Models/Box.obj"));
    const QString sidecarPath = ModelImportSidecar::ResolveSidecarPath(assetPath, projectRoot, contentRoot);
    check(!sidecarPath.isEmpty(), "Sidecar path resolves inside sandbox");
    const QByteArray original("{\"version\":1,\"model\":{\"scale\":0.01}}");
    const QByteArray updated("{\"version\":2,\"model\":{\"scale\":2.0}}");

    auto readSidecar = [&](QByteArray& out) {
        QString err;
        check(fs.ReadFile(sidecarPath, out, &err), "Sidecar readable");
    };

    // Failed UpdateAsset restores the previous content byte for byte.
    {
        check(fs.WriteFileAtomic(sidecarPath, original), "Write original sidecar");
        SidecarTransaction tx;
        tx.Begin(fs, assetPath, contentRoot);
        check(tx.active(), "Transaction active after Begin");
        check(fs.WriteFileAtomic(sidecarPath, updated), "Overwrite sidecar with new settings");

        QString err;
        const auto result = tx.FinishUpdate(fs, assetPath, contentRoot, false, &err);
        check(result == SidecarUpdateResult::RolledBack, "Failed UpdateAsset rolls back");
        QByteArray content;
        readSidecar(content);
        check(content == original, "Sidecar restored to previous content");
        check(!tx.active(), "Transaction finished after rollback");
    }

    // Successful UpdateAsset keeps the new content.
    {
        check(fs.WriteFileAtomic(sidecarPath, original), "Write original sidecar");
        SidecarTransaction tx;
        tx.Begin(fs, assetPath, contentRoot);
        check(fs.WriteFileAtomic(sidecarPath, updated), "Overwrite sidecar with new settings");

        const auto result = tx.FinishUpdate(fs, assetPath, contentRoot, true);
        check(result == SidecarUpdateResult::Committed, "Successful UpdateAsset commits");
        QByteArray content;
        readSidecar(content);
        check(content == updated, "Sidecar keeps new content after commit");
        check(!tx.active(), "Transaction finished after commit");
    }

    // No prior sidecar: rollback removes the newly created file.
    {
        check(fs.DeleteFile(sidecarPath), "Remove sidecar before Begin");
        SidecarTransaction tx;
        tx.Begin(fs, assetPath, contentRoot);
        check(tx.active(), "Transaction active without prior sidecar");
        check(fs.WriteFileAtomic(sidecarPath, updated), "Create new sidecar");

        const auto result = tx.FinishUpdate(fs, assetPath, contentRoot, false);
        check(result == SidecarUpdateResult::RolledBack, "Failed UpdateAsset rolls back new sidecar");
        check(!fs.FileExists(sidecarPath), "Newly created sidecar removed by rollback");
    }

    // UpdateAsset of a different asset never touches our sidecar.
    {
        check(fs.WriteFileAtomic(sidecarPath, original), "Write original sidecar");
        SidecarTransaction tx;
        tx.Begin(fs, assetPath, contentRoot);
        check(fs.WriteFileAtomic(sidecarPath, updated), "Overwrite sidecar with new settings");

        const QString otherAsset = QDir(contentRoot).filePath(QStringLiteral("Models/Other.obj"));
        const auto result = tx.FinishUpdate(fs, otherAsset, contentRoot, false);
        check(result == SidecarUpdateResult::NotMine, "Different asset UpdateAsset is not ours");
        QByteArray content;
        readSidecar(content);
        check(content == updated, "Different asset failure keeps our saved sidecar");
        check(!tx.active(), "Stale transaction discarded on different asset");
    }

    // Begin with a disk path matches FinishUpdate with the /Game virtual path.
    {
        check(fs.WriteFileAtomic(sidecarPath, original), "Write original sidecar");
        SidecarTransaction tx;
        tx.Begin(fs, assetPath, contentRoot);
        check(fs.WriteFileAtomic(sidecarPath, updated), "Overwrite sidecar with new settings");

        const auto result = tx.FinishUpdate(fs, QStringLiteral("/Game/Models/Box.obj"), contentRoot, false);
        check(result == SidecarUpdateResult::RolledBack, "Virtual path matches disk path transaction");
        QByteArray content;
        readSidecar(content);
        check(content == original, "Sidecar restored via virtual path match");
    }

    // Inactive transaction ignores UpdateAsset results; Commit discards the backup.
    {
        SidecarTransaction tx;
        check(tx.FinishUpdate(fs, assetPath, contentRoot, false) == SidecarUpdateResult::NotMine,
              "Inactive transaction ignores UpdateAsset");
        check(fs.WriteFileAtomic(sidecarPath, original), "Write original sidecar");
        tx.Begin(fs, assetPath, contentRoot);
        check(fs.WriteFileAtomic(sidecarPath, updated), "Overwrite sidecar with new settings");
        tx.Commit();
        check(!tx.active(), "Commit deactivates transaction");
        check(!tx.Rollback(fs), "Rollback after Commit does nothing");
        QByteArray content;
        readSidecar(content);
        check(content == updated, "Commit keeps the new sidecar content");
    }

    outChecks += checks;
}

} // namespace SidecarTransactionTests