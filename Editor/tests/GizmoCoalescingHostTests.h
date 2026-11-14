#pragma once
#include "QtHost.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <map>
#include <stdexcept>

// A fast gizmo drag queues hundreds of absolute transform ticks per frame; the host
// applies only the newest per object, keeps one Undo point per gesture and still reports failures.
namespace GizmoCoalescingHostTests {
struct Scene {
    std::map<uint64_t, float> x{{1, 0.0f}, {2, 0.0f}};
    int setTransforms = 0, saves = 0, publishes = 0;
    EngineEditSessionApi api{};
    Scene() {
        api.context = api.scene.context = this;
        api.saveScene = [](void* p, const char* path, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.saves;
            QJsonArray rows;
            for (const auto& [id, x] : s.x) rows.append(QJsonObject{{"id", qint64(id)}, {"x", x}});
            QFile f(QString::fromUtf8(path));
            return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(rows).toJson()) > 0 ? EngineSceneOk : EngineSceneIoError;
        };
        api.loadScene = [](void* p, const char* path, uint64_t* id, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QFile f(QString::fromUtf8(path)); if (!f.open(QIODevice::ReadOnly)) return EngineSceneIoError;
            s.x.clear();
            for (const auto& row : QJsonDocument::fromJson(f.readAll()).array())
                s.x[row.toObject()["id"].toInteger()] = float(row.toObject()["x"].toDouble());
            *id = 1; return EngineSceneOk;
        };
        api.readActiveScene = [](void* p, EngineSceneInfo*, EngineSceneObjectVisitor visitor, void* user) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.publishes;
            for (const auto& [id, x] : s.x) {
                EngineSceneObjectInfo o{}; o.id = id; o.name = id == 1 ? "A" : "B"; o.typeName = "Primitive.Cube";
                o.transform = {{x, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
                visitor(user, &o);
            }
            return EngineSceneOk;
        };
        api.scene.setTransform = [](void* p, uint64_t id, const EngineSceneTransform* t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.setTransforms;
            auto it = s.x.find(id); if (it == s.x.end()) return EngineSceneNotFound;
            it->second = t->position[0]; return EngineSceneOk;
        };
        api.getState = [](void*, uint32_t* state) -> uint32_t { *state = 0; return EngineSceneOk; };
    }
};
struct Listener : IEditorUiListener {
    std::map<uint32_t, int> ok, failed;
    uint32_t lastFailure = EngineSceneOk;
    void OnLogMessage(const QString&) override {}
    void OnCommandFinished(const EditorCommand& c, uint32_t status, const QString&) override {
        if (status == EngineSceneOk) ++ok[c.type]; else { ++failed[c.type]; lastFailure = status; }
    }
};
inline EditorCommand Tick(uint64_t id, float x) {
    EditorCommand c; c.type = EditorCommand::UpdateObjectTransform; c.objectId = id; c.path = "gizmo";
    c.transform.position[0] = x; return c;
}
inline EditorCommand Gesture(EditorCommand::Type type, uint64_t id) { EditorCommand c; c.type = type; c.objectId = id; return c; }

inline int Run() {
    int checks = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) throw std::runtime_error(text); ++checks; };
    Scene s; Listener listener;
    EngineModuleHost callbacks{};
    EditorQtHost host(callbacks);
    host.SetModuleApi(s.api);
    host.SetUiListener(&listener);
    check(host.Start(), "host starts with in-memory scene");
    EditorCommand refresh; refresh.type = EditorCommand::RefreshScene;
    EditorCommand cursor; cursor.type = EditorCommand::InputCursor;

    const int ticks = 500;
    s.setTransforms = s.saves = s.publishes = 0;
    host.PostCommand(Gesture(EditorCommand::BeginGizmoDrag, 1));
    for (int i = 1; i <= ticks; ++i) { host.PostCommand(Tick(1, i * 0.5f)); host.PostCommand(cursor); host.PostCommand(refresh); }
    host.PostCommand(Gesture(EditorCommand::EndGizmoDrag, 1));
    host.Update(0.016);
    check(s.x[1] == ticks * 0.5f, "final transform equals the last gizmo tick");
    check(s.setTransforms == 1, "a batch of gizmo ticks applies only the newest transform");
    check(listener.ok[EditorCommand::UpdateObjectTransform] == 1, "superseded gizmo ticks produce no completions");
    check(s.publishes <= 3, "N gizmo ticks with refreshes publish the scene at most a few times");
    check(s.saves == 1 && host.ReadSnapshot().canUndo, "one Undo checkpoint per gesture");

    // The same gesture split across frames still records one Undo point.
    s.setTransforms = s.saves = 0;
    host.PostCommand(Gesture(EditorCommand::BeginGizmoDrag, 1));
    for (int frame = 0; frame < 4; ++frame) {
        for (int i = 0; i < 50; ++i) host.PostCommand(Tick(1, 1000.0f + frame * 50 + i));
        host.Update(0.016);
    }
    host.PostCommand(Gesture(EditorCommand::EndGizmoDrag, 1));
    host.Update(0.016);
    check(s.x[1] == 1199.0f && s.setTransforms == 4, "each frame applies its newest tick");
    check(s.saves == 1, "multi-frame gesture keeps one Undo checkpoint");

    EditorCommand undo; undo.type = EditorCommand::Undo;
    host.PostCommand(undo); host.Update(0.016);
    check(s.x[1] == ticks * 0.5f, "one Undo reverts the whole second gesture");
    host.PostCommand(undo); host.Update(0.016);
    check(s.x[1] == 0.0f, "next Undo reverts the whole first gesture");
    EditorCommand redo; redo.type = EditorCommand::Redo;
    host.PostCommand(redo); host.Update(0.016);
    check(s.x[1] == ticks * 0.5f, "Redo restores the first gesture result");

    // Ticks for different objects, or split by another command, are never merged.
    s.setTransforms = 0;
    host.PostCommand(Tick(1, 7)); host.PostCommand(Tick(2, 8)); host.PostCommand(Tick(1, 9));
    EditorCommand select; select.type = EditorCommand::SelectObject; select.objectId = 2;
    host.PostCommand(Tick(2, 10)); host.PostCommand(select); host.PostCommand(Tick(2, 11));
    host.Update(0.016);
    check(s.setTransforms == 5 && s.x[1] == 9 && s.x[2] == 11, "only same-object runs are coalesced");

    // A failing drag still reports its error.
    for (int i = 0; i < 100; ++i) host.PostCommand(Tick(999, float(i)));
    host.Update(0.016);
    check(listener.failed[EditorCommand::UpdateObjectTransform] == 1 && listener.lastFailure == EngineSceneNotFound,
          "failed gizmo ticks still complete with their error status");

    host.SetUiListener(nullptr);
    std::cout << "Gizmo coalescing host checks: " << checks << std::endl;
    return checks;
}
}
