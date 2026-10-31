#include "SceneCommandHandler.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <algorithm>
#include <chrono>

SceneCommandHandler::SceneCommandHandler() = default;

bool SceneCommandHandler::CanHandle(EditorCommand::Type type) const
{
    switch (type)
    {
    case EditorCommand::FetchScene:
    case EditorCommand::SelectObject:
    case EditorCommand::RefreshScene:
    case EditorCommand::UpdateObjectTransform:
    case EditorCommand::CreateObject:
    case EditorCommand::CreateModel:
    case EditorCommand::SetModel:
    case EditorCommand::SetTexture:
    case EditorCommand::SetMaterial:
    case EditorCommand::SetPointLight:
    case EditorCommand::SetProperty:
    case EditorCommand::SetProjectData:
    case EditorCommand::ConfigurePlay:
    case EditorCommand::RemoveObject:
    case EditorCommand::RenameObject:
    case EditorCommand::DuplicateObject:
    case EditorCommand::SetParent:
    case EditorCommand::SaveScene:
    case EditorCommand::LoadScene:
    case EditorCommand::NewScene:
    case EditorCommand::Undo:
    case EditorCommand::Redo:
        return true;
    default:
        return false;
    }
}

void SceneCommandHandler::Handle(const EditorCommand& cmd,
                                 Context& ctx,
                                 EditorHistory& history,
                                 IEditorHost::EditorState playState,
                                 uint32_t& result,
                                 QString& error)
{
    if (!ctx.scene.context)
    {
        result = EngineSceneReadOnly;
        error = QStringLiteral("Этот проект поддерживает только preview.");
        return;
    }

    auto& api = ctx.scene.scene;
    result = EngineSceneOk;
    char errorBuf[1024]{};
    QString checkpoint;

    const bool mutation = (cmd.type == EditorCommand::UpdateObjectTransform || cmd.type == EditorCommand::CreateObject ||
        cmd.type == EditorCommand::RemoveObject || cmd.type == EditorCommand::RenameObject || cmd.type == EditorCommand::DuplicateObject ||
        cmd.type == EditorCommand::SetModel || cmd.type == EditorCommand::CreateModel || cmd.type == EditorCommand::SetTexture ||
        cmd.type == EditorCommand::SetMaterial || cmd.type == EditorCommand::SetPointLight || cmd.type == EditorCommand::SetProjectData ||
        cmd.type == EditorCommand::ConfigurePlay || cmd.type == EditorCommand::SetProperty || cmd.type == EditorCommand::SetParent);

    if ((mutation || cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene ||
        cmd.type == EditorCommand::Undo || cmd.type == EditorCommand::Redo) && playState != IEditorHost::Edit)
    {
        result = EngineSceneReadOnly;
        error = QStringLiteral("Остановите Play перед редактированием.");
        return;
    }

    try
    {
        if (cmd.type != EditorCommand::UpdateObjectTransform && cmd.type != EditorCommand::RefreshScene)
            history.BreakCoalescing();
        const auto now = std::chrono::steady_clock::now();
        const bool coalesced = history.ShouldCoalesce(cmd, now);

        if (mutation && !coalesced)
            checkpoint = history.Checkpoint(ctx.scene);

        if (cmd.type == EditorCommand::SelectObject)
        {
            if (cmd.name == "toggle" && ctx.batch) PickSelection(cmd.objectId, true);
            else { selectedId_ = cmd.objectId; selectedIds_ = cmd.objectIds; }
        }
        else if (cmd.type == EditorCommand::FetchScene)
        {
            auto view = ReadSnapshot();
            view.objects.erase(std::remove_if(view.objects.begin(), view.objects.end(),
                [](const auto& o) { return o.gameplayKind == 3 || o.type == "Gameplay.PlaySettings"; }), view.objects.end());
            auto found = std::find_if(view.objects.begin(), view.objects.end(),
                [&](const auto& o) { return o.id == selectedId_; });
            selectedId_ = view.objects.empty() ? 0 : (found == view.objects.end() || ++found == view.objects.end() ? view.objects.front().id : found->id);
            selectedIds_.clear();
        }
                else if ((cmd.type == EditorCommand::UpdateObjectTransform || cmd.type == EditorCommand::RemoveObject || cmd.type == EditorCommand::DuplicateObject) && selectedIds_.size() > 1) {
                    const auto* batch = ctx.batch;
                    if (!batch || !batch->apply) throw std::runtime_error("This game DLL supports single-object editing. Rebuild a project copy with the current SDK for group operations.");
                    auto target = cmd.transform;
                    if (cmd.path == "nudge") for (const auto& o : ReadSnapshot().objects) if (o.id == cmd.objectId) {
                        target = o.transform; target.position[0] += cmd.right; target.position[1] += cmd.up; target.position[2] += cmd.forward;
                    }
                    std::vector<uint64_t> output(selectedIds_.size());
                    const auto active = std::find(selectedIds_.begin(),selectedIds_.end(),cmd.objectId);
                    const auto activeIndex = active == selectedIds_.end() ? 0 : static_cast<size_t>(active-selectedIds_.begin());
                    const auto operation = cmd.type == EditorCommand::RemoveObject ? EngineBatchDelete : cmd.type == EditorCommand::DuplicateObject ? EngineBatchDuplicate : EngineBatchTransform;
                    result = batch->apply(batch->context,operation,selectedIds_.data(),static_cast<uint32_t>(selectedIds_.size()),cmd.objectId,&target,output.data(),errorBuf,sizeof(errorBuf));
                    if(result == EngineSceneOk) {
                        selectedIds_ = output; selectedId_ = output[activeIndex];
                        selectedIds_.erase(std::remove(selectedIds_.begin(),selectedIds_.end(),0),selectedIds_.end());
                        history.OnTransformApplied(cmd, now);
                    }
                }
        else if (cmd.type == EditorCommand::UpdateObjectTransform)
        {
            auto transform = cmd.transform;
            if (cmd.path == QStringLiteral("nudge"))
            {
                for (const auto& o : ReadSnapshot().objects)
                {
                    if (o.id == cmd.objectId)
                    {
                        transform = o.transform;
                        transform.position[0] += cmd.right;
                        transform.position[1] += cmd.up;
                        transform.position[2] += cmd.forward;
                        break;
                    }
                }
            }
            result = api.setTransform(api.context, cmd.objectId, &transform);
            if (result == EngineSceneOk)
                history.OnTransformApplied(cmd, now);
        }
        else if (cmd.type == EditorCommand::RemoveObject)
        {
            result = api.removeObject(api.context, cmd.objectId);
        }
        else if (cmd.type == EditorCommand::RenameObject)
        {
            result = api.renameObject(api.context, cmd.objectId, cmd.name.toUtf8().constData());
        }
        else if (cmd.type == EditorCommand::SetParent)
        {
            result = ctx.scene.setParent ? ctx.scene.setParent(ctx.scene.context, cmd.objectId, cmd.parentId, 1) : EngineSceneReadOnly;
        }
        else if (cmd.type == EditorCommand::SetModel)
        {
            const auto selectedName = GetSelectedObjectName();
            QFile input(checkpoint);
            if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read model change checkpoint");
            auto document = QJsonDocument::fromJson(input.readAll()).object();
            auto objects = document["objects"].toArray();
            bool found = false;
            for (int i = 0; i < objects.size(); ++i)
            {
                auto object = objects[i].toObject();
                if (object["id"].toInteger() != static_cast<qint64>(cmd.objectId)) continue;
                const auto type = object["type"].toString();
                if (type != "Engine.Model" && !type.startsWith("Primitive."))
                    throw std::runtime_error("Only model objects and primitives support model replacement");
                object["type"] = "Engine.Model";
                auto assets = object["assets"].toObject();
                assets["model"] = cmd.path;
                if (!assets.contains("texture")) assets["texture"] = "";
                object["assets"] = assets;
                objects[i] = object;
                found = true;
            }
            if (!found) throw std::runtime_error("Object no longer exists");
            document["objects"] = objects;

            QTemporaryFile candidate(history.GetTemporaryFilePath(QStringLiteral("model-XXXXXX.scene.json")));
            const auto bytes = QJsonDocument(document).toJson();
            if (!candidate.open() || candidate.write(bytes) != bytes.size() || !candidate.flush())
                throw std::runtime_error("Cannot write model change candidate");
            candidate.close();
            uint64_t id = 0;
            result = ctx.scene.loadScene(ctx.scene.context, candidate.fileName().toUtf8().constData(), &id, errorBuf, sizeof(errorBuf));
            if (result == EngineSceneOk)
            {
                IEditorHost::Snapshot restored;
                EngineSceneInfo sinfo{};
                ctx.scene.readActiveScene(ctx.scene.context, &sinfo, SceneVisitor, &restored);
                selectedId_ = 0;
                for (const auto& o : restored.objects)
                    if (o.name == selectedName) selectedId_ = o.id;
            }
        }
        else if (cmd.type == EditorCommand::SetTexture)
        {
            result = ctx.scene.setTexture(ctx.scene.context, cmd.objectId, cmd.path.toUtf8().constData(), errorBuf, sizeof(errorBuf));
        }
        else if (cmd.type == EditorCommand::SetMaterial)
        {
            result = ctx.scene.setMaterial
                ? ctx.scene.setMaterial(ctx.scene.context, cmd.objectId, cmd.path.toUtf8().constData(), errorBuf, sizeof(errorBuf))
                : EngineSceneReadOnly;
        }
        else if (cmd.type == EditorCommand::SetPointLight)
        {
            result = api.setPointLight ? api.setPointLight(api.context, cmd.objectId, &cmd.pointLight) : EngineSceneReadOnly;
        }
        else if (cmd.type == EditorCommand::SetProperty)
        {
            result = ctx.authoring && ctx.authoring->setProperty ? ctx.authoring->setProperty(ctx.authoring->context, cmd.objectId, cmd.path.toUtf8().constData(), cmd.name.toUtf8().constData(), errorBuf, sizeof(errorBuf)) : EngineSceneReadOnly;
        }
        else if (cmd.type == EditorCommand::SetProjectData)
        {
            result = ctx.projectData.write ? ctx.projectData.write(ctx.projectData.context, cmd.objectId, cmd.path.toUtf8().constData()) : EngineSceneReadOnly;
        }
        else if (cmd.type == EditorCommand::ConfigurePlay)
        {
            uint64_t id = cmd.objectId;
            bool created = false;
            if (!ctx.projectData.write)
            {
                result = EngineSceneReadOnly;
            }
            else
            {
                if (!id)
                {
                    result = api.createObject(api.context, "Gameplay.PlaySettings", "Play Settings", &cmd.transform, &id);
                    created = (result == EngineSceneOk);
                }
                if (result == EngineSceneOk)
                    result = ctx.projectData.write(ctx.projectData.context, id, cmd.path.toUtf8().constData());
                if (result != EngineSceneOk && created)
                    api.removeObject(api.context, id);
            }
        }
        else if (cmd.type == EditorCommand::CreateObject || cmd.type == EditorCommand::DuplicateObject || cmd.type == EditorCommand::CreateModel)
        {
            auto view = ReadSnapshot();
            QString type = cmd.path, base = cmd.name;
            QString model = (cmd.type == EditorCommand::CreateModel ? cmd.path : QString());
            QString texture, material, projectData;
            EngineSceneTransform transform = cmd.transform;
            uint64_t parent = cmd.parentId;
            bool inherit = true;
            bool hasLight = false;
            EngineScenePointLight light{};

            if (cmd.type == EditorCommand::DuplicateObject)
            {
                auto found = std::find_if(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == cmd.objectId; });
                if (found == view.objects.end()) throw std::runtime_error("Object no longer exists");
                type = QString::fromStdString(found->type);
                base = QString::fromStdString(found->name);
                transform = found->transform;
                transform.position[0] += 1;
                parent = found->parent;
                inherit = found->inheritScale;
                model = QString::fromStdString(found->modelAsset);
                texture = QString::fromStdString(found->textureAsset);
                material = QString::fromStdString(found->materialAsset);
                hasLight = found->hasPointLight;
                light = found->pointLight;
                projectData = QString::fromStdString(found->projectData);
            }
            QString name = base;
            int suffix = 1;
            while (std::any_of(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.name == name.toStdString(); }))
                name = base + QStringLiteral(" %1").arg(++suffix);

            uint64_t id = 0;
            result = model.isEmpty()
                ? api.createObject(api.context, type.toUtf8().constData(), name.toUtf8().constData(), &transform, &id)
                : ctx.scene.createModel(ctx.scene.context, model.toUtf8().constData(), name.toUtf8().constData(), &transform, &id, errorBuf, sizeof(errorBuf));

            if (result == EngineSceneOk)
            {
                if (parent) result = ctx.scene.setParent(ctx.scene.context, id, parent, inherit);
                if (result == EngineSceneOk && !texture.isEmpty())
                    result = ctx.scene.setTexture(ctx.scene.context, id, texture.toUtf8().constData(), errorBuf, sizeof(errorBuf));
                if (result == EngineSceneOk && !material.isEmpty())
                    result = ctx.scene.setMaterial ? ctx.scene.setMaterial(ctx.scene.context, id, material.toUtf8().constData(), errorBuf, sizeof(errorBuf)) : EngineSceneReadOnly;
                if (result == EngineSceneOk && hasLight)
                    result = api.setPointLight ? api.setPointLight(api.context, id, &light) : EngineSceneReadOnly;
                if (result == EngineSceneOk && !projectData.isEmpty())
                    result = ctx.projectData.write ? ctx.projectData.write(ctx.projectData.context, id, projectData.toUtf8().constData()) : EngineSceneReadOnly;
                if (result == EngineSceneOk && !projectData.isEmpty())
                    result = ctx.projectData.remapDuplicate ? ctx.projectData.remapDuplicate(ctx.projectData.context, id, cmd.objectId) : EngineSceneReadOnly;
                if (result == EngineSceneOk) { selectedId_ = id; selectedIds_.clear(); }
                else api.removeObject(api.context, id);
            }
        }
        else if (cmd.type == EditorCommand::SaveScene)
        {
            result = ctx.scene.saveScene(ctx.scene.context, cmd.path.toUtf8().constData(), errorBuf, sizeof(errorBuf));
            if (result == EngineSceneOk) dirty_ = false;
        }
        else if (cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene)
        {
            QString path = cmd.path;
            if (cmd.type == EditorCommand::NewScene)
                path = history.CreateEmptySceneFile();

            uint64_t id = 0;
            result = ctx.scene.loadScene(ctx.scene.context, path.toUtf8().constData(), &id, errorBuf, sizeof(errorBuf));
            if (result == EngineSceneOk)
            {
                selectedId_ = 0;
                dirty_ = (cmd.type == EditorCommand::NewScene || cmd.name == "recovery");
                history.ClearHistory();
            }
        }
        else if (cmd.type == EditorCommand::Undo)
        {
            if (history.Undo(ctx.scene, result, errorBuf, sizeof(errorBuf)))
            {
                selectedId_ = 0;
                dirty_ = true;
            }
        }
        else if (cmd.type == EditorCommand::Redo)
        {
            if (history.Redo(ctx.scene, result, errorBuf, sizeof(errorBuf)))
            {
                selectedId_ = 0;
                dirty_ = true;
            }
        }

        if (mutation)
        {
            if (result == EngineSceneOk)
            {
                history.RecordChange(checkpoint);
                dirty_ = true;
            }
            else
            {
                QFile::remove(checkpoint);
            }
        }
    }
    catch (const std::exception& e)
    {
        QFile::remove(checkpoint);
        result = EngineSceneFailure;
        std::snprintf(errorBuf, sizeof(errorBuf), "%s", e.what());
    }

    PublishScene(ctx, playState);
    error = QString::fromUtf8(errorBuf);
}

void SceneCommandHandler::SetSelectedId(uint64_t id)
{
    selectedId_ = id;
    selectedIds_.clear();
}

void SceneCommandHandler::PickSelection(uint64_t id, bool toggle)
{
    selectedId_ = id;
    auto found = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (toggle && id) {
        if (found == selectedIds_.end()) selectedIds_.push_back(id);
        else { selectedIds_.erase(found); selectedId_ = selectedIds_.empty() ? 0 : selectedIds_.back(); }
    } else if (!id || found == selectedIds_.end()) {
        selectedIds_.clear(); if (id) selectedIds_.push_back(id);
    }
}

IEditorHost::Snapshot SceneCommandHandler::ReadSnapshot()
{
    std::lock_guard lock(sceneMutex_);
    return snapshot_;
}

void ENGINE_MODULE_CALL SceneCommandHandler::SceneVisitor(void* context, const EngineSceneObjectInfo* info)
{
    auto& view = *static_cast<IEditorHost::Snapshot*>(context);
    view.objects.push_back({
        info->id,
        info->parentId,
        info->inheritParentScale != 0,
        info->name ? info->name : "",
        info->typeName ? info->typeName : "",
        info->transform,
        info->modelAsset ? info->modelAsset : "",
        info->textureAsset ? info->textureAsset : "",
        info->hasPointLight != 0,
        info->pointLight,
        info->materialAsset ? info->materialAsset : ""
    });
}

