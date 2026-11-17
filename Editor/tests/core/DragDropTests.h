#pragma once

#include "../../Core/AssetDrag.h"
#include "../../Core/DropRules.h"
#include "../../Core/AssetOps.h"

#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <cstdio>
#include <cstdlib>

namespace
{

int gDragDropChecks = 0;

void CheckDragDrop(bool cond, const char* msg)
{
    ++gDragDropChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (DragDrop): %s\n", msg);
        std::exit(1);
    }
}

inline int RunDragDropTests()
{
    using namespace EditorCore;

    // --- MIME payload roundtrip ---
    AssetDrag::Payload payload;
    payload.virtualPaths = {QStringLiteral("/Game/Models/Hero.glb"), QStringLiteral("/Game/Models/Tree.glb")};
    payload.kind = AssetKind::Model;
    payload.source = QStringLiteral("ContentBrowser");
    const QByteArray encoded = AssetDrag::Encode(payload);

    AssetDrag::Payload decoded;
    CheckDragDrop(AssetDrag::Decode(encoded, decoded), "Decode roundtrip succeeds");
    CheckDragDrop(decoded.virtualPaths == payload.virtualPaths, "Decode preserves paths");
    CheckDragDrop(decoded.kind == AssetKind::Model, "Decode preserves kind");
    CheckDragDrop(decoded.source == QStringLiteral("ContentBrowser"), "Decode preserves source");

    // --- Malformed payloads rejected ---
    AssetDrag::Payload ignored;
    CheckDragDrop(!AssetDrag::Decode(QByteArray(), ignored), "Decode empty rejected");
    CheckDragDrop(!AssetDrag::Decode(QByteArrayLiteral("not json"), ignored), "Decode non-JSON rejected");
    CheckDragDrop(!AssetDrag::Decode(QByteArrayLiteral("[1,2,3]"), ignored), "Decode non-object rejected");
    CheckDragDrop(!AssetDrag::Decode(QByteArrayLiteral("{\"kind\":1}"), ignored), "Decode missing paths rejected");
    CheckDragDrop(!AssetDrag::Decode(QByteArrayLiteral("{\"paths\":[1,2]}"), ignored), "Decode non-string path rejected");

    // --- Out-of-range kind clamps to Unknown ---
    AssetDrag::Payload badKind;
    CheckDragDrop(AssetDrag::Decode(QByteArrayLiteral("{\"paths\":[\"/Game/a.mat\"],\"kind\":99}"), badKind), "Decode valid paths with bad kind");
    CheckDragDrop(badKind.kind == AssetKind::Unknown, "Out-of-range kind becomes Unknown");

    // --- uri-list -> local paths ---
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(QStringLiteral("C:/Project/Content/Models/Hero.glb"));
    urls << QUrl(QStringLiteral("https://example.com/remote.glb"));
    const QStringList local = AssetDrag::LocalPathsFromUrls(urls);
    CheckDragDrop(local.size() == 1, "Only local file URLs kept");
    CheckDragDrop(local.first() == QStringLiteral("C:/Project/Content/Models/Hero.glb"), "Local path extracted");

    // --- FromMime: our MIME ---
    QMimeData ownMime;
    ownMime.setData(QLatin1String(AssetDrag::kMimeType), encoded);
    CheckDragDrop(AssetDrag::HasAssetMime(ownMime), "HasAssetMime detects own format");
    const AssetDrag::Payload fromOwn = AssetDrag::FromMime(ownMime);
    CheckDragDrop(fromOwn.virtualPaths == payload.virtualPaths, "FromMime decodes own payload");

    // --- FromMime: external uri-list normalized to /Game (contentRoot must exist on disk) ---
    QTemporaryDir tempDir;
    CheckDragDrop(tempDir.isValid(), "Temporary content root created");
    const QString contentRoot = tempDir.path();
    const QString externalFile = contentRoot + QStringLiteral("/Textures/Albedo.png");
    QMimeData external;
    QList<QUrl> extUrls;
    extUrls << QUrl::fromLocalFile(externalFile);
    external.setUrls(extUrls);
    const AssetDrag::Payload fromExt = AssetDrag::FromMime(external, contentRoot);
    CheckDragDrop(fromExt.source == QStringLiteral("Explorer"), "External source is Explorer");
    CheckDragDrop(fromExt.kind == AssetKind::Texture, "External kind from extension");
    CheckDragDrop(fromExt.virtualPaths.first() == QStringLiteral("/Game/Textures/Albedo.png"), "External path normalized to /Game");

    // --- Drop rule matrix ---
    const auto action = [](DragSource s, DropTarget t, AssetKind k, bool copy = false) {
        return DecideDrop(s, t, k, copy).action;
    };
    CheckDragDrop(action(DragSource::Explorer, DropTarget::ContentBrowser, AssetKind::Model) == DropAction::Import, "Explorer->CB import");
    CheckDragDrop(action(DragSource::Explorer, DropTarget::Viewport, AssetKind::Model) == DropAction::None, "Explorer->Viewport disallowed");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::ContentBrowser, AssetKind::Model) == DropAction::MoveAsset, "CB->CB move");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::ContentBrowser, AssetKind::Model, true) == DropAction::CopyAsset, "CB->CB copy modifier");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Model) == DropAction::SpawnModel, "CB->Viewport model spawn");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Material) == DropAction::AssignMaterial, "CB->Viewport material");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Texture) == DropAction::AssignTexture, "CB->Viewport texture");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Script) == DropAction::AssignScript, "CB->Viewport script");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::AiMachine) == DropAction::AssignAiMachine, "CB->Viewport ai machine");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Audio) == DropAction::None, "CB->Viewport audio needs ABI");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Hierarchy, AssetKind::Model) == DropAction::ReplaceModel, "CB->Hierarchy model replace");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::InspectorSlot, AssetKind::Material) == DropAction::AssignMaterial, "CB->Inspector material");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::MaterialTextureSlot, AssetKind::Texture) == DropAction::AssignTexture, "CB->MaterialSlot texture");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::MaterialTextureSlot, AssetKind::Model) == DropAction::None, "CB->MaterialSlot model disallowed");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::GraphCanvas, AssetKind::Texture) == DropAction::CreateTextureNode, "CB->Graph texture node");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::GraphCanvas, AssetKind::Model) == DropAction::None, "CB->Graph model disallowed");
    CheckDragDrop(action(DragSource::Hierarchy, DropTarget::Hierarchy, AssetKind::Unknown) == DropAction::Reparent, "Hierarchy->Hierarchy reparent");
    CheckDragDrop(action(DragSource::Hierarchy, DropTarget::Viewport, AssetKind::Unknown) == DropAction::None, "Hierarchy->Viewport disallowed");
    CheckDragDrop(action(DragSource::GraphVariable, DropTarget::GraphCanvas, AssetKind::Unknown) == DropAction::CreateVariableNode, "Variable->Graph Get/Set node");
    CheckDragDrop(action(DragSource::UiPalette, DropTarget::UiCanvas, AssetKind::Unknown) == DropAction::AddUiWidget, "UiPalette->Canvas widget");
    CheckDragDrop(action(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Unknown) == DropAction::None, "Unknown kind disallowed");

    // --- Allowed decisions carry a command id ---
    const DropDecision spawn = DecideDrop(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Model);
    CheckDragDrop(spawn.allowed && spawn.commandId == QStringLiteral("Scene.SpawnModel"), "Spawn decision carries command id");
    const DropDecision blocked = DecideDrop(DragSource::ContentBrowser, DropTarget::Viewport, AssetKind::Audio);
    CheckDragDrop(!blocked.allowed && blocked.commandId.isEmpty(), "Blocked decision has no command");

    // --- Name-conflict suffix (_1, _2 ...) preserves directory and extension ---
    CheckDragDrop(AssetOps::ConflictName(QStringLiteral("Models/Hero.glb"), 1) == QStringLiteral("Models/Hero_1.glb"), "ConflictName inserts _1 before extension");
    CheckDragDrop(AssetOps::ConflictName(QStringLiteral("Models/Hero.glb"), 2) == QStringLiteral("Models/Hero_2.glb"), "ConflictName increments index");
    CheckDragDrop(AssetOps::ConflictName(QStringLiteral("Hero.glb"), 1) == QStringLiteral("Hero_1.glb"), "ConflictName without directory");
    CheckDragDrop(AssetOps::ConflictName(QStringLiteral("Models/Archive.tar.gz"), 1) == QStringLiteral("Models/Archive.tar_1.gz"), "ConflictName uses complete base name");

    return gDragDropChecks;
}

} // namespace