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
        SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
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
    if (overlay_.version == 0)
    {
        std::printf("[Info] Viewport overlay service: NOT FOUND (version 0)\n");
        std::fflush(stdout);
    }
    return true;
}

void EditorQtHost::Update(double dt)
{
    updates_.fetch_add(1);
    lastDt_.store(dt);
    DrainCommands();
    if (std::chrono::steady_clock::now() - recoveryTime_ >= std::chrono::seconds(30)) {
        recoveryTime_ = std::chrono::steady_clock::now(); WriteRecovery();
    }

    if (host_.getService) {
        const auto* current = static_cast<const EngineViewportOverlayApi*>(host_.getService(host_.user, EngineViewportOverlayService, 1));
        if (current && (current->context != overlay_.context || current->getCamera != overlay_.getCamera)) SetViewportOverlayApi(*current);
        else if (!current && overlay_.context) SetViewportOverlayApi({});
    }
    if (overlay_.getCamera)
    {
        EngineCameraInfo cam{};
        if (overlay_.getCamera(overlay_.context, &cam) == EngineSceneOk)
        {
            std::lock_guard lk(cameraMutex_);
            lastCamera_ = cam;
            lastCameraValid_ = (cam.width > 0 && cam.height > 0);
        }
        else
        {
            std::lock_guard lk(cameraMutex_);
            lastCameraValid_ = false;
        }
    }

    {
        static int sLastCameraState = -1;
        int curCameraState = (lastCameraValid_ ? 1 : 0);
        if (curCameraState != sLastCameraState)
        {
            sLastCameraState = curCameraState;
            std::printf("[Info] Camera: %s\n", (curCameraState == 1) ? "ok" : "zero");
            std::fflush(stdout);
        }
    }

    if (overlay_.setOverlayLines || overlay_.setOverlayBillboards || overlay_.setOverlayIcon || (overlay_.version >= 3 && overlay_.setOverlayTriangles))
    {
        std::vector<PendingIcon> iconsToUpload;
        std::vector<EngineOverlayLine> linesToSend;
        std::vector<EngineOverlayBillboard> billboardsToSend;
        std::vector<EngineOverlayTriangle> trianglesToSend;
        float thicknessToSend = 1.0f;
        bool sendLines = false;
        bool sendBillboards = false;
        bool sendTriangles = false;

        {
            std::lock_guard lk(overlayMutex_);
            if (iconsDirty_ && lastCameraValid_)
            {
                iconsToUpload = pendingIcons_;
                iconsDirty_ = false;
            }
            if (pendingLinesDirty_)
            {
                linesToSend = pendingLines_;
                thicknessToSend = pendingThickness_;
                pendingLinesDirty_ = false;
                sendLines = true;
            }
            if (pendingBillboardsDirty_)
            {
                billboardsToSend = pendingBillboards_;
                pendingBillboardsDirty_ = false;
                sendBillboards = true;
            }
            if (pendingTrianglesDirty_)
            {
                trianglesToSend = pendingTriangles_;
                pendingTrianglesDirty_ = false;
                sendTriangles = true;
            }
        }

        if (overlay_.setOverlayIcon)
        {
            for (const auto& icon : iconsToUpload)
            {
                if (overlay_.setOverlayIcon(overlay_.context, icon.id, icon.width, icon.height, icon.pixels.data()) != EngineSceneOk) {
                    std::lock_guard lk(overlayMutex_); iconsDirty_ = true;
                }
            }
        }
        else if (!iconsToUpload.empty())
        {
            std::lock_guard lk(overlayMutex_);
            iconsDirty_ = true;
        }
        if (sendLines && overlay_.setOverlayLines)
        {
            overlay_.setOverlayLines(overlay_.context, linesToSend.data(), static_cast<uint32_t>(linesToSend.size()), thicknessToSend);
        }
        if (sendBillboards && overlay_.setOverlayBillboards)
        {
            overlay_.setOverlayBillboards(overlay_.context, billboardsToSend.data(), static_cast<uint32_t>(billboardsToSend.size()));
        }
        if (sendTriangles && overlay_.version >= 3 && overlay_.setOverlayTriangles)
        {
            overlay_.setOverlayTriangles(overlay_.context, trianglesToSend.data(), static_cast<uint32_t>(trianglesToSend.size()));
        }
    }
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

static std::atomic<uint64_t> s_resizeCount{0};

uint64_t EditorQtHost::ViewportResizeCount() const
{
    return s_resizeCount.load();
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
            recovery_.Clear();
            Log("EditorRequestExit");
            if (host_.requestExit) host_.requestExit(host_.user);
        }
        else if (cmd.type == EditorCommand::WriteRecovery) { WriteRecovery(); }
        else if (cmd.type == EditorCommand::DiscardRecovery) { recovery_.Clear(); }
        else if (cmd.type == EditorCommand::UpdateAsset) {
            const auto* api = AuthoringApi(); char detail[2048]{};
            const auto name = sceneHandler_.GetSelectedObjectName();
            result = playState_.load() == Edit && api && api->asset ? api->asset(api->context, cmd.w, cmd.path.toUtf8().constData(), cmd.name.toUtf8().constData(), detail, sizeof(detail)) : EngineSceneReadOnly;
            if (result == EngineSceneOk) {
                history_.Reset();
                SceneCommandHandler::Context ctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
                sceneHandler_.SetSelectedId(0);
                sceneHandler_.MarkDirty();
                sceneHandler_.PublishScene(ctx, playState_.load());
                for (const auto& o : sceneHandler_.ReadSnapshot().objects) if (o.name == name) sceneHandler_.SetSelectedId(o.id);
                sceneHandler_.PublishScene(ctx, playState_.load());
                recovery_.Clear(); WriteRecovery();
            }
            Complete(cmd, result, QString::fromUtf8(detail));
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
        else if(cmd.type==EditorCommand::InputCursor){
            if(const auto* cursor=CursorApi())result=cursor->set(cursor->context,cmd.right,cmd.up,cmd.w,cmd.h,cmd.forward!=0);
            Complete(cmd,result,error);
        }
        else if (viewportHandler_.CanHandle(cmd.type))
        {
            if (cmd.type == EditorCommand::InputFocus) viewportFocused_ = cmd.w != 0;
            ViewportCommandHandler::Context vctx{
                host_,
                scene_,
                viewportInput_,
                [this](const char* msg) { Log(msg); },
                [this] {
                    SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
                    sceneHandler_.PublishScene(sctx, playState_.load());
                }
            };
            uint64_t sel = sceneHandler_.SelectedId();
            viewportHandler_.Handle(cmd, vctx, viewportActive_, playState_, sel, result, error);
            if (cmd.type == EditorCommand::ViewportResize)
            {
                uint64_t curCount = ++s_resizeCount;
                std::printf("[ResizeDiag] ViewportResize executed: total=%llu size=%ux%u\n", curCount, cmd.w, cmd.h);
                std::fflush(stdout);
            }
            if (cmd.type == EditorCommand::ViewportAttach || cmd.type == EditorCommand::ViewportResize)
            {
                if (overlay_.setAntialiasing && overlay_.context)
                    overlay_.setAntialiasing(overlay_.context, antialiasingMode_);
            }
            if (cmd.type == EditorCommand::PickObject) {
                sceneHandler_.PickSelection(sel, cmd.name == "toggle" && BatchApi());
                SceneCommandHandler::Context ctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
                sceneHandler_.PublishScene(ctx, playState_.load());
            }
            if (cmd.type == EditorCommand::TogglePlay && result == EngineSceneOk) InvalidateOverlays();
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
                audio_,
                [this](const char* msg) { Log(msg); },
                [this](const auto& s, const auto& d, const auto& sel, const auto& in, const auto& audio) {
                    recovery_.RefreshBuild();
                    SetModuleApi(s);
                    SetProjectDataApi(d);
                    SetSelectionApi(sel);
                    SetViewportInputApi(in);
                    SetAudioPreviewApi(audio);
                },
                [this] { return sceneHandler_.GetSelectedObjectName(); },
                [this](uint64_t id) { sceneHandler_.SetSelectedId(id); },
                [this] {
                    SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
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
            SceneCommandHandler::Context sctx{scene_, projectData_, selection_, viewportInput_, BatchApi(), AuthoringApi(), PointLightV2Api(), ScriptApi()};
            sceneHandler_.Handle(cmd, sctx, history_, playState_.load(), result, error);
            if (result == EngineSceneOk) {
                if (cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene || cmd.type == EditorCommand::Undo || cmd.type == EditorCommand::Redo) InvalidateOverlays();
                if (cmd.type == EditorCommand::SaveScene) { recovery_.scenePath = cmd.path; recovery_.Clear(); }
                if (cmd.type == EditorCommand::LoadScene || cmd.type == EditorCommand::NewScene) {
                    recovery_.Clear(); recovery_.scenePath = sceneHandler_.IsDirty() ? QString{} : cmd.path;
                    if (cmd.name == "recovery") { WriteRecovery(); if (!RecoveryStore::Read(recovery_.record + ".json").isEmpty()) recovery_.ForgetSnapshot(cmd.path); }
                }
            }
            if (cmd.type != EditorCommand::RefreshScene || !cmd.name.isEmpty())
                Complete(cmd, result, error);
        }
    }
    canUndo_.store(history_.CanUndo());
    canRedo_.store(history_.CanRedo());
}

IEditorHost::Snapshot EditorQtHost::ReadSnapshot()
{
    auto snapshot = sceneHandler_.ReadSnapshot();
    snapshot.canUndo = canUndo_.load();
    snapshot.canRedo = canRedo_.load();
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
    SetViewportOverlayApi({});
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

bool EditorQtHost::GetCamera(EngineCameraInfo& info) const
{
    std::lock_guard lk(cameraMutex_);
    if (!lastCameraValid_) return false;
    info = lastCamera_;
    return true;
}

void EditorQtHost::SetOverlayLines(const std::vector<EngineOverlayLine>& lines, float thickness)
{
    std::lock_guard lk(overlayMutex_);
    pendingLines_ = lines;
    pendingThickness_ = thickness;
    pendingLinesDirty_ = true;
}

void EditorQtHost::SetOverlayIcon(uint32_t iconId, uint32_t width, uint32_t height, const uint8_t* rgbaPixels)
{
    if (!rgbaPixels || width == 0 || height == 0) return;
    std::lock_guard lk(overlayMutex_);
    PendingIcon icon;
    icon.id = iconId;
    icon.width = width;
    icon.height = height;
    icon.pixels.assign(rgbaPixels, rgbaPixels + (width * height * 4));
    for (auto& existing : pendingIcons_) if (existing.id == iconId) {
        if (existing.width == width && existing.height == height && existing.pixels == icon.pixels) return;
        existing = std::move(icon); iconsDirty_ = true; return;
    }
    pendingIcons_.push_back(std::move(icon)); iconsDirty_ = true;
}

void EditorQtHost::SetOverlayBillboards(const std::vector<EngineOverlayBillboard>& billboards)
{
    std::lock_guard lk(overlayMutex_);
    pendingBillboards_ = billboards;
    pendingBillboardsDirty_ = true;
}

void EditorQtHost::SetOverlayTriangles(const std::vector<EngineOverlayTriangle>& triangles)
{
    std::lock_guard lk(overlayMutex_);
    pendingTriangles_ = triangles;
    pendingTrianglesDirty_ = true;
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

void EditorQtHost::SetViewportOverlayApi(const EngineViewportOverlayApi& api)
{
    overlay_ = api;
    { std::lock_guard lk(cameraMutex_); lastCameraValid_ = false; }
    InvalidateOverlays();
    const bool found = ((overlay_.version >= 1 && overlay_.version <= 3) && overlay_.setOverlayLines && overlay_.getCamera);
    std::printf("[Info] Viewport overlay service: %s (version %u)\n", found ? "FOUND" : "NOT FOUND", overlay_.version);
    std::fflush(stdout);
    if (overlay_.setAntialiasing && overlay_.context)
    {
        overlay_.setAntialiasing(overlay_.context, antialiasingMode_);
    }
}

void EditorQtHost::SetAntialiasing(uint32_t mode)
{
    antialiasingMode_ = mode;
    std::printf("[Info] QtHost::SetAntialiasing: mode = %u, serviceVersion = %u, handler = %p\n",
        mode, overlay_.version, (void*)overlay_.setAntialiasing);
    std::fflush(stdout);
    if (overlay_.setAntialiasing && overlay_.context)
    {
        overlay_.setAntialiasing(overlay_.context, mode);
    }
}

void EditorQtHost::InitializeRecovery() {
    if (host_.projectFile && *host_.projectFile) {
        try { recovery_.Open(QString::fromUtf8(host_.projectFile), QCoreApplication::applicationDirPath()); }
        catch (const std::exception& e) { Log(e.what()); }
    }
}
void EditorQtHost::WriteRecovery()
{
    if (!HasScene() || !sceneHandler_.IsDirty() || PlayState() != Edit || recovery_.record.isEmpty() || !recovery_.lock || !recovery_.lock->isLocked()) return;
    QString path;
    try {
        path = recovery_.NextScene(); char error[2048]{};
        if (scene_.saveScene(scene_.context, path.toUtf8().constData(), error, sizeof(error)) != EngineSceneOk)
            throw std::runtime_error(error);
        recovery_.Commit(path);
        Log("Recovery: unsaved scene checkpoint committed");
    } catch (const std::exception& e) { QFile::remove(path); Log(e.what()); }
}


void EditorQtHost::InvalidateOverlays() {
    std::lock_guard lk(overlayMutex_);
    iconsDirty_ = true; pendingLinesDirty_ = true; pendingBillboardsDirty_ = true;
}
