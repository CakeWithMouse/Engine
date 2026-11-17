#include "MainWindow.h"
#include "QtHost.h"
#include "CommandRegistry.h"
#include "AssetOps.h"
#include "AssetExtensions.h"
#include "FileService.h"
#include "Logger.h"
#include "CameraGuide.h"

#include <QDir>
#include <QFileInfo>
#include <glm/gtc/matrix_inverse.hpp>

namespace
{
// Local transform that keeps the child's world pose under a new parent. Rejects cycles
// (child is an ancestor of the new parent) and degenerate scale the engine would refuse.
bool KeepWorldTransform(const IEditorHost::Snapshot& scene, uint64_t childId, uint64_t parentId,
                        EngineSceneTransform& out)
{
    const IEditorHost::SceneObject* child = nullptr;
    const IEditorHost::SceneObject* parent = nullptr;
    for (const auto& o : scene.objects)
    {
        if (o.id == childId) child = &o;
        if (parentId != 0 && o.id == parentId) parent = &o;
    }
    if (!child || (parentId != 0 && !parent))
        return false;
    for (const IEditorHost::SceneObject* a = parent; a; )
    {
        if (a->id == childId)
            return false;
        const uint64_t up = a->parent;
        a = nullptr;
        if (up == 0) break;
        for (const auto& o : scene.objects)
            if (o.id == up) { a = &o; break; }
    }
    const glm::mat4 childWorld = CameraGuide::World(*child, scene);
    const glm::mat4 local = parent
        ? glm::inverse(CameraGuide::World(*parent, scene)) * childWorld
        : childWorld;
    glm::vec3 scale(1), translation(0), skew;
    glm::vec4 perspective;
    glm::quat rotation(1, 0, 0, 0);
    if (!glm::decompose(local, scale, rotation, translation, skew, perspective))
        return false;
    if (std::abs(scale.x) < 1e-6f || std::abs(scale.y) < 1e-6f || std::abs(scale.z) < 1e-6f)
        return false;
    rotation = glm::normalize(rotation);
    out.position[0] = translation.x; out.position[1] = translation.y; out.position[2] = translation.z;
    out.rotation[0] = rotation.x; out.rotation[1] = rotation.y; out.rotation[2] = rotation.z; out.rotation[3] = rotation.w;
    out.scale[0] = scale.x; out.scale[1] = scale.y; out.scale[2] = scale.z;
    return true;
}
} // namespace

