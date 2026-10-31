#pragma once
#include "IEditorHost.h"
#include <atomic>
#include <functional>

class ViewportCommandHandler
{
public:
    struct Context
    {
        EngineModuleHost host;
        EngineEditSessionApi scene;
        EngineViewportInputApi viewportInput;
        std::function<void(const char*)> log;
        std::function<void()> publishScene;
    };

    ViewportCommandHandler();

    bool CanHandle(EditorCommand::Type type) const;

    void Handle(const EditorCommand& cmd,
                Context& ctx,
                std::atomic<bool>& viewportActive,
                std::atomic<IEditorHost::EditorState>& playState,
                uint64_t& selectedId,
                uint32_t& result,
                QString& error);

    void* ViewportHwnd() const { return viewportHwnd_; }
    uint32_t ViewportWidth() const { return viewportWidth_; }
    uint32_t ViewportHeight() const { return viewportHeight_; }
    bool IsSceneViewportAttached() const { return sceneViewportAttached_; }

private:
    void* viewportHwnd_ = nullptr;
    uint32_t viewportWidth_ = 0;
    uint32_t viewportHeight_ = 0;
    bool sceneViewportAttached_ = false;
};

