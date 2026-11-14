#pragma once
#include "SceneCommandHandler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

// Visual script commands over an in-memory scene whose checkpoints keep script paths, like scene files
// keep "assets.script": routing, Play read-only, undo/redo, duplicate and the published snapshot.
namespace HostScriptTests {
struct Scene {
    std::map<uint64_t, std::string> scripts{{1, ""}, {2, ""}};
    int saves = 0, sets = 0;
    EngineEditSessionApi api{};
    EngineScriptApi script{};
    Scene() {
        api.context = api.scene.context = this;
        api.saveScene = [](void* p, const char* path, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.saves;
            QJsonArray rows;
            for (const auto& [id, file] : s.scripts) rows.append(QJsonObject{{"id", qint64(id)}, {"script", QString::fromStdString(file)}});
            QFile f(QString::fromUtf8(path));
            return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(rows).toJson()) > 0 ? EngineSceneOk : EngineSceneIoError;
        };
        api.loadScene = [](void* p, const char* path, uint64_t* id, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QFile f(QString::fromUtf8(path)); if (!f.open(QIODevice::ReadOnly)) return EngineSceneIoError;
            s.scripts.clear();
            for (const auto& row : QJsonDocument::fromJson(f.readAll()).array())
                s.scripts[row.toObject()["id"].toInteger()] = row.toObject()["script"].toString().toStdString();
            *id = 1; return EngineSceneOk;
        };
        api.readActiveScene = [](void* p, EngineSceneInfo*, EngineSceneObjectVisitor visitor, void* user) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            for (const auto& entry : s.scripts) {
                const std::string name = "Object" + std::to_string(entry.first);
                EngineSceneObjectInfo o{}; o.id = entry.first; o.name = name.c_str(); o.typeName = "Primitive.Cube";
                o.transform = {{0,0,0},{0,0,0,1},{1,1,1}}; o.modelAsset = ""; o.textureAsset = ""; o.materialAsset = "";
                visitor(user, &o);
            }
            return EngineSceneOk;
        };
        api.scene.createObject = [](void* p, const char*, const char*, const EngineSceneTransform*, uint64_t* id) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); *id = s.scripts.rbegin()->first + 1; s.scripts[*id] = ""; return EngineSceneOk;
        };
        api.scene.removeObject = [](void* p, uint64_t id) -> uint32_t { static_cast<Scene*>(p)->scripts.erase(id); return EngineSceneOk; };
        script.version = 1; script.context = this;
        script.describe = [](void*, EngineScriptTextVisitor visitor, void* user) -> uint32_t { visitor(user, "{}"); return EngineSceneOk; };
        script.get = [](void* p, uint64_t id, EngineScriptTextVisitor visitor, void* user) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); auto found = s.scripts.find(id);
            if (found == s.scripts.end()) return EngineSceneNotFound;
            visitor(user, found->second.c_str()); return EngineSceneOk;
        };
        script.set = [](void* p, uint64_t id, const char* path, char* error, uint32_t capacity) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); ++s.sets;
            if (!s.scripts.count(id)) return EngineSceneNotFound;
            const std::string file = path;
            if (!file.empty() && file.find(".vscript") == std::string::npos) { if (capacity) std::snprintf(error, capacity, "Expected a .vscript"); return EngineSceneInvalidArgument; }
            s.scripts[id] = file; return EngineSceneOk;
        };
        script.check = [](void*, const char*, EngineScriptProblemVisitor, void*) -> uint32_t { return EngineSceneOk; };
        script.readProblems = [](void*, EngineScriptProblemVisitor, void*) -> uint32_t { return EngineSceneOk; };
    }
};

inline int Run() {
    int checks = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); ++checks; };
    Scene s; SceneCommandHandler handler; EditorHistory history;
    check(history.Initialize(), "script history initialization");
    SceneCommandHandler::Context ctx{s.api};
    ctx.script = &s.script;
    auto run = [&](EditorCommand c, IEditorHost::EditorState state = IEditorHost::Edit) {
        uint32_t result = 999; QString error; handler.Handle(c, ctx, history, state, result, error); return result;
    };
    auto published = [&](uint64_t id) {
        for (const auto& o : handler.ReadSnapshot().objects) if (o.id == id) return o.scriptAsset;
        return std::string("<missing>");
    };

    handler.PublishScene(ctx, IEditorHost::Edit);
    check(handler.ReadSnapshot().scripts, "script capability published");
    check(published(1).empty(), "object without a script publishes an empty path");

    EditorCommand assign; assign.type = EditorCommand::SetScript; assign.objectId = 1; assign.path = "/Game/Scripts/Spin.vscript";
    check(run(assign) == EngineSceneOk && s.scripts[1] == "/Game/Scripts/Spin.vscript", "SetScript routed to the extension");
    check(s.saves == 1 && history.CanUndo(), "SetScript is an undoable change");
    check(published(1) == "/Game/Scripts/Spin.vscript", "assigned script is published");

    EditorCommand wrong = assign; wrong.path = "/Game/Textures/wood.png";
    check(run(wrong) == EngineSceneInvalidArgument && s.scripts[1] == "/Game/Scripts/Spin.vscript", "invalid script refused, old one kept");

    check(run(assign, IEditorHost::Play) == EngineSceneReadOnly, "Play rejects SetScript");

    EditorCommand undo; undo.type = EditorCommand::Undo;
    check(run(undo) == EngineSceneOk && s.scripts[1].empty() && published(1).empty(), "undo removes the script");
    EditorCommand redo; redo.type = EditorCommand::Redo;
    check(run(redo) == EngineSceneOk && s.scripts[1] == "/Game/Scripts/Spin.vscript", "redo restores the script");

    EditorCommand duplicate; duplicate.type = EditorCommand::DuplicateObject; duplicate.objectId = 1;
    const int setsBefore = s.sets;
    check(run(duplicate) == EngineSceneOk, "duplicate succeeds");
    check(s.scripts.size() == 3 && s.scripts.rbegin()->second == "/Game/Scripts/Spin.vscript" && s.sets == setsBefore + 1, "duplicate copies the script");

    EditorCommand clear = assign; clear.path.clear();
    check(run(clear) == EngineSceneOk && s.scripts[1].empty(), "empty path clears the script");

    ctx.script = nullptr;
    check(run(assign) == EngineSceneReadOnly, "module without scripts keeps SetScript read-only");
    handler.PublishScene(ctx, IEditorHost::Edit);
    check(!handler.ReadSnapshot().scripts, "module without scripts publishes no capability");

    std::cout << "Host script checks: " << checks << std::endl;
    return checks;
}
}
