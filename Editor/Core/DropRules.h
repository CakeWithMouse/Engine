#pragma once

#include "AssetExtensions.h"

#include <QString>

namespace EditorCore
{
// Реестр правил drop: источник × цель × тип ресурса → конкретное действие и команда
// CommandRegistry. Чистая Core-логика (без виджетов), покрыта автотестом. Ячейки без
// рабочего пути к движку помечены None (Нужно в ABI / Backlog), без фиктивных обработчиков.
enum class DragSource
{
    ContentBrowser,
    Explorer,
    Hierarchy,
    GraphPalette,
    GraphVariable,
    UiPalette,
    Unknown
};

enum class DropTarget
{
    ContentBrowser,
    Viewport,
    Hierarchy,
    InspectorSlot,
    MaterialTextureSlot,
    GraphCanvas,
    UiCanvas,
    Unknown
};

enum class DropAction
{
    None,
    Import,
    SpawnModel,
    AssignMaterial,
    AssignTexture,
    AssignScript,
    AssignAiMachine,
    ReplaceModel,
    MoveAsset,
    CopyAsset,
    Reparent,
    CreateTextureNode,
    CreateVariableNode,
    AddUiWidget
};

struct DropDecision
{
    DropAction action = DropAction::None;
    QString commandId;
    bool allowed = false;
};

// copyRequested — модификатор копирования при внутреннем перетаскивании в браузере.
DropDecision DecideDrop(DragSource source, DropTarget target, AssetKind kind, bool copyRequested = false);
}