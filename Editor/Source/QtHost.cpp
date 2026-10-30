#include "QtHost.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <cstdio>
#include <thread>
#include <chrono>

namespace
{
    EditorQtHost::UiLauncher s_defaultUiLauncher = nullptr;
}

void EditorQtHostRegisterDefaultLauncher(EditorQtHost::UiLauncher launcher)
{
    s_defaultUiLauncher = std::move(launcher);
}

EditorQtHost::EditorQtHost(const EngineModuleHost& host, UiLauncher uiLauncher)
    : host_(host), uiLauncher_(uiLauncher ? std::move(uiLauncher) : s_defaultUiLauncher)
{
}

EditorQtHost::~EditorQtHost()
{
    Stop();
}

bool EditorQtHost::Start()
{
    if (uiThread_.joinable()) return false;
    if (!history_.Initialize()) return false;

    if (HasScene() && host_.contentRoot && *host_.contentRoot)
    {
        char error[1024]{};
        if (scene_.setContentRoot(scene_.context, host_.contentRoot, error, sizeof(error)) != EngineSceneOk)
        {
            Log(error);
            return false;
        }
    }

    if (HasScene())
    {
        SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_};
        sceneHandler_.PublishScene(sctx, playState_.load());
    }

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
        if (scene_.getState(scene_.context, &state) == EngineSceneOk)
            playState_.store(static_cast<EditorState>(state));
    }
}

void EditorQtHost::Stop()
{
    if (!uiThread_.joinable()) return;
    if (viewportActive_.exchange(false) && host_.viewportDetach)
        host_.viewportDetach(host_.user);

    if (uiReady_.load())
    {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [] { QCoreApplication::exit(0); },
                                  Qt::QueuedConnection);
    }
    uiThread_.join();

    char buf[96];
    std::snprintf(buf, sizeof(buf), "EditorUpdates=%llu", static_cast<unsigned long long>(updates_.load()));
    Log(buf);
    Log("EditorStop");
}

void EditorQtHost::UiThreadMain()
{
    if (uiLauncher_)
    {
        uiLauncher_(this, &uiReady_);
    }
    else
    {
        uiReady_.store(true);
    }
}

void EditorQtHost::Log(const char* text)
{
    {
        std::lock_guard lk(logMutex_);
        if (host_.log) host_.log(host_.user, text);
    }

    std::lock_guard lk(listenerMutex_);
    if (uiListener_)
    {
        uiListener_->OnLogMessage(QString::fromUtf8(text));
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
    {
        if (pending[i].type == EditorCommand::ViewportResize)
        {
            lastResize = i;
            break;
        }
    }

    for (size_t i = 0; i < pending.size(); ++i)
    {
        const EditorCommand& cmd = pending[i];
        if (cmd.type == EditorCommand::ViewportResize && static_cast<int>(i) != lastResize)
            continue;

        uint32_t result = EngineSceneOk;
        QString error;
        EditorCommand completedCmd = cmd;

        if (cmd.type == EditorCommand::RequestExit)
        {
            Log("EditorRequestExit");
            if (host_.requestExit) host_.requestExit(host_.user);
        }
        else if (cmd.type == EditorCommand::SetGizmoMode)
        {
            gizmoMode_.store(cmd.gizmo);
        }
        else if (cmd.type == EditorCommand::BeginGizmoDrag)
        {
            if (playState_.load() == Edit)
                history_.BeginGizmoDrag(cmd.objectId);
        }
        else if (cmd.type == EditorCommand::EndGizmoDrag)
        {
            history_.EndGizmoDrag();
        }
        else if (viewportHandler_.CanHandle(cmd.type))
        {
            ViewportCommandHandler::Context vctx{
                host_,
                scene_,
                viewportInput_,
                [this](const char* msg) { Log(msg); },
                [this] {
                    SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_};
                    sceneHandler_.PublishScene(sctx, playState_.load());
                }
            };
            uint64_t sel = sceneHandler_.SelectedId();
            viewportHandler_.Handle(cmd, vctx, viewportActive_, playState_, sel, result, error);
            sceneHandler_.SetSelectedId(sel);
            Complete(cmd, result, error);
        }
        else if (assetHandler_.CanHandle(cmd.type))
        {
            AssetCommandHandler::Context actx{
                host_,
                scene_,
                projectData_,
                selection_,
                viewportInput_,
                [this](const char* msg) { Log(msg); },
                [this](const auto& s, const auto& d, const auto& sel, const auto& in) {
                    SetModuleApi(s);
                    SetProjectDataApi(d);
                    SetSelectionApi(sel);
                    SetViewportInputApi(in);
                },
                [this] { return sceneHandler_.GetSelectedObjectName(); },
                [this](uint64_t id) { sceneHandler_.SetSelectedId(id); },
                [this] {
                    SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_};
                    sceneHandler_.PublishScene(sctx, playState_.load());
                }
            };
            assetHandler_.Handle(cmd, actx, history_, playState_.load(),
                viewportHandler_.ViewportHwnd(),
                viewportHandler_.ViewportWidth(),
                viewportHandler_.ViewportHeight(),
                completedCmd, result, error);
            Complete(completedCmd, result, error);
        }
        else if (sceneHandler_.CanHandle(cmd.type))
        {
            SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_};
            sceneHandler_.Handle(cmd, sctx, history_, playState_.load(), result, error);
            if (cmd.type != EditorCommand::RefreshScene || !cmd.name.isEmpty())
                Complete(cmd, result, error);
        }
    }
}

IEditorHost::Snapshot EditorQtHost::ReadSnapshot()
{
    auto snapshot = sceneHandler_.ReadSnapshot();
    snapshot.canUndo = history_.CanUndo();
    snapshot.canRedo = history_.CanRedo();
    return snapshot;
}

void EditorQtHost::FetchSceneAndSelectNext()
{
    sceneHandler_.FetchSceneAndSelectNext([this](const EditorCommand& c) { PostCommand(c); });
}

void EditorQtHost::MoveSelectedObject(float right, float up, float forward)
{
    if (PlayState() != Edit) return;
    sceneHandler_.MoveSelectedObject(right, up, forward, [this](const EditorCommand& c) { PostCommand(c); });
}

std::string EditorQtHost::GetSelectedObjectName()
{
    return sceneHandler_.GetSelectedObjectName();
}

bool EditorQtHost::HasSelection() const
{
    return sceneHandler_.HasSelection();
}

void EditorQtHost::SetGizmoMode(EditorGizmoMode mode)
{
    EditorCommand c;
    c.type = EditorCommand::SetGizmoMode;
    c.gizmo = mode;
    PostCommand(c);
}

void EditorQtHost::SetModuleApi(const EngineEditSessionApi& scene)
{
    scene_ = scene;
    sceneAvailable_.store(scene.context != nullptr);
    materialsAvailable_.store(scene.context && scene.setMaterial && scene.readMaterial && scene.reloadShaderFile && scene.attachMaterialPreview);
}

void EditorQtHost::SetUiListener(IEditorUiListener* listener)
{
    std::lock_guard lk(listenerMutex_);
    uiListener_ = listener;
}

void EditorQtHost::Complete(const EditorCommand& command, uint32_t status, const QString& error)
{
    std::lock_guard lk(listenerMutex_);
    if (uiListener_)
    {
        uiListener_->OnCommandFinished(command, status, error);
    }
}

void EditorQtHost::SetParent(uint64_t childId, uint64_t parentId)
{
    EditorCommand c;
    c.type = EditorCommand::SetParent;
    c.objectId = childId;
    c.parentId = parentId;
    PostCommand(c);
}

void EditorQtHost::RunOnUi(std::function<void()> action)
{
    if (QCoreApplication::instance())
    {
        QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(action), Qt::QueuedConnection);
    }
}