void EditorMainWindow::RegisterDragDropCommands()
{
    const QString projectRoot = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);

    const auto canExecute = [this]() {
        return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
    };

    const auto moveOrCopy = [projectRoot, contentRoot](const CommandContext& ctx, bool copy) {
        FileService files(&Logger::Instance());
        files.SetProjectRoot(projectRoot);
        files.SetContentRoot(contentRoot);
        EditorCore::AssetOps operations(&files);

        const QStringList paths = ctx.parameters.value(QStringLiteral("paths")).toStringList();
        const QString destination = ctx.parameters.value(QStringLiteral("destination")).toString();
        if (paths.isEmpty() || destination.isEmpty())
            return false;

        const QDir projectDir(projectRoot);
        bool allOk = true;
        for (const QString& rawPath : paths)
        {
            const QString absolute = rawPath.startsWith(QStringLiteral("/Game/"))
                ? QDir::cleanPath(contentRoot + QStringLiteral("/") + rawPath.mid(6))
                : QDir::cleanPath(rawPath);
            const QFileInfo sourceInfo(absolute);
            if (!sourceInfo.exists())
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: source does not exist: %1").arg(rawPath));
                allOk = false;
                continue;
            }
            const QString sourceAbs = sourceInfo.absoluteFilePath();
            const QString destFolder = QDir::cleanPath(destination);
            if (sourceInfo.isDir() && (destFolder == sourceAbs || destFolder.startsWith(sourceAbs + QStringLiteral("/"))))
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: cannot move folder into itself: %1").arg(sourceAbs));
                allOk = false;
                continue;
            }
            const QString targetAbs = QDir(destFolder).filePath(sourceInfo.fileName());
            if (!copy && QDir::cleanPath(targetAbs) == sourceAbs)
                continue;

            const QString sourceRel = projectDir.relativeFilePath(sourceAbs);
            const QString baseTargetRel = projectDir.relativeFilePath(QDir::cleanPath(targetAbs));
            QString targetRel = baseTargetRel;
            int counter = 0;
            while (operations.Exists(targetRel))
                targetRel = EditorCore::AssetOps::ConflictName(baseTargetRel, ++counter);

            QString error;
            const bool ok = copy
                ? operations.Duplicate(sourceRel, targetRel, &error, nullptr, true)
                : operations.Rename(sourceRel, targetRel, &error, true);
            if (!ok)
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: %1").arg(error));
                allOk = false;
            }
            else
            {
                Logger::Instance().Info(QStringLiteral("AssetMoveCopy: %1 '%2' -> '%3'")
                    .arg(copy ? QStringLiteral("copied") : QStringLiteral("moved"), sourceRel, targetRel));
            }
        }
        return allOk;
    };

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Move"),
        QStringLiteral("Move Assets"),
        QString(),
        QStringLiteral("Move assets and their companion files to a project folder"),
        [moveOrCopy](const CommandContext& ctx) { return moveOrCopy(ctx, false); },
        canExecute
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Copy"),
        QStringLiteral("Copy Assets"),
        QString(),
        QStringLiteral("Copy assets and their companion files to a project folder"),
        [moveOrCopy](const CommandContext& ctx) { return moveOrCopy(ctx, true); },
        canExecute
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.SpawnModel"),
        QStringLiteral("Spawn Model"),
        QString(),
        QStringLiteral("Spawn a project model into the scene at a world position"),
        [this, contentRoot](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const QString model = ctx.parameters.value(QStringLiteral("model")).toString();
            if (model.isEmpty())
                return false;
            const QString normalized = EditorCore::NormalizeVirtualPath(model, contentRoot);
            EditorCommand c;
            c.type = EditorCommand::CreateModel;
            c.path = normalized;
            c.name = QFileInfo(normalized).completeBaseName();
            c.transform.position[0] = ctx.parameters.value(QStringLiteral("x"), 0.0).toFloat();
            c.transform.position[1] = ctx.parameters.value(QStringLiteral("y"), 0.0).toFloat();
            c.transform.position[2] = ctx.parameters.value(QStringLiteral("z"), 0.0).toFloat();
            const float scale = GetModelSpawnScale(normalized);
            c.transform.scale[0] = scale;
            c.transform.scale[1] = scale;
            c.transform.scale[2] = scale;
            host_->PostCommand(c);
            return true;
        },
        canExecute
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.AssignTexture"),
        QStringLiteral("Assign Texture"),
        QString(),
        QStringLiteral("Assign project texture to selected scene object"),
        [this, contentRoot](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
            if (targetId == 0)
                return false;
            const QString texture = ctx.parameters.value(QStringLiteral("texture")).toString();
            if (texture.isEmpty())
                return false;
            EditorCommand c;
            c.type = EditorCommand::SetTexture;
            c.objectId = targetId;
            c.path = EditorCore::NormalizeVirtualPath(texture, contentRoot);
            host_->PostCommand(c);
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit && selectedId_ != 0;
        }
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.Reparent"),
        QStringLiteral("Reparent Object"),
        QString(),
        QStringLiteral("Reparent a scene object preserving its world transform"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t childId = ctx.parameters.value(QStringLiteral("childId")).toULongLong();
            const uint64_t parentId = ctx.parameters.value(QStringLiteral("parentId")).toULongLong();
            if (childId == 0 || childId == parentId)
                return false;
            EngineSceneTransform local{};
            if (!KeepWorldTransform(host_->ReadSnapshot(), childId, parentId, local))
                return false;
            EditorCommand c;
            c.type = EditorCommand::SetParent;
            c.objectId = childId;
            c.parentId = parentId;
            c.path = QStringLiteral("keepworld");
            c.transform = local;
            host_->PostCommand(c);
            return true;
        },
        canExecute
    });
}

void EditorMainWindow::UnregisterDragDropCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Move"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Copy"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.SpawnModel"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.AssignTexture"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.Reparent"));
}