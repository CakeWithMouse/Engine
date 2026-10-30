#pragma once
#include "IEditorHost.h"
#include <memory>
#include <vector>
#include <chrono>
#include <QString>

class QTemporaryDir;

class EditorHistory
{
public:
    EditorHistory();
    ~EditorHistory();

    bool Initialize();
    void Reset();

    QString Checkpoint(const EngineEditSessionApi& scene);
    void RecordChange(const QString& checkpoint);

    bool ShouldCoalesce(const EditorCommand& cmd, std::chrono::steady_clock::time_point now) const;
    void OnTransformApplied(const EditorCommand& cmd, std::chrono::steady_clock::time_point now);

    void BeginGizmoDrag(uint64_t objectId);
    void EndGizmoDrag();

    bool Undo(const EngineEditSessionApi& scene, uint32_t& result, char* error, size_t errorSize);
    bool Redo(const EngineEditSessionApi& scene, uint32_t& result, char* error, size_t errorSize);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    void ClearHistory();
    QString CreateEmptySceneFile();
    QString GetTemporaryFilePath(const QString& templateName);

private:
    std::unique_ptr<QTemporaryDir> historyDirectory_;
    uint64_t historySerial_ = 0;
    uint64_t lastTransformId_ = 0;
    std::chrono::steady_clock::time_point lastTransformTime_{};

    bool gizmoDragActive_ = false;
    bool gizmoDragCheckpointed_ = false;
    uint64_t gizmoDragObject_ = 0;

    std::vector<QString> undo_;
    std::vector<QString> redo_;
};
