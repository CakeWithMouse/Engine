#pragma once
#include "IEditorHost.h"
#include "EditorHistory.h"
#include <mutex>
#include <atomic>

class SceneCommandHandler
{
public:
    struct Context
    {
        EngineEditSessionApi scene;
        EngineProjectDataApi projectData;
        EngineSelectionApi selection;
        EngineViewportInputApi viewportInput;
        const EngineSceneBatchApi* batch = nullptr;
        const EngineAuthoringApi* authoring = nullptr;
    };

    SceneCommandHandler();

    bool CanHandle(EditorCommand::Type type) const;

    void Handle(const EditorCommand& cmd,
                Context& ctx,
                EditorHistory& history,
                IEditorHost::EditorState playState,
                uint32_t& result,
                QString& error);

    IEditorHost::Snapshot ReadSnapshot();
    void PublishScene(const Context& ctx, IEditorHost::EditorState playState);

    void SetSelectedId(uint64_t id);
    void PickSelection(uint64_t id, bool toggle);
    void MarkDirty() { dirty_ = true; }
    uint64_t SelectedId() const { return selectedId_; }
    bool HasSelection() const { return hasSelection_.load(); }
    std::string GetSelectedObjectName();
    void FetchSceneAndSelectNext(const std::function<void(const EditorCommand&)>& postCommand);
    void MoveSelectedObject(float right, float up, float forward, const std::function<void(const EditorCommand&)>& postCommand);
    bool IsDirty() const { return dirty_; }

private:
    static void ENGINE_MODULE_CALL SceneVisitor(void* ctx, const EngineSceneObjectInfo* info);

    std::mutex sceneMutex_;
    IEditorHost::Snapshot snapshot_;
    uint64_t selectedId_ = 0;
    std::vector<uint64_t> selectedIds_;
    std::atomic<bool> hasSelection_{false};
    bool dirty_ = false;
};