void SceneCommandHandler::PublishScene(const Context& ctx, IEditorHost::EditorState playState)
{
    if (!ctx.scene.context) return;

    IEditorHost::Snapshot view;
    EngineSceneInfo sinfo{};
    if (ctx.scene.readActiveScene(ctx.scene.context, &sinfo, SceneVisitor, &view) != EngineSceneOk)
        return;

    if (ctx.scene.scene.listTypes)
    {
        ctx.scene.scene.listTypes(ctx.scene.scene.context, [](void* p, const EngineSceneTypeInfo* t) {
            static_cast<IEditorHost::Snapshot*>(p)->types.push_back({t->typeName ? t->typeName : "", t->displayName ? t->displayName : ""});
        }, &view);
    }

    if (ctx.projectData.read)
    {
        for (auto& object : view.objects)
            ctx.projectData.read(ctx.projectData.context, object.id,
                [](void* p, const char* value) { *static_cast<std::string*>(p) = (value ? value : ""); }, &object.projectData);
    }

    if (ctx.viewportInput.objectKind)
    {
        for (auto& object : view.objects)
            ctx.viewportInput.objectKind(ctx.viewportInput.context, object.id, &object.gameplayKind);
    }

    if (std::none_of(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == selectedId_; }))
        selectedId_ = 0;

    if(!selectedId_)selectedIds_.clear();
    if(!ctx.batch) { selectedIds_.clear(); if(selectedId_)selectedIds_.push_back(selectedId_); }
    selectedIds_.erase(std::remove_if(selectedIds_.begin(),selectedIds_.end(),[&](auto id){return std::none_of(view.objects.begin(),view.objects.end(),[&](const auto& o){return o.id==id;});}),selectedIds_.end());
    if(selectedId_ && std::find(selectedIds_.begin(),selectedIds_.end(),selectedId_)==selectedIds_.end()) { selectedIds_.clear(); selectedIds_.push_back(selectedId_); }
    // A selected descendant follows its selected ancestor. Use that ancestor as
    // the inspector/drag anchor so absolute local targets remain stable per frame.
    auto ancestor = selectedId_;
    while (ancestor) {
        const auto item=std::find_if(view.objects.begin(),view.objects.end(),[&](const auto& o){return o.id==ancestor;});
        ancestor=item==view.objects.end()?0:item->parent;
        if(ancestor && std::find(selectedIds_.begin(),selectedIds_.end(),ancestor)!=selectedIds_.end())selectedId_=ancestor;
    }
    view.authoring=ctx.authoring!=nullptr;
    if(const auto* authoring=ctx.authoring)for(auto& o:view.objects)if(o.id==selectedId_)
        authoring->describe(authoring->context,o.id,[](void* p,const char* text){*static_cast<std::string*>(p)=text;},&o.propertySchema);
    view.selectedIds = selectedIds_;
    view.batchEditing = ctx.batch != nullptr;
    view.selected = selectedId_; view.dirty = dirty_; view.canUndo = false; view.canRedo = false;
    hasSelection_.store(selectedId_ != 0);
    if (const auto* batch = ctx.batch; batch && batch->select) batch->select(batch->context,selectedIds_.data(),playState==IEditorHost::Edit ? static_cast<uint32_t>(selectedIds_.size()) : 0);
    else if (ctx.selection.select) ctx.selection.select(ctx.selection.context, playState == IEditorHost::Edit ? selectedId_ : 0);

    std::lock_guard lock(sceneMutex_);
    view.serial = snapshot_.serial + 1;
    snapshot_ = std::move(view);
}

std::string SceneCommandHandler::GetSelectedObjectName()
{
    const auto view = ReadSnapshot();
    for (const auto& obj : view.objects)
        if (obj.id == view.selected) return obj.name;
    return {};
}

void SceneCommandHandler::FetchSceneAndSelectNext(const std::function<void(const EditorCommand&)>& postCommand)
{
    EditorCommand c;
    c.type = EditorCommand::FetchScene;
    if (postCommand) postCommand(c);
}

void SceneCommandHandler::MoveSelectedObject(float right, float up, float forward, const std::function<void(const EditorCommand&)>& postCommand)
{
    const auto view = ReadSnapshot();
    for (const auto& obj : view.objects)
    {
        if (obj.id == view.selected)
        {
            EditorCommand c;
            c.type = EditorCommand::UpdateObjectTransform;
            c.objectId = obj.id;
            c.transform = obj.transform;
            c.path = QStringLiteral("nudge");
            c.right = right;
            c.up = up;
            c.forward = forward;
            if (postCommand) postCommand(c);
            break;
        }
    }
}
