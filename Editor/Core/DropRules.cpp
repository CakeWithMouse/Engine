#include "DropRules.h"

namespace EditorCore
{
namespace
{
    DropDecision Allow(DropAction action, const char* commandId)
    {
        DropDecision decision;
        decision.action = action;
        decision.commandId = QLatin1String(commandId);
        decision.allowed = true;
        return decision;
    }
}

DropDecision DecideDrop(DragSource source, DropTarget target, AssetKind kind, bool copyRequested)
{
    const DropDecision none;

    // Перетаскивание объектов сцены: только reparent в дереве (тип ресурса не важен).
    if (source == DragSource::Hierarchy)
        return target == DropTarget::Hierarchy ? Allow(DropAction::Reparent, "Scene.Reparent") : none;

    // Переменная графа -> нода Get/Set на холсте.
    if (source == DragSource::GraphVariable)
        return target == DropTarget::GraphCanvas ? Allow(DropAction::CreateVariableNode, "Graph.CreateVariableNode") : none;

    // Палитра UI-редактора -> виджет на холсте.
    if (source == DragSource::UiPalette)
        return target == DropTarget::UiCanvas ? Allow(DropAction::AddUiWidget, "UI.AddWidget") : none;

    // Внешние файлы (Проводник): импорт в Content Browser; остальные цели требуют импорта.
    if (source == DragSource::Explorer)
    {
        if (target == DropTarget::ContentBrowser && kind != AssetKind::Unknown)
            return Allow(DropAction::Import, "Assets.Import");
        return none;
    }

    if (source != DragSource::ContentBrowser || kind == AssetKind::Unknown)
        return none;

    switch (target)
    {
    case DropTarget::ContentBrowser:
        return copyRequested ? Allow(DropAction::CopyAsset, "Assets.Copy")
                             : Allow(DropAction::MoveAsset, "Assets.Move");
    case DropTarget::Viewport:
        switch (kind)
        {
        case AssetKind::Model: return Allow(DropAction::SpawnModel, "Scene.SpawnModel");
        case AssetKind::Material: return Allow(DropAction::AssignMaterial, "Scene.AssignMaterial");
        case AssetKind::Texture: return Allow(DropAction::AssignTexture, "Scene.AssignTexture");
        case AssetKind::Script: return Allow(DropAction::AssignScript, "Scene.AssignScript");
        case AssetKind::AiMachine: return Allow(DropAction::AssignAiMachine, "Scene.AssignAiMachine");
        default: return none; // Audio: спавн звука требует ABI; UI: не спавнится во вьюпорте.
        }
    case DropTarget::Hierarchy:
    case DropTarget::InspectorSlot:
        switch (kind)
        {
        case AssetKind::Model: return Allow(DropAction::ReplaceModel, "Scene.ReplaceModel");
        case AssetKind::Material: return Allow(DropAction::AssignMaterial, "Scene.AssignMaterial");
        case AssetKind::Texture: return Allow(DropAction::AssignTexture, "Scene.AssignTexture");
        case AssetKind::Script: return Allow(DropAction::AssignScript, "Scene.AssignScript");
        case AssetKind::AiMachine: return Allow(DropAction::AssignAiMachine, "Scene.AssignAiMachine");
        default: return none;
        }
    case DropTarget::MaterialTextureSlot:
        return kind == AssetKind::Texture ? Allow(DropAction::AssignTexture, "Material.AssignTexture") : none;
    case DropTarget::GraphCanvas:
        return kind == AssetKind::Texture ? Allow(DropAction::CreateTextureNode, "Graph.CreateTextureNode") : none;
    default:
        return none;
    }
}
}