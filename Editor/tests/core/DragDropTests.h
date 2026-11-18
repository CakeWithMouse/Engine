#pragma once

#include "../../Core/AssetDrag.h"
#include "../../Core/DropRules.h"
#include "../../Core/AssetOps.h"
#include "../../Core/UiWidgetDrag.h"
#include "../../Core/SceneReparent.h"
#include "../../Core/UiReparent.h"

#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <algorithm>
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
    CheckDragDrop(action(DragSource::UiPalette, DropTarget::Hierarchy, AssetKind::Unknown) == DropAction::AddUiWidget, "UiPalette->Hierarchy widget");
    CheckDragDrop(action(DragSource::UiPalette, DropTarget::Viewport, AssetKind::Unknown) == DropAction::None, "UiPalette->Viewport disallowed");
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

    // --- UI widget palette drag payload ---
    for (const char* type : {"Panel", "Label", "Button", "Bar", "Group"})
    {
        UiWidgetDrag::Payload widget;
        widget.type = QLatin1String(type);
        UiWidgetDrag::Payload out;
        CheckDragDrop(UiWidgetDrag::Decode(UiWidgetDrag::Encode(widget), out), "UiWidgetDrag roundtrip decodes");
        CheckDragDrop(out.type == QLatin1String(type), "UiWidgetDrag preserves type");
    }
    UiWidgetDrag::Payload badWidget;
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArray(), badWidget), "UiWidgetDrag empty rejected");
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArrayLiteral("not json"), badWidget), "UiWidgetDrag non-JSON rejected");
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArrayLiteral("[1,2]"), badWidget), "UiWidgetDrag non-object rejected");
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArrayLiteral("{\"type\":\"Image\"}"), badWidget), "UiWidgetDrag unknown type rejected");
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArrayLiteral("{\"type\":5}"), badWidget), "UiWidgetDrag non-string type rejected");
    CheckDragDrop(!UiWidgetDrag::Decode(QByteArrayLiteral("{}"), badWidget), "UiWidgetDrag missing type rejected");
    QMimeData widgetMime;
    UiWidgetDrag::Payload button;
    button.type = QStringLiteral("Button");
    widgetMime.setData(QLatin1String(UiWidgetDrag::kMimeType), UiWidgetDrag::Encode(button));
    CheckDragDrop(UiWidgetDrag::HasMime(widgetMime), "UiWidgetDrag HasMime detects own format");
    CheckDragDrop(UiWidgetDrag::FromMime(widgetMime).type == QStringLiteral("Button"), "UiWidgetDrag FromMime decodes");
    QMimeData emptyMime;
    CheckDragDrop(UiWidgetDrag::FromMime(emptyMime).type.isEmpty(), "UiWidgetDrag FromMime empty without format");

    // --- Scene reparent validation (single source of truth for Scene.Reparent) ---
    {
        using EditorCore::SceneReparent::Node;
        std::vector<Node> scene;
        for (uint64_t i = 1; i <= 12; ++i)
            scene.push_back({i, 0});

        CheckDragDrop(EditorCore::SceneReparent::CanReparent(scene, 2, 1), "Reparent root child under another root allowed");
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(scene, 2, 2), "Reparent into itself rejected");
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(scene, 0, 1), "Reparent of missing child rejected");
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(scene, 2, 99), "Reparent under missing parent rejected");
        CheckDragDrop(EditorCore::SceneReparent::CanReparent(scene, 2, 0), "Reparent to scene root allowed");

        // Build chain 1 -> 2 -> 3 (3 is child of 2, 2 is child of 1).
        scene[1].parent = 1;
        scene[2].parent = 2;
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(scene, 1, 3), "Reparent into own descendant rejected");
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(scene, 1, 2), "Reparent into own child rejected");
        CheckDragDrop(EditorCore::SceneReparent::CanReparent(scene, 3, 4), "Reparent leaf elsewhere allowed");
        CheckDragDrop(!EditorCore::SceneReparent::HasCycle(scene), "Chain scene has no cycle");

        // Corrupt input is rejected, not looped on.
        std::vector<Node> dup = scene;
        dup.push_back({3, 0});
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(dup, 4, 1), "Duplicate ids rejected");
        std::vector<Node> cyclic = {{1, 2}, {2, 1}};
        CheckDragDrop(!EditorCore::SceneReparent::CanReparent(cyclic, 1, 2), "Pre-existing cycle rejected");
        CheckDragDrop(EditorCore::SceneReparent::HasCycle(cyclic), "HasCycle detects cycle");

        // N random reparents through the same guard: object count stays constant,
        // ids stay unique, no cycles ever appear.
        uint32_t rng = 0x12345678u;
        const auto nextRand = [&rng]() { rng = rng * 1664525u + 1013904223u; return rng; };
        int applied = 0;
        for (int step = 0; step < 500; ++step)
        {
            const uint64_t childId = 1 + nextRand() % 12;
            const uint64_t parentId = nextRand() % 13; // 0 = root
            if (!EditorCore::SceneReparent::CanReparent(scene, childId, parentId))
                continue;
            for (auto& n : scene)
                if (n.id == childId)
                    n.parent = parentId;
            ++applied;
            CheckDragDrop(scene.size() == 12, "Reparent keeps object count constant");
            CheckDragDrop(!EditorCore::SceneReparent::HasCycle(scene), "Reparent never creates a cycle");
        }
        CheckDragDrop(applied > 100, "Random reparent sequence actually applied moves");
        std::vector<Node> sorted = scene;
        std::sort(sorted.begin(), sorted.end(), [](const Node& a, const Node& b) { return a.id < b.id; });
        bool uniqueIds = true;
        for (size_t i = 0; i < sorted.size(); ++i)
            if (sorted[i].id != i + 1)
                uniqueIds = false;
        CheckDragDrop(uniqueIds, "No duplicate or lost ids after reparent sequence");
    }

    // --- UI hierarchy reparent validation and drop rules ---
    {
        using namespace EditorCore::UiReparent;
        auto root = std::make_unique<UIPanel>();
        root->Name = "Root";

        auto* panel1 = root->Add(std::make_unique<UIPanel>());
        panel1->Name = "Panel1";
        auto* panel2 = root->Add(std::make_unique<UIPanel>());
        panel2->Name = "Panel2";
        auto* button1 = panel1->Add(std::make_unique<UIButton>());
        button1->Name = "Button1";
        auto* label1 = panel1->Add(std::make_unique<UILabel>());
        label1->Name = "Label1";
        auto* bar1 = panel2->Add(std::make_unique<UIBar>());
        bar1->Name = "Bar1";

        CheckDragDrop(IsContainer(root.get()), "Root is container");
        CheckDragDrop(IsContainer(panel1), "Panel is container");
        CheckDragDrop(!IsContainer(button1), "Button is non-container");
        CheckDragDrop(!IsContainer(label1), "Label is non-container");
        CheckDragDrop(!IsContainer(bar1), "Bar is non-container");

        const size_t initialCount = CountElements(root.get());
        CheckDragDrop(initialCount == 6, "Initial UI element count is 6");
        CheckDragDrop(ValidateTree(root.get()), "Initial UI tree is valid");

        // CanReparent validation:
        CheckDragDrop(!CanReparent(root.get(), panel1, root.get()), "Cannot reparent root");
        CheckDragDrop(!CanReparent(panel1, panel1, root.get()), "Cannot reparent into itself");
        CheckDragDrop(!CanReparent(panel1, button1, root.get()), "Cannot reparent into non-container");
        CheckDragDrop(!CanReparent(panel1, label1, root.get()), "Cannot reparent into own descendant");
        CheckDragDrop(CanReparent(button1, panel2, root.get()), "Can reparent leaf into another panel");

        // MoveElement: move button1 from panel1 to panel2
        CheckDragDrop(MoveElement(button1, panel2, 0, root.get()), "Move button1 to panel2");
        CheckDragDrop(button1->GetParent() == panel2, "button1 parent updated to panel2");
        CheckDragDrop(IndexOfChild(button1) == 0, "button1 is at index 0 of panel2");
        CheckDragDrop(CountElements(root.get()) == initialCount, "Element count preserved after move");
        CheckDragDrop(ValidateTree(root.get()), "Tree valid after move");

        // ResolveDropTarget rules:
        // Dropping onto container -> becomes child at the end
        UIElement* targetParent = nullptr;
        size_t targetIndex = 0;
        ResolveDropTarget(panel1, DropPosition::OnItem, root.get(), targetParent, targetIndex);
        CheckDragDrop(targetParent == panel1, "Drop on container targets container as parent");
        CheckDragDrop(targetIndex == panel1->GetChildren().size(), "Drop on container targets end of children");

        // Dropping onto non-container -> becomes sibling after target
        ResolveDropTarget(label1, DropPosition::OnItem, root.get(), targetParent, targetIndex);
        CheckDragDrop(targetParent == panel1, "Drop on non-container targets its parent");
        CheckDragDrop(targetIndex == IndexOfChild(label1) + 1, "Drop on non-container targets index after it");

        // Dropping Above non-container -> sibling before target
        ResolveDropTarget(label1, DropPosition::AboveItem, root.get(), targetParent, targetIndex);
        CheckDragDrop(targetParent == panel1, "Drop Above targets parent");
        CheckDragDrop(targetIndex == IndexOfChild(label1), "Drop Above targets index of item");

        // Dropping on viewport -> child of root
        ResolveDropTarget(nullptr, DropPosition::OnViewport, root.get(), targetParent, targetIndex);
        CheckDragDrop(targetParent == root.get(), "Drop on viewport targets root");
        CheckDragDrop(targetIndex == root->GetChildren().size(), "Drop on viewport targets end of root");

        // Stress test: 100 random moves across tree
        // Verify: element count NEVER changes, tree NEVER has cycles or duplicates
        uint32_t rng = 0x98765432u;
        const auto nextRand = [&rng]() { rng = rng * 1664525u + 1013904223u; return rng; };
        int appliedMoves = 0;
        for (int step = 0; step < 200; ++step)
        {
            std::vector<UIElement*> all;
            auto collect = [&](auto self, UIElement* node) -> void {
                all.push_back(node);
                for (const auto& c : node->GetChildren()) self(self, c.get());
            };
            collect(collect, root.get());

            const size_t elemIdx = 1 + (nextRand() % (all.size() - 1)); // skip root
            const size_t targetIdx = nextRand() % all.size();
            UIElement* elem = all[elemIdx];
            UIElement* target = all[targetIdx];

            UIElement* p = nullptr;
            size_t idx = 0;
            const DropPosition dpos = static_cast<DropPosition>(nextRand() % 4);
            ResolveDropTarget(target, dpos, root.get(), p, idx);
            if (p && CanReparent(elem, p, root.get()))
            {
                const bool ok = MoveElement(elem, p, idx, root.get());
                CheckDragDrop(ok, "Valid MoveElement must succeed");
                ++appliedMoves;
            }
            CheckDragDrop(CountElements(root.get()) == initialCount, "Element count constant during stress test");
            CheckDragDrop(ValidateTree(root.get()), "Tree valid at every step of stress test");
        }
        CheckDragDrop(appliedMoves > 20, "Applied sufficient random UI moves");
    }

    return gDragDropChecks;
}

} // namespace