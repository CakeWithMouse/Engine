#include "ViewportCommandHandler.h"
#include <stdexcept>

ViewportCommandHandler::ViewportCommandHandler() = default;

bool ViewportCommandHandler::CanHandle(EditorCommand::Type type) const
{
    switch (type)
    {
    case EditorCommand::InputFocus:
    case EditorCommand::InputKey:
    case EditorCommand::InputPointer:
    case EditorCommand::TogglePlay:
    case EditorCommand::TogglePause:
    case EditorCommand::ViewportAttach:
    case EditorCommand::ViewportResize:
    case EditorCommand::ViewportCamera:
    case EditorCommand::ViewportDetach:
    case EditorCommand::PickObject:
    case EditorCommand::FocusObject:
        return true;
    default:
        return false;
    }
}

void ViewportCommandHandler::Handle(const EditorCommand& cmd,
                                    Context& ctx,
                                    std::atomic<bool>& viewportActive,
                                    std::atomic<IEditorHost::EditorState>& playState,
                                    uint64_t& selectedId,
                                    uint32_t& result,
                                    QString& error)
{
    (void)error;
    const bool hasScene = (ctx.scene.context != nullptr);

    switch (cmd.type)
    {
    case EditorCommand::InputFocus:
        if (ctx.viewportInput.focus)
            ctx.viewportInput.focus(ctx.viewportInput.context, cmd.w);
        break;

    case EditorCommand::InputKey:
        if (ctx.viewportInput.key)
            ctx.viewportInput.key(ctx.viewportInput.context, cmd.w, cmd.h);
        break;

    case EditorCommand::InputPointer:
        if (ctx.viewportInput.pointer)
            ctx.viewportInput.pointer(ctx.viewportInput.context, cmd.yaw, cmd.pitch, cmd.zoom);
        break;

    case EditorCommand::TogglePlay:
    case EditorCommand::TogglePause:
        if (hasScene && viewportActive.load())
        {
            const auto state = playState.load();
            const uint32_t action = cmd.type == EditorCommand::TogglePlay
                ? (state == IEditorHost::Edit ? EnginePlay : EngineStop)
                : (state == IEditorHost::Play ? EnginePause : EngineResume);
            result = ctx.scene.control(ctx.scene.context, action);
            uint32_t next = 0;
            ctx.scene.getState(ctx.scene.context, &next);
            playState.store(static_cast<IEditorHost::EditorState>(next));
            if (ctx.publishScene) ctx.publishScene();
        }
        else if (viewportActive.load() && ctx.host.viewportControl)
        {
            const int state = ctx.host.viewportControl(ctx.host.user, cmd.type == EditorCommand::TogglePlay ? 0 : 1);
            if (state >= 0) playState.store(static_cast<IEditorHost::EditorState>(state));
            else if (ctx.log) ctx.log("Play control failed; see engine log");
        }
        break;

    case EditorCommand::ViewportAttach:
        viewportHwnd_ = cmd.hwnd;
        viewportWidth_ = cmd.w;
        viewportHeight_ = cmd.h;
        if (hasScene)
        {
            const auto attachRes = sceneViewportAttached_
                ? ctx.scene.resizeViewport(ctx.scene.context, cmd.w, cmd.h)
                : ctx.scene.attachViewport(ctx.scene.context, cmd.hwnd, cmd.w, cmd.h);
            if (attachRes != EngineSceneOk)
                throw std::runtime_error("Cannot initialize scene viewport");
            sceneViewportAttached_ = true;
            viewportActive.store(true);
            if (ctx.log) ctx.log("EditorSceneViewportAttach ok");
        }
        else if (ctx.host.viewportAttach && ctx.host.viewportAttach(ctx.host.user, cmd.hwnd, cmd.w, cmd.h))
        {
            viewportActive.store(true);
            playState.store(IEditorHost::Edit);
            if (ctx.log) ctx.log("EditorViewportAttach ok");
        }
        else
        {
            if (ctx.log) ctx.log("EditorViewportAttach failed");
            throw std::runtime_error("Cannot initialize editor viewport");
        }
        break;

    case EditorCommand::ViewportResize:
        viewportWidth_ = cmd.w;
        viewportHeight_ = cmd.h;
        if (hasScene && viewportActive.load())
            ctx.scene.resizeViewport(ctx.scene.context, cmd.w, cmd.h);
        else if (viewportActive.load() && ctx.host.viewportResize)
            ctx.host.viewportResize(ctx.host.user, cmd.w, cmd.h);
        break;

    case EditorCommand::ViewportCamera:
        if (playState.load() != IEditorHost::Edit && hasScene)
            break;
        if (hasScene && viewportActive.load())
            ctx.scene.camera(ctx.scene.context, cmd.right, cmd.up, cmd.forward, cmd.yaw, cmd.pitch, cmd.zoom);
        else if (viewportActive.load() && ctx.host.viewportCamera)
            ctx.host.viewportCamera(ctx.host.user, cmd.right, cmd.up, cmd.forward, cmd.yaw, cmd.pitch, cmd.zoom);
        break;

    case EditorCommand::ViewportDetach:
        viewportHwnd_ = nullptr;
        if (hasScene && sceneViewportAttached_)
            ctx.scene.resizeViewport(ctx.scene.context, 0, 0);
        else
            playState.store(IEditorHost::Edit);
        if (viewportActive.exchange(false) && ctx.host.viewportDetach)
            ctx.host.viewportDetach(ctx.host.user);
        if (ctx.log) ctx.log("EditorViewportDetach");
        break;

    case EditorCommand::PickObject:
        if (hasScene && playState.load() == IEditorHost::Edit)
        {
            uint64_t id = 0;
            if (ctx.scene.pickObject(ctx.scene.context, cmd.right, cmd.up, &id) == EngineSceneOk)
                selectedId = id;
            else
                selectedId = 0;
            if (ctx.publishScene) ctx.publishScene();
        }
        break;

    case EditorCommand::FocusObject:
        if (hasScene && selectedId)
            result = ctx.scene.focusObject(ctx.scene.context, selectedId);
        break;

    default:
        break;
    }
}
