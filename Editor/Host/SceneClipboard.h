#pragma once
#include "IEditorHost.h"
#include "SceneCommandHandler.h"
#include <QString>
#include <vector>

// Буфер обмена сцены на стороне редактора: снимок выделения читается через ReadSnapshot,
// вставка создаёт объекты существующими вызовами ABI одним чекпоинтом истории.
class SceneClipboard
{
public:
    bool HasContent() const { return !items_.empty(); }

    uint32_t Apply(const EditorCommand& cmd, SceneCommandHandler::Context& ctx, SceneCommandHandler& handler,
                   EditorHistory& history, IEditorHost::EditorState playState, QString& error);

private:
    std::vector<IEditorHost::SceneObject> items_;
};