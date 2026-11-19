#include "SceneClipboard.h"
#include "SceneCommandHandler.h"

#include <QFile>
#include <QStringList>
#include <algorithm>
#include <map>

uint32_t SceneClipboard::Apply(const EditorCommand& cmd, SceneCommandHandler::Context& ctx, SceneCommandHandler& handler,
                               EditorHistory& history, IEditorHost::EditorState playState, QString& error)
{
    if (!ctx.scene.context)
    {
        error = QStringLiteral("Этот проект поддерживает только preview.");
        return EngineSceneReadOnly;
    }
    if (cmd.type != EditorCommand::CopyObject && playState != IEditorHost::Edit)
    {
        error = QStringLiteral("Остановите Play перед редактированием.");
        return EngineSceneReadOnly;
    }

    auto& api = ctx.scene.scene;
    char errorBuf[1024]{};

    if (cmd.type == EditorCommand::CopyObject || cmd.type == EditorCommand::CutObject)
    {
        const auto view = handler.ReadSnapshot();
        std::vector<uint64_t> ids = handler.SelectedIds();
        if (ids.empty() && handler.SelectedId()) ids.push_back(handler.SelectedId());
        items_.clear();
        for (const auto& o : view.objects)
            if (std::find(ids.begin(), ids.end(), o.id) != ids.end()) items_.push_back(o);
        if (items_.empty())
        {
            error = QStringLiteral("Нет выделенных объектов для копирования.");
            return EngineSceneInvalidArgument;
        }
        if (cmd.type == EditorCommand::CopyObject) return EngineSceneOk;

        history.BreakCoalescing();
        const QString checkpoint = history.Checkpoint(ctx.scene);
        uint32_t result = EngineSceneOk;
        if (ids.size() > 1)
        {
            if (!ctx.batch || !ctx.batch->apply)
            {
                QFile::remove(checkpoint);
                error = QStringLiteral("This game DLL supports single-object editing. Rebuild a project copy with the current SDK for group operations.");
                return EngineSceneFailure;
            }
            std::vector<uint64_t> output(ids.size());
            result = ctx.batch->apply(ctx.batch->context, EngineBatchDelete, ids.data(), static_cast<uint32_t>(ids.size()),
                                      ids.front(), nullptr, output.data(), errorBuf, sizeof(errorBuf));
        }
        else
            result = api.removeObject(api.context, ids.front());
        if (result == EngineSceneOk)
        {
            history.RecordChange(checkpoint);
            handler.SetSelectedId(0);
            handler.MarkDirty();
        }
        else
            QFile::remove(checkpoint);
        handler.PublishScene(ctx, playState);
        error = QString::fromUtf8(errorBuf);
        return result;
    }

    if (items_.empty())
    {
        error = QStringLiteral("Буфер обмена сцены пуст.");
        return EngineSceneInvalidArgument;
    }
    history.BreakCoalescing();
    const QString checkpoint = history.Checkpoint(ctx.scene);
    const auto view = handler.ReadSnapshot();

    std::vector<IEditorHost::SceneObject> ordered;
    std::vector<IEditorHost::SceneObject> remaining = items_;
    while (!remaining.empty())
    {
        const auto it = std::find_if(remaining.begin(), remaining.end(), [&](const auto& o) {
            return std::none_of(remaining.begin(), remaining.end(), [&](const auto& p) { return p.id == o.parent; });
        });
        if (it == remaining.end()) break;
        ordered.push_back(*it);
        remaining.erase(it);
    }
    ordered.insert(ordered.end(), remaining.begin(), remaining.end());

    QStringList usedNames;
    for (const auto& o : view.objects) usedNames << QString::fromStdString(o.name);
    std::map<uint64_t, uint64_t> remap;
    std::vector<uint64_t> created;
    uint32_t result = EngineSceneOk;
    for (const auto& o : ordered)
    {
        const QString base = QString::fromStdString(o.name);
        QString name = base;
        int suffix = 1;
        while (usedNames.contains(name)) name = base + QStringLiteral(" %1").arg(++suffix);
        usedNames << name;
        auto transform = o.transform;
        transform.position[0] += 1;
        const QString model = QString::fromStdString(o.modelAsset);
        uint64_t id = 0;
        result = model.isEmpty()
            ? api.createObject(api.context, o.type.c_str(), name.toUtf8().constData(), &transform, &id)
            : ctx.scene.createModel(ctx.scene.context, model.toUtf8().constData(), name.toUtf8().constData(), &transform, &id, errorBuf, sizeof(errorBuf));
        if (result != EngineSceneOk) break;
        created.push_back(id);
        remap[o.id] = id;
        uint64_t parent = o.parent;
        const auto mapped = remap.find(parent);
        if (mapped != remap.end()) parent = mapped->second;
        if (parent) result = ctx.scene.setParent(ctx.scene.context, id, parent, o.inheritScale ? 1u : 0u);
        const QString texture = QString::fromStdString(o.textureAsset);
        if (result == EngineSceneOk && !texture.isEmpty())
            result = ctx.scene.setTexture(ctx.scene.context, id, texture.toUtf8().constData(), errorBuf, sizeof(errorBuf));
        const QString material = QString::fromStdString(o.materialAsset);
        if (result == EngineSceneOk && !material.isEmpty())
            result = ctx.scene.setMaterial ? ctx.scene.setMaterial(ctx.scene.context, id, material.toUtf8().constData(), errorBuf, sizeof(errorBuf)) : EngineSceneReadOnly;
        const QString script = QString::fromStdString(o.scriptAsset);
        if (result == EngineSceneOk && !script.isEmpty())
            result = ctx.script && ctx.script->set ? ctx.script->set(ctx.script->context, id, script.toUtf8().constData(), errorBuf, sizeof(errorBuf)) : EngineSceneReadOnly;
        const QString scriptVariables = QString::fromStdString(o.scriptVariables);
        if (result == EngineSceneOk && !scriptVariables.isEmpty())
            result = ctx.script && ctx.script->setVariables ? ctx.script->setVariables(ctx.script->context, id, scriptVariables.toUtf8().constData(), errorBuf, sizeof(errorBuf)) : EngineSceneReadOnly;
        if (result == EngineSceneOk && o.hasPointLight)
            result = (ctx.pointLightV2 && ctx.pointLightV2->set)
                ? ctx.pointLightV2->set(ctx.pointLightV2->context, id, &o.pointLight, o.lightCastShadows ? 1u : 0u)
                : (api.setPointLight ? api.setPointLight(api.context, id, &o.pointLight) : EngineSceneReadOnly);
        if (result == EngineSceneOk && o.hasPhysics)
            result = (ctx.physicsObject && ctx.physicsObject->write) ? ctx.physicsObject->write(ctx.physicsObject->context, id, &o.physics) : EngineSceneReadOnly;
        const QString projectData = QString::fromStdString(o.projectData);
        if (result == EngineSceneOk && !projectData.isEmpty())
            result = ctx.projectData.write ? ctx.projectData.write(ctx.projectData.context, id, projectData.toUtf8().constData()) : EngineSceneReadOnly;
        if (result == EngineSceneOk && !projectData.isEmpty())
            result = ctx.projectData.remapDuplicate ? ctx.projectData.remapDuplicate(ctx.projectData.context, id, o.id) : EngineSceneReadOnly;
        if (result != EngineSceneOk) break;
    }
    if (result == EngineSceneOk)
    {
        history.RecordChange(checkpoint);
        handler.SetSelectedIds(created);
        handler.MarkDirty();
    }
    else
    {
        QFile::remove(checkpoint);
        for (uint64_t id : created) api.removeObject(api.context, id);
    }
    handler.PublishScene(ctx, playState);
    error = QString::fromUtf8(errorBuf);
    return result;
}