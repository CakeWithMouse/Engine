#pragma once
#include "IEditorHost.h"
#include "EditorHistory.h"
#include <functional>

class AssetCommandHandler
{
public:
    struct Context
    {
        EngineModuleHost host;
        EngineEditSessionApi scene;
        EngineProjectDataApi projectData;
        EngineSelectionApi selection;
        EngineViewportInputApi viewportInput;
        std::function<void(const char*)> log;
        std::function<void(const EngineEditSessionApi&, const EngineProjectDataApi&, const EngineSelectionApi&, const EngineViewportInputApi&)> onReloadBindings;
        std::function<std::string()> getSelectedObjectName;
        std::function<void(uint64_t)> setSelectedId;
        std::function<void()> publishScene;
    };

    AssetCommandHandler();

    bool CanHandle(EditorCommand::Type type) const;

    void Handle(const EditorCommand& cmd,
                Context& ctx,
                EditorHistory& history,
                IEditorHost::EditorState playState,
                void* viewportHwnd,
                uint32_t viewportWidth,
                uint32_t viewportHeight,
                EditorCommand& completedCmd,
                uint32_t& result,
                QString& error);
};

