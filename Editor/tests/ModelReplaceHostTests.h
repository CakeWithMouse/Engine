#pragma once
#include "QtHost.h"
#include "../Host/ModelReplacement.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <map>
#include <stdexcept>

namespace ModelReplaceHostTests {

struct TestObject {
    std::string name;
    std::string type;
    std::string model;
    EngineSceneTransform transform{};
};

struct Scene {
    std::map<uint64_t, TestObject> objects;
    EngineEditSessionApi api{};

    Scene() {
        objects[1] = {"Character", "Engine.Model", "/Game/Models/CesiumMan.glb", {{0,0,0},{0,0,0,1},{1,1,1}}};
        objects[2] = {"Box", "Primitive.Cube", "", {{1,0,0},{0,0,0,1},{1,1,1}}};

        api.context = api.scene.context = this;
        api.saveScene = [](void* p, const char* path, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QJsonArray rows;
            for (const auto& [id, obj] : s.objects) {
                QJsonObject o;
                o["id"] = qint64(id);
                o["name"] = QString::fromStdString(obj.name);
                o["type"] = QString::fromStdString(obj.type);
                QJsonObject assets;
                assets["model"] = QString::fromStdString(obj.model);
                assets["texture"] = "";
                o["assets"] = assets;
                rows.append(o);
            }
            QJsonObject root;
            root["version"] = 1;
            root["objects"] = rows;
            QFile f(QString::fromUtf8(path));
            return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(root).toJson()) > 0 ? EngineSceneOk : EngineSceneIoError;
        };
        api.loadScene = [](void* p, const char* path, uint64_t* id, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QFile f(QString::fromUtf8(path));
            if (!f.open(QIODevice::ReadOnly)) return EngineSceneIoError;
            s.objects.clear();
            auto root = QJsonDocument::fromJson(f.readAll()).object();
            for (const auto& row : root["objects"].toArray()) {
                auto o = row.toObject();
                TestObject obj;
                obj.name = o["name"].toString().toStdString();
                obj.type = o["type"].toString().toStdString();
                obj.model = o["assets"].toObject()["model"].toString().toStdString();
                s.objects[o["id"].toInteger()] = obj;
            }
            *id = 1;
            return EngineSceneOk;
        };
        api.readActiveScene = [](void* p, EngineSceneInfo*, EngineSceneObjectVisitor visitor, void* user) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            for (const auto& [id, obj] : s.objects) {
                EngineSceneObjectInfo o{};
                o.id = id;
                o.name = obj.name.c_str();
                o.typeName = obj.type.c_str();
                o.modelAsset = obj.model.c_str();
                visitor(user, &o);
            }
            return EngineSceneOk;
        };
        api.getState = [](void*, uint32_t* state) -> uint32_t { *state = 0; return EngineSceneOk; };
    }
};

inline int Run() {
    int checks = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) throw std::runtime_error(text); ++checks; };

    Scene s;
    EngineModuleHost callbacks{};
    EditorQtHost host(callbacks);
    host.SetModuleApi(s.api);
    check(host.Start(), "Host started with test scene");

    // 1. Initial state
    check(s.objects[1].model == "/Game/Models/CesiumMan.glb", "Initial model is CesiumMan");

    // 2. Replace model on Engine.Model (id=1)
    EditorCommand select;
    select.type = EditorCommand::SelectObject;
    select.objectId = 2;
    host.PostCommand(select);

    EditorCommand cmd;
    cmd.type = EditorCommand::SetModel;
    cmd.objectId = 1;
    cmd.path = "/Game/Models/Hero.fbx";
    host.PostCommand(cmd);
    host.Update(0.016);

    check(s.objects[1].model == "/Game/Models/Hero.fbx", "Model replaced to Hero.fbx");
    check(host.ReadSnapshot().canUndo, "Undo point recorded");
    check(host.ReadSnapshot().selected == 1, "Explicit model target selected instead of prior primitive");

    // 3. Undo rolls back to original model
    EditorCommand undoCmd;
    undoCmd.type = EditorCommand::Undo;
    host.PostCommand(undoCmd);
    host.Update(0.016);

    check(s.objects[1].model == "/Game/Models/CesiumMan.glb", "Undo restored original model CesiumMan");
    check(!host.ReadSnapshot().canUndo, "Exactly one Undo point: none remain after undo");
    check(host.ReadSnapshot().canRedo, "canRedo is true after undo");

    // 4. Redo reapplies replaced model
    EditorCommand redoCmd;
    redoCmd.type = EditorCommand::Redo;
    host.PostCommand(redoCmd);
    host.Update(0.016);

    check(s.objects[1].model == "/Game/Models/Hero.fbx", "Redo reapplied replaced model");

    // 5. Empty model path rejected (prevents 'no project data codec' bug)
    const auto modelBeforeEmpty = s.objects[1].model;
    EditorCommand emptyCmd;
    emptyCmd.type = EditorCommand::SetModel;
    emptyCmd.objectId = 1;
    emptyCmd.path = "";
    host.PostCommand(emptyCmd);
    host.Update(0.016);

    check(s.objects[1].model == modelBeforeEmpty, "Empty model path does not modify scene");
    check(s.objects[1].type == "Engine.Model", "Object type is not mutated to Primitive.Cube on empty path");

    // 6. Replacing primitive with model is blocked
    EditorCommand primCmd;
    primCmd.type = EditorCommand::SetModel;
    primCmd.objectId = 2; // Primitive.Cube
    primCmd.path = "/Game/Models/Hero.fbx";
    host.PostCommand(primCmd);
    host.Update(0.016);

    check(s.objects[2].type == "Primitive.Cube", "Primitive is not mutated when replace is blocked");

    // Unsupported representations fail before loadScene, with type/version context.
    EditorHistory history;
    check(history.Initialize(), "Initialize validation history");
    const auto checkpoint = history.Checkpoint(s.api);
    QFile saved(checkpoint);
    check(saved.open(QIODevice::ReadOnly), "Read validation checkpoint");
    const auto valid = QJsonDocument::fromJson(saved.readAll()).object(); saved.close();
    auto rejected = [&](const QString& type, int version, bool malformed, const char* expected) {
        auto root = valid; root["version"] = version;
        auto rows = root["objects"].toArray(); auto object = rows[0].toObject();
        object["type"] = type;
        if (malformed) object["assets"] = QJsonArray();
        rows[0] = object; root["objects"] = rows;
        check(saved.open(QIODevice::WriteOnly | QIODevice::Truncate), "Write unsupported checkpoint");
        saved.write(QJsonDocument(root).toJson()); saved.close();
        bool refused = false;
        try { EditorHost::ReplaceModel(s.api, history, checkpoint, cmd); }
        catch (const std::exception& e) {
            const QString message = QString::fromUtf8(e.what());
            refused = message.contains(expected) && (type.startsWith("Primitive.") || !message.contains("C-ABI"));
        }
        check(refused, "Unsupported representation has precise diagnostic");
        check(s.objects[1].model == modelBeforeEmpty, "Unsupported checkpoint leaves scene untouched");
    };
    rejected("Engine.Model", 1, true, "assets.model");
    rejected("Engine.Model", 99, false, "version 99");
    rejected("Project.Model", 1, false, "Project.Model");
    rejected("Light.Point", 1, false, "Light.Point");
    rejected("Primitive.Cube", 1, false, "C-ABI");
    QFile::remove(checkpoint);

    return checks;
}

} // namespace ModelReplaceHostTests
