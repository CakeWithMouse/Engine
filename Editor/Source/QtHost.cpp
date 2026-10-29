#include "QtHost.h"
#include <Engine/Runtime/EditorReloadApi.h>
#include "MainWindow.h"
#include "EditorTheme.h"
#include <QApplication>
#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <algorithm>

namespace
{
    // argc/argv должны жить не меньше QApplication — статического хранения достаточно.
    int g_argc = 1;
    char g_arg0[] = "Editor.dll";
    char* g_argv[] = {g_arg0, nullptr};
}

EditorQtHost::EditorQtHost(const EngineModuleHost& host) : host_(host)
{
}

EditorQtHost::~EditorQtHost() { Stop(); }

bool EditorQtHost::Start()
{
    if (uiThread_.joinable()) return false;
    historyDirectory_ = std::make_unique<QTemporaryDir>();
    if (HasScene() && host_.contentRoot && *host_.contentRoot)
    {
        char error[1024]{};
        if (scene_.setContentRoot(scene_.context, host_.contentRoot, error, sizeof(error)) != EngineSceneOk)
        { Log(error); return false; }
    }
    if (HasScene()) PublishScene();
    uiReady_.store(false);
    uiThread_ = std::thread([this] { UiThreadMain(); });
    for (int i = 0; i < 200 && !uiReady_.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if (!uiReady_.load())
    {
        Stop();
        return false;
    }
    Log("EditorStart");
    Log(host_.projectName ? host_.projectName : "(no project)");
    Log(host_.projectFile ? host_.projectFile : "(no project file)");
    return true;
}

void EditorQtHost::Update(double dt)
{
    updates_.fetch_add(1);
    lastDt_.store(dt);
    DrainCommands();
    if (HasScene())
    {
        uint32_t state = 0;
        if (scene_.getState(scene_.context, &state) == EngineSceneOk) playState_.store(static_cast<EditorState>(state));
    }
}

void EditorQtHost::Stop()
{
    if (!uiThread_.joinable()) return;
    if (viewportActive_.exchange(false) && host_.viewportDetach)
        host_.viewportDetach(host_.user); // поток движка, до уничтожения виджетов
    if (uiReady_.load())
        // quit() asks windows to close and can be vetoed by our closeEvent guard.
        // GPU resources are gone now: exit the UI loop directly, on its own thread.
        QMetaObject::invokeMethod(QCoreApplication::instance(), [] { QCoreApplication::exit(0); },
                                  Qt::QueuedConnection);
    uiThread_.join();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "EditorUpdates=%llu", static_cast<unsigned long long>(updates_.load()));
    Log(buf);
    Log("EditorStop");
}

void EditorQtHost::UiThreadMain()
{
    QApplication app(g_argc, g_argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("Engine Editor"));
    EditorTheme::Apply(app);

    EditorMainWindow window(this);
    {
        std::lock_guard lk(windowMutex_);
        window_ = &window;
    }
    window.show();
    uiReady_.store(true);

    app.exec();

    {
        std::lock_guard lk(windowMutex_);
        window_ = nullptr;
    }
}

void EditorQtHost::Log(const char* text)
{
    {
        std::lock_guard lk(logMutex_);
        if (host_.log) host_.log(host_.user, text);
    }

    // Keep the window alive until invokeMethod has queued the call.
    std::lock_guard windowLock(windowMutex_);
    if (EditorMainWindow* w = window_)
    {
        const QString line = QString::fromUtf8(text);
        // типобезопасная форма: аргументы передаются как есть, без Q_ARG
        QMetaObject::invokeMethod(w, &EditorMainWindow::AppendLine, Qt::QueuedConnection, line);
    }
}

void EditorQtHost::PostCommand(const EditorCommand& cmd)
{
    std::lock_guard lk(commandMutex_);
    commands_.push_back(cmd);
}

void EditorQtHost::DrainCommands()
{
    std::deque<EditorCommand> pending;
    {
        std::lock_guard lk(commandMutex_);
        pending.swap(commands_);
    }

    int lastResize = -1;
    for (int i = static_cast<int>(pending.size()) - 1; i >= 0; --i)
        if (pending[i].type == EditorCommand::ViewportResize)
        {
            lastResize = i;
            break;
        }


    for (size_t i = 0; i < pending.size(); ++i)
    {
        const EditorCommand& cmd = pending[i];
        if (cmd.type == EditorCommand::ViewportResize && static_cast<int>(i) != lastResize)
            continue;
        switch (cmd.type)
        {
        case EditorCommand::InputFocus:
            if (viewportInput_.focus) viewportInput_.focus(viewportInput_.context,cmd.w);
            break;
        case EditorCommand::InputKey:
            if (viewportInput_.key) viewportInput_.key(viewportInput_.context,cmd.w,cmd.h);
            break;
        case EditorCommand::InputPointer:
            if (viewportInput_.pointer) viewportInput_.pointer(viewportInput_.context,cmd.yaw,cmd.pitch,cmd.zoom);
            break;
        case EditorCommand::TogglePlay:
        case EditorCommand::TogglePause:
            if (HasScene() && viewportActive_.load())
            {
                const auto state = PlayState();
                const uint32_t action = cmd.type == EditorCommand::TogglePlay ? (state == Edit ? EnginePlay : EngineStop) :
                    (state == Play ? EnginePause : EngineResume);
                const auto result = scene_.control(scene_.context, action);
                uint32_t next = 0; scene_.getState(scene_.context, &next); playState_.store(static_cast<EditorState>(next));
                PublishScene(); Complete(cmd, result);
            }
            else if (viewportActive_.load() && host_.viewportControl)
            {
                const int state = host_.viewportControl(host_.user, cmd.type == EditorCommand::TogglePlay ? 0 : 1);
                if (state >= 0) playState_.store(static_cast<EditorState>(state));
                else Log("Play control failed; see engine log");
            }
            break;
        case EditorCommand::ReloadProject:
        {
            if (!HasScene() || PlayState() != Edit) { Complete(cmd, EngineSceneReadOnly); break; }
            const auto* reload = host_.getService ? static_cast<const EngineEditorReloadApi*>(host_.getService(host_.user, EngineEditorReloadService, 1)) : nullptr;
            if (!reload || !reload->reload) { Complete(cmd, EngineSceneReadOnly, QStringLiteral("В этой версии runtime нет hot reload. Откройте проект заново.")); break; }
            QString checkpoint;
            try {
                const auto selectedName = GetSelectedObjectName();
                checkpoint = Checkpoint(); char error[2048]{}; EngineEditorReloadBindings bindings{};
                const auto result = reload->reload(reload->context, cmd.path.toUtf8().constData(), checkpoint.toUtf8().constData(),
                    viewportHwnd_, viewportWidth_, viewportHeight_, &bindings, error, sizeof(error));
                if (result == EngineSceneOk && bindings.scene.context == scene_.context) {
                    QFile::remove(checkpoint); auto unchanged=cmd; unchanged.name="unchanged"; Complete(unchanged,result); break;
                }
                if (result == EngineSceneOk) {
                    SetModuleApi(bindings.scene); SetProjectDataApi(bindings.data); SetSelectionApi(bindings.selection); SetViewportInputApi(bindings.input);
                    // Old undo files can reference types removed by the new DLL. Keep the live edit snapshot, reset only history.
                    for (const auto& path : undo_) QFile::remove(path);
                    for (const auto& path : redo_) QFile::remove(path);
                    undo_.clear(); redo_.clear(); lastTransformId_ = 0;
                    Snapshot restored; EngineSceneInfo info{};
                    scene_.readActiveScene(scene_.context, &info, SceneVisitor, &restored); selectedId_ = 0;
                    for (const auto& o : restored.objects) if (o.name == selectedName) selectedId_ = o.id;
                    PublishScene();
                }
                QFile::remove(checkpoint); Complete(cmd, result, QString::fromUtf8(error));
            } catch (const std::exception& e) { QFile::remove(checkpoint); Complete(cmd, EngineSceneFailure, QString::fromUtf8(e.what())); }
            break;
        }
        case EditorCommand::RequestExit:
            Log("EditorRequestExit");
            if (host_.requestExit) host_.requestExit(host_.user);
            break;

        case EditorCommand::ImportModel:
        case EditorCommand::ImportTexture:
            {
                if (!HasScene()) { Complete(cmd, EngineSceneReadOnly); break; }
                char path[4096]{}, error[1024]{};
                const auto result = scene_.importAsset(scene_.context, cmd.path.toUtf8().constData(),
                    cmd.type == EditorCommand::ImportTexture ? 1 : 0, path, sizeof(path), error, sizeof(error));
                auto completed = cmd;
                if (result == EngineSceneOk) completed.path = QString::fromUtf8(path);
                Complete(completed, result, QString::fromUtf8(error));
                break;
            }
        case EditorCommand::ViewportAttach:
            viewportHwnd_ = cmd.hwnd; viewportWidth_ = cmd.w; viewportHeight_ = cmd.h;
            if (HasScene())
            {
                const auto result = sceneViewportAttached_ ? scene_.resizeViewport(scene_.context, cmd.w, cmd.h) :
                    scene_.attachViewport(scene_.context, cmd.hwnd, cmd.w, cmd.h);
                if (result != EngineSceneOk) throw std::runtime_error("Cannot initialize scene viewport");
                sceneViewportAttached_ = true; viewportActive_.store(true);
                Log("EditorSceneViewportAttach ok");
            }
            else if (host_.viewportAttach && host_.viewportAttach(host_.user, cmd.hwnd, cmd.w, cmd.h))
            {
                viewportActive_.store(true);
                playState_.store(Edit);
                Log("EditorViewportAttach ok");
            }
            else
            {
                Log("EditorViewportAttach failed");
                throw std::runtime_error("Cannot initialize editor viewport");
            }
            break;
        case EditorCommand::ViewportResize:
            viewportWidth_ = cmd.w; viewportHeight_ = cmd.h;
            if (HasScene() && viewportActive_.load()) scene_.resizeViewport(scene_.context, cmd.w, cmd.h);
            else if (viewportActive_.load() && host_.viewportResize)
                host_.viewportResize(host_.user, cmd.w, cmd.h);
            break;
        case EditorCommand::ViewportCamera:
            if (PlayState() != Edit && HasScene()) break;
            if (HasScene() && viewportActive_.load()) scene_.camera(scene_.context, cmd.right, cmd.up, cmd.forward, cmd.yaw, cmd.pitch, cmd.zoom);
            else if (viewportActive_.load() && host_.viewportCamera)
                host_.viewportCamera(host_.user, cmd.right, cmd.up, cmd.forward, cmd.yaw, cmd.pitch, cmd.zoom);
            break;
        case EditorCommand::ViewportDetach:
            viewportHwnd_ = nullptr;
            if (HasScene() && sceneViewportAttached_) scene_.resizeViewport(scene_.context, 0, 0);
            else playState_.store(Edit);
            if (viewportActive_.exchange(false) && host_.viewportDetach)
                host_.viewportDetach(host_.user);
            Log("EditorViewportDetach");
            break;
        case EditorCommand::PickObject:
            if (HasScene() && PlayState() == Edit)
            {
                uint64_t id = 0;
                if (scene_.pickObject(scene_.context, cmd.right, cmd.up, &id) == EngineSceneOk) selectedId_ = id;
                else selectedId_ = 0;
                PublishScene();
            }
            break;
        case EditorCommand::FocusObject:
            if (HasScene() && selectedId_) Complete(cmd, scene_.focusObject(scene_.context, selectedId_));
            break;
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
        case EditorCommand::SetProjectData:
        case EditorCommand::ConfigurePlay:
        case EditorCommand::RemoveObject:
        case EditorCommand::RenameObject:
        case EditorCommand::DuplicateObject:
        case EditorCommand::SaveScene:
        case EditorCommand::LoadScene:
        case EditorCommand::NewScene:
        case EditorCommand::Undo:
        case EditorCommand::Redo:
        {
            if (!HasScene()) { Complete(cmd, EngineSceneReadOnly, QStringLiteral("Этот проект поддерживает только preview.")); break; }
            auto& api = scene_.scene;
            uint32_t result = EngineSceneOk;
            char error[1024]{};
            QString checkpoint;
            const bool mutation = cmd.type == EditorCommand::UpdateObjectTransform || cmd.type == EditorCommand::CreateObject ||
                cmd.type == EditorCommand::RemoveObject || cmd.type == EditorCommand::RenameObject || cmd.type == EditorCommand::DuplicateObject ||
                cmd.type == EditorCommand::SetModel || cmd.type == EditorCommand::CreateModel || cmd.type == EditorCommand::SetTexture || cmd.type == EditorCommand::SetMaterial || cmd.type == EditorCommand::SetPointLight || cmd.type == EditorCommand::SetProjectData || cmd.type == EditorCommand::ConfigurePlay;
            if ((mutation || cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene ||
                cmd.type == EditorCommand::Undo || cmd.type == EditorCommand::Redo) && PlayState() != Edit)
            { Complete(cmd, EngineSceneReadOnly, QStringLiteral("Остановите Play перед редактированием.")); break; }
            try
            {
                const auto now = std::chrono::steady_clock::now();
                const bool isNudge =
                    cmd.type == EditorCommand::UpdateObjectTransform &&
                    cmd.path == QStringLiteral("nudge");

                const bool isGizmo =
                    cmd.type == EditorCommand::UpdateObjectTransform &&
                    cmd.path == QStringLiteral("gizmo");

                const bool gizmoSession =
                    isGizmo &&
                    gizmoDragActive_ &&
                    cmd.objectId == gizmoDragObject_;

                const bool coalesced =
                    cmd.type == EditorCommand::UpdateObjectTransform &&
                    (
                        (isNudge &&
                            lastTransformId_ == cmd.objectId &&
                            now - lastTransformTime_ < std::chrono::milliseconds(350))
                        ||
                        (gizmoSession && gizmoDragCheckpointed_)
                        ||
                        (isGizmo &&
                            !gizmoDragActive_ &&
                            lastTransformId_ == cmd.objectId &&
                            now - lastTransformTime_ < std::chrono::milliseconds(350))
                    );

                if (mutation && !coalesced) checkpoint = Checkpoint();
                if (mutation) lastTransformId_ = 0;
                if (cmd.type == EditorCommand::SelectObject) selectedId_ = cmd.objectId;
                else if (cmd.type == EditorCommand::FetchScene)
                {
                    auto view = ReadSnapshot();
                    view.objects.erase(std::remove_if(view.objects.begin(),view.objects.end(),[](const auto& o){ return o.gameplayKind == 3 || o.type == "Gameplay.PlaySettings"; }),view.objects.end());
                    auto found = std::find_if(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == selectedId_; });
                    selectedId_ = view.objects.empty() ? 0 : (found == view.objects.end() || ++found == view.objects.end() ? view.objects.front().id : found->id);
                }
                else if (cmd.type == EditorCommand::UpdateObjectTransform)
                {
                    auto transform = cmd.transform;
                    if (cmd.path == QStringLiteral("nudge"))
                        for (const auto& o : ReadSnapshot().objects) if (o.id == cmd.objectId)
                        { transform = o.transform; transform.position[0] += cmd.right; transform.position[1] += cmd.up; transform.position[2] += cmd.forward; }
                    result = api.setTransform(api.context, cmd.objectId, &transform);
                    if (result == EngineSceneOk && (isNudge || isGizmo))
                    {
                        lastTransformId_ = cmd.objectId;
                        lastTransformTime_ = now;

                        if (isGizmo && gizmoDragActive_ && cmd.objectId == gizmoDragObject_)
                            gizmoDragCheckpointed_ = true;
                    }
                }
                else if (cmd.type == EditorCommand::RemoveObject)
                    result = api.removeObject(api.context, cmd.objectId);
                else if (cmd.type == EditorCommand::RenameObject)
                    result = api.renameObject(api.context, cmd.objectId, cmd.name.toUtf8().constData());
                else if (cmd.type == EditorCommand::SetModel)
                {
                    const auto selectedName = GetSelectedObjectName();
                    QFile input(checkpoint);
                    if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read model change checkpoint");
                    auto document = QJsonDocument::fromJson(input.readAll()).object();
                    auto objects = document["objects"].toArray();
                    bool found = false;
                    for (int i = 0; i < objects.size(); ++i) {
                        auto object = objects[i].toObject();
                        if (object["id"].toInteger() != static_cast<qint64>(cmd.objectId)) continue;
                        const auto type = object["type"].toString();
                        if (type != "Engine.Model" && !type.startsWith("Primitive.")) throw std::runtime_error("Only model objects and primitives support model replacement");
                        object["type"] = "Engine.Model";
                        auto assets = object["assets"].toObject(); assets["model"] = cmd.path;
                        if (!assets.contains("texture")) assets["texture"] = "";
                        object["assets"] = assets; objects[i] = object; found = true;
                    }
                    if (!found) throw std::runtime_error("Object no longer exists");
                    document["objects"] = objects;
                    QTemporaryFile candidate(historyDirectory_->filePath("model-XXXXXX.scene.json"));
                    const auto bytes = QJsonDocument(document).toJson();
                    if (!candidate.open() || candidate.write(bytes) != bytes.size() || !candidate.flush()) throw std::runtime_error("Cannot write model change candidate");
                    candidate.close(); uint64_t id = 0;
                    result = scene_.loadScene(scene_.context, candidate.fileName().toUtf8().constData(), &id, error, sizeof(error));
                    if (result == EngineSceneOk) {
                        Snapshot restored; EngineSceneInfo info{}; scene_.readActiveScene(scene_.context, &info, SceneVisitor, &restored);
                        selectedId_ = 0; for (const auto& o : restored.objects) if (o.name == selectedName) selectedId_ = o.id;
                    }
                }
                else if (cmd.type == EditorCommand::SetTexture)
                    result = scene_.setTexture(scene_.context, cmd.objectId, cmd.path.toUtf8().constData(), error, sizeof(error));
                else if (cmd.type == EditorCommand::SetMaterial)
                    result = scene_.setMaterial ? scene_.setMaterial(scene_.context, cmd.objectId, cmd.path.toUtf8().constData(), error, sizeof(error)) : EngineSceneReadOnly;
                else if (cmd.type == EditorCommand::SetPointLight)
                    result = api.setPointLight ? api.setPointLight(api.context, cmd.objectId, &cmd.pointLight) : EngineSceneReadOnly;
                else if (cmd.type == EditorCommand::SetProjectData)
                    result = projectData_.write ? projectData_.write(projectData_.context, cmd.objectId, cmd.path.toUtf8().constData()) : EngineSceneReadOnly;
                else if (cmd.type == EditorCommand::ConfigurePlay)
                {
                    uint64_t id=cmd.objectId; bool created=false;
                    if(!projectData_.write)result=EngineSceneReadOnly;
                    else {
                        if(!id){result=api.createObject(api.context,"Gameplay.PlaySettings","Play Settings",&cmd.transform,&id);created=result==EngineSceneOk;}
                        if(result==EngineSceneOk)result=projectData_.write(projectData_.context,id,cmd.path.toUtf8().constData());
                        if(result!=EngineSceneOk&&created)api.removeObject(api.context,id);
                    }
                }
                else if (cmd.type == EditorCommand::CreateObject || cmd.type == EditorCommand::DuplicateObject || cmd.type == EditorCommand::CreateModel)
                {
                    auto view = ReadSnapshot();
                    QString type = cmd.path, base = cmd.name;
                    QString model = cmd.type == EditorCommand::CreateModel ? cmd.path : QString(), texture, material, projectData;
                    EngineSceneTransform transform = cmd.transform;
                    uint64_t parent = 0; bool inherit = true;
                    bool hasLight = false; EngineScenePointLight light{};
                    if (cmd.type == EditorCommand::DuplicateObject)
                    {
                        auto found = std::find_if(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == cmd.objectId; });
                        if (found == view.objects.end()) throw std::runtime_error("Object no longer exists");
                        type = QString::fromStdString(found->type); base = QString::fromStdString(found->name);
                        transform = found->transform; transform.position[0] += 1;
                        parent = found->parent; inherit = found->inheritScale;
                        model = QString::fromStdString(found->modelAsset); texture = QString::fromStdString(found->textureAsset);
                        material = QString::fromStdString(found->materialAsset);
                        hasLight = found->hasPointLight; light = found->pointLight;
                        projectData = QString::fromStdString(found->projectData);
                    }
                    QString name = base; int suffix = 1;
                    while (std::any_of(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.name == name.toStdString(); }))
                        name = base + QStringLiteral(" %1").arg(++suffix);
                    uint64_t id = 0;
                    result = model.isEmpty() ? api.createObject(api.context, type.toUtf8().constData(), name.toUtf8().constData(), &transform, &id) :
                        scene_.createModel(scene_.context, model.toUtf8().constData(), name.toUtf8().constData(), &transform, &id, error, sizeof(error));
                    if (result == EngineSceneOk)
                    {
                        if (parent) result = scene_.setParent(scene_.context, id, parent, inherit);
                        if (result == EngineSceneOk && !texture.isEmpty())
                            result = scene_.setTexture(scene_.context, id, texture.toUtf8().constData(), error, sizeof(error));
                        if (result == EngineSceneOk && !material.isEmpty())
                            result = scene_.setMaterial ? scene_.setMaterial(scene_.context, id, material.toUtf8().constData(), error, sizeof(error)) : EngineSceneReadOnly;
                        if (result == EngineSceneOk && hasLight) result = api.setPointLight ? api.setPointLight(api.context, id, &light) : EngineSceneReadOnly;
                        if (result == EngineSceneOk && !projectData.isEmpty()) result = projectData_.write ? projectData_.write(projectData_.context, id, projectData.toUtf8().constData()) : EngineSceneReadOnly;
                        if (result == EngineSceneOk && !projectData.isEmpty()) result = projectData_.remapDuplicate ? projectData_.remapDuplicate(projectData_.context, id, cmd.objectId) : EngineSceneReadOnly;
                        if (result == EngineSceneOk) selectedId_ = id;
                        else api.removeObject(api.context, id);
                    }
                }
                else if (cmd.type == EditorCommand::SaveScene)
                {
                    lastTransformId_ = 0;
                    result = scene_.saveScene(scene_.context, cmd.path.toUtf8().constData(), error, sizeof(error));
                    if (result == EngineSceneOk) dirty_ = false;
                }
                else if (cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene)
                {
                    lastTransformId_ = 0;
                    QString path = cmd.path;
                    if (cmd.type == EditorCommand::NewScene)
                    {
                        path = historyDirectory_->filePath(QStringLiteral("empty.scene.json"));
                        QFile file(path);
                        if (!file.open(QIODevice::WriteOnly) || file.write("{\"version\":1,\"name\":\"Scene\",\"objects\":[]}") < 0)
                            throw std::runtime_error("Cannot create empty scene");
                        file.close();
                    }
                    uint64_t id = 0;
                    result = scene_.loadScene(scene_.context, path.toUtf8().constData(), &id, error, sizeof(error));
                    if (result == EngineSceneOk)
                    {
                        selectedId_ = 0; dirty_ = cmd.type == EditorCommand::NewScene;
                        for (const auto& file : undo_) QFile::remove(file);
                        for (const auto& file : redo_) QFile::remove(file);
                        undo_.clear(); redo_.clear();
                    }
                }
                else if (cmd.type == EditorCommand::Undo || cmd.type == EditorCommand::Redo)
                {
                    lastTransformId_ = 0;
                    auto& from = cmd.type == EditorCommand::Undo ? undo_ : redo_;
                    auto& to = cmd.type == EditorCommand::Undo ? redo_ : undo_;
                    if (!from.empty())
                    {
                        const auto current = Checkpoint(); uint64_t id = 0;
                        result = scene_.loadScene(scene_.context, from.back().toUtf8().constData(), &id, error, sizeof(error));
                        if (result == EngineSceneOk)
                        { to.push_back(current); QFile::remove(from.back()); from.pop_back(); selectedId_ = 0; dirty_ = true; }
                        else QFile::remove(current);
                    }
                }
                if (mutation)
                {
                    if (result == EngineSceneOk) { RecordChange(checkpoint); dirty_ = true; }
                    else QFile::remove(checkpoint);
                }
            }
            catch (const std::exception& e)
            { QFile::remove(checkpoint); result = EngineSceneFailure; std::snprintf(error, sizeof(error), "%s", e.what()); }
            PublishScene();
            if (cmd.type != EditorCommand::RefreshScene || !cmd.name.isEmpty()) Complete(cmd, result, QString::fromUtf8(error));
            break;
        }
        case EditorCommand::RunTask:
            if (HasScene() && cmd.task) cmd.task(scene_);
            break;
        case EditorCommand::SetGizmoMode:
            gizmoMode_.store(cmd.gizmo);
            lastTransformId_ = 0;
            break;

        case EditorCommand::BeginGizmoDrag:
            if (PlayState() == Edit)
            {
                gizmoDragActive_ = true;
                gizmoDragCheckpointed_ = false;
                gizmoDragObject_ = cmd.objectId;
                lastTransformId_ = 0;
            }
            break;

        case EditorCommand::EndGizmoDrag:
            gizmoDragActive_ = false;
            gizmoDragCheckpointed_ = false;
            gizmoDragObject_ = 0;
            lastTransformId_ = 0;
            break;
        default:
            break;
        }
    }
}

void EditorQtHost::FetchSceneAndSelectNext()
{
    EditorCommand c;
    c.type = EditorCommand::FetchScene;
    PostCommand(c);
}

void EditorQtHost::MoveSelectedObject(float right, float up, float forward)
{
    if (PlayState() != Edit) return;
    const auto view = ReadSnapshot();
    for (const auto& obj : view.objects) if (obj.id == view.selected)
    {
        EditorCommand c; c.type = EditorCommand::UpdateObjectTransform; c.objectId = obj.id; c.transform = obj.transform;
        c.path = QStringLiteral("nudge"); c.right = right; c.up = up; c.forward = forward;
        PostCommand(c); break;
    }
}
std::string EditorQtHost::GetSelectedObjectName()
{
    const auto view = ReadSnapshot();
    for (const auto& obj : view.objects) if (obj.id == view.selected) return obj.name;
    return {};
}
EditorQtHost::Snapshot EditorQtHost::ReadSnapshot()
{
    std::lock_guard lock(sceneMutex_); return snapshot_;
}
void ENGINE_MODULE_CALL EditorQtHost::SceneVisitor(void* context, const EngineSceneObjectInfo* info)
{
    auto& view = *static_cast<Snapshot*>(context);
    view.objects.push_back({info->id, info->parentId, info->inheritParentScale != 0, info->name, info->typeName, info->transform,
        info->modelAsset ? info->modelAsset : "", info->textureAsset ? info->textureAsset : "",
        info->hasPointLight != 0, info->pointLight, info->materialAsset ? info->materialAsset : ""});
}
void EditorQtHost::PublishScene()
{
    Snapshot view;
    EngineSceneInfo info{};
    if (scene_.readActiveScene(scene_.context, &info, SceneVisitor, &view) != EngineSceneOk) return;
    scene_.scene.listTypes(scene_.scene.context, [](void* p, const EngineSceneTypeInfo* t) {
        static_cast<Snapshot*>(p)->types.push_back({t->typeName, t->displayName});
    }, &view);
    if (projectData_.read) for (auto& object : view.objects)
        projectData_.read(projectData_.context, object.id, [](void* p, const char* value) { *static_cast<std::string*>(p) = value; }, &object.projectData);
    if (viewportInput_.objectKind) for (auto& object : view.objects)
        viewportInput_.objectKind(viewportInput_.context,object.id,&object.gameplayKind);
    if (std::none_of(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == selectedId_; })) selectedId_ = 0;
    view.selected = selectedId_; view.dirty = dirty_; view.canUndo = !undo_.empty(); view.canRedo = !redo_.empty();
    hasSelection_.store(selectedId_ != 0);
    if (selection_.select) selection_.select(selection_.context, PlayState() == Edit ? selectedId_ : 0);
    std::lock_guard lock(sceneMutex_); view.serial = snapshot_.serial + 1; snapshot_ = std::move(view);
}
QString EditorQtHost::Checkpoint()
{
    if (!historyDirectory_ || !historyDirectory_->isValid()) throw std::runtime_error("Undo history directory is unavailable");
    auto path = historyDirectory_->filePath(QString::number(++historySerial_) + QStringLiteral(".scene.json"));
    char error[1024]{};
    if (scene_.saveScene(scene_.context, path.toUtf8().constData(), error, sizeof(error)) != EngineSceneOk)
        throw std::runtime_error(error);
    return path;
}
void EditorQtHost::RecordChange(const QString& checkpoint)
{
    for (const auto& file : redo_) QFile::remove(file);
    redo_.clear();
    if (checkpoint.isEmpty()) return;
    undo_.push_back(checkpoint);
    if (undo_.size() > 64) { QFile::remove(undo_.front()); undo_.erase(undo_.begin()); }
}
void EditorQtHost::Complete(const EditorCommand& command, uint32_t result, const QString& error)
{
    std::lock_guard lock(windowMutex_);
    if (auto* window = window_)
        QMetaObject::invokeMethod(window, [window, command, result, error] { window->CommandFinished(command, result, error); }, Qt::QueuedConnection);
}

void EditorQtHost::RunOnUi(std::function<void()> action)
{
    std::lock_guard lock(windowMutex_);
    if (auto* window = window_)
        QMetaObject::invokeMethod(window, std::move(action), Qt::QueuedConnection);
}

void EditorQtHost::SetGizmoMode(EditorGizmoMode mode)
{
    EditorCommand c;
    c.type = EditorCommand::SetGizmoMode;
    c.gizmo = mode;
    PostCommand(c);
}
