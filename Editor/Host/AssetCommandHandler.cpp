#include "AssetCommandHandler.h"
#include <Engine/Runtime/EditorReloadApi.h>
#include <QFile>

AssetCommandHandler::AssetCommandHandler() = default;

bool AssetCommandHandler::CanHandle(EditorCommand::Type type) const
{
    switch (type)
    {
    case EditorCommand::ImportModel:
    case EditorCommand::ImportTexture:
    case EditorCommand::ReloadProject:
    case EditorCommand::RunTask:
        return true;
    default:
        return false;
    }
}

void AssetCommandHandler::Handle(const EditorCommand& cmd,
                                 Context& ctx,
                                 EditorHistory& history,
                                 IEditorHost::EditorState playState,
                                 void* viewportHwnd,
                                 uint32_t viewportWidth,
                                 uint32_t viewportHeight,
                                 EditorCommand& completedCmd,
                                 uint32_t& result,
                                 QString& error)
{
    completedCmd = cmd;
    const bool hasScene = (ctx.scene.context != nullptr);

    if (cmd.type == EditorCommand::RunTask)
    {
        if (hasScene && cmd.task)
            cmd.task(ctx.scene);
        result = EngineSceneOk;
        return;
    }

    if (cmd.type == EditorCommand::ImportModel || cmd.type == EditorCommand::ImportTexture)
    {
        if (!hasScene)
        {
            result = EngineSceneReadOnly;
            return;
        }
        char path[4096]{};
        char errBuf[1024]{};
        result = ctx.scene.importAsset(ctx.scene.context,
                                       cmd.path.toUtf8().constData(),
                                       cmd.type == EditorCommand::ImportTexture ? 1 : 0,
                                       path,
                                       sizeof(path),
                                       errBuf,
                                       sizeof(errBuf));
        if (result == EngineSceneOk)
            completedCmd.path = QString::fromUtf8(path);
        error = QString::fromUtf8(errBuf);
        return;
    }

    if (cmd.type == EditorCommand::ReloadProject)
    {
        if (!hasScene || playState != IEditorHost::Edit)
        {
            result = EngineSceneReadOnly;
            return;
        }

        const auto* reload = ctx.host.getService
            ? static_cast<const EngineEditorReloadApi*>(ctx.host.getService(ctx.host.user, EngineEditorReloadService, 1))
            : nullptr;

        if (!reload || !reload->reload)
        {
            result = EngineSceneReadOnly;
            error = QStringLiteral("В этой версии runtime нет hot reload. Откройте проект заново.");
            return;
        }

        QString checkpoint;
        try
        {
            const auto selectedName = ctx.getSelectedObjectName ? ctx.getSelectedObjectName() : std::string{};
            checkpoint = history.Checkpoint(ctx.scene);
            char errBuf[2048]{};
            EngineEditorReloadBindings bindings{};
            result = reload->reload(reload->context,
                                    cmd.path.toUtf8().constData(),
                                    checkpoint.toUtf8().constData(),
                                    viewportHwnd,
                                    viewportWidth,
                                    viewportHeight,
                                    &bindings,
                                    errBuf,
                                    sizeof(errBuf));

            if (result == EngineSceneOk && bindings.scene.context == ctx.scene.context)
            {
                QFile::remove(checkpoint);
                completedCmd.name = QStringLiteral("unchanged");
                return;
            }

            if (result == EngineSceneOk)
            {
                if (ctx.onReloadBindings)
                    ctx.onReloadBindings(bindings.scene, bindings.data, bindings.selection, bindings.input);

                history.Reset();

                if (ctx.setSelectedId)
                {
                    uint64_t matchedId = 0;
                    auto visitor = [](void* context, const EngineSceneObjectInfo* info) {
                        auto* list = static_cast<std::vector<std::pair<uint64_t, std::string>>*>(context);
                        list->push_back({info->id, info->name ? info->name : ""});
                    };
                    std::vector<std::pair<uint64_t, std::string>> objects;
                    EngineSceneInfo sinfo{};
                    bindings.scene.readActiveScene(bindings.scene.context, &sinfo, visitor, &objects);
                    for (const auto& o : objects)
                    {
                        if (o.second == selectedName)
                        {
                            matchedId = o.first;
                            break;
                        }
                    }
                    ctx.setSelectedId(matchedId);
                }

                if (ctx.publishScene) ctx.publishScene();
            }

            QFile::remove(checkpoint);
            error = QString::fromUtf8(errBuf);
        }
        catch (const std::exception& e)
        {
            QFile::remove(checkpoint);
            result = EngineSceneFailure;
            error = QString::fromUtf8(e.what());
        }
    }
}

