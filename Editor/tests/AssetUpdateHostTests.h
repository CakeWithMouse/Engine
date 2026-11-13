#pragma once
#include "QtHost.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <map>
#include <stdexcept>

// Reimport/move through the real command queue: selection follows the object
// name across ReplaceEdit id regeneration, and undo survives reimport.
namespace AssetUpdateHostTests {
struct Scene {
    std::map<uint64_t, std::pair<std::string, EngineSceneTransform>> objects{
        {1, {"Hero", {{0,0,0},{0,0,0,1},{1,1,1}}}}};
    uint64_t nextId = 101;
    uint32_t lastAssetOp = 99;
    EngineEditSessionApi api{};
    EngineAuthoringApi authoring{};
    Scene() {
        api.context = api.scene.context = this;
        api.saveScene = [](void* p, const char* path, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QJsonArray rows;
            for (const auto& [id, obj] : s.objects)
                rows.append(QJsonObject{{"id", qint64(id)}, {"name", QString::fromStdString(obj.first)}, {"x", obj.second.position[0]}});
            QFile f(QString::fromUtf8(path));
            return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(rows).toJson()) > 0 ? EngineSceneOk : EngineSceneIoError;
        };
        api.loadScene = [](void* p, const char* path, uint64_t* id, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            QFile f(QString::fromUtf8(path)); if (!f.open(QIODevice::ReadOnly)) return EngineSceneIoError;
            s.objects.clear();
            for (const auto& row : QJsonDocument::fromJson(f.readAll()).array()) {
                auto o = row.toObject();
                s.objects[o["id"].toInteger()] = {o["name"].toString().toStdString(), {{float(o["x"].toDouble()),0,0},{0,0,0,1},{1,1,1}}};
            }
            *id = 1; return EngineSceneOk;
        };
        api.readActiveScene = [](void* p, EngineSceneInfo*, EngineSceneObjectVisitor visitor, void* user) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            for (const auto& [id, obj] : s.objects) {
                EngineSceneObjectInfo o{}; o.id = id; o.name = obj.first.c_str(); o.typeName = "Primitive.Cube"; o.transform = obj.second;
                visitor(user, &o);
            }
            return EngineSceneOk;
        };
        api.scene.setTransform = [](void* p, uint64_t id, const EngineSceneTransform* t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p);
            auto it = s.objects.find(id); if (it == s.objects.end()) return EngineSceneNotFound;
            it->second.second = *t; return EngineSceneOk;
        };
        api.getState = [](void*, uint32_t* state) -> uint32_t { *state = 0; return EngineSceneOk; };
        authoring.version = 1; authoring.context = this;
        authoring.describe = [](void*, uint64_t, EngineProjectDataVisitor visitor, void* p) -> uint32_t { visitor(p, "{\"fields\":[]}"); return EngineSceneOk; };
        authoring.asset = [](void* p, uint32_t op, const char*, const char*, char*, uint32_t) -> uint32_t {
            auto& s = *static_cast<Scene*>(p); s.lastAssetOp = op;
            // ReplaceEdit regenerates object ids; the name survives.
            const auto obj = s.objects.begin()->second;
            s.objects.clear(); s.objects[s.nextId] = obj; s.nextId += 100;
            return EngineSceneOk;
        };
    }
};
inline int Run() {
    int checks = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) throw std::runtime_error(text); ++checks; };
    Scene s;
    EngineModuleHost callbacks{};
    EditorQtHost host(callbacks);
    host.SetModuleApi(s.api);
    host.SetAuthoringApi(s.authoring);
    check(host.Start(), "host starts with in-memory scene");

    EditorCommand select; select.type = EditorCommand::SelectObject; select.objectId = 1;
    EditorCommand nudge; nudge.type = EditorCommand::UpdateObjectTransform; nudge.objectId = 1; nudge.path = "nudge"; nudge.right = 2;
    host.PostCommand(select); host.PostCommand(nudge); host.Update(0.016);
    check(host.ReadSnapshot().selected == 1 && host.ReadSnapshot().canUndo, "transform seeds undo");

    EditorCommand reimport; reimport.type = EditorCommand::UpdateAsset; reimport.w = 0;
    reimport.path = "/Game/Models/Hero/Hero.glb"; reimport.name = "C:/Sources/Hero.glb";
    host.PostCommand(reimport); host.Update(0.016);
    const auto afterReimport = host.ReadSnapshot();
    check(s.lastAssetOp == 0, "reimport routed as operation 0");
    check(afterReimport.selected == 101, "selection follows object name across id regeneration");
    check(afterReimport.canUndo, "reimport keeps undo history");

    EditorCommand undo; undo.type = EditorCommand::Undo;
    host.PostCommand(undo); host.Update(0.016);
    check(s.objects.size() == 1 && s.objects.begin()->first == 1 && s.objects.begin()->second.second.position[0] == 0,
          "undo after reimport restores the pre-edit checkpoint");

    EditorCommand move; move.type = EditorCommand::UpdateAsset; move.w = 1;
    move.path = "/Game/Models/Hero/Hero.glb"; move.name = "/Game/Models/HeroRenamed/Hero.glb";
    host.PostCommand(move); host.Update(0.016);
    check(s.lastAssetOp == 1, "move routed as operation 1");
    check(!host.ReadSnapshot().canUndo && !host.ReadSnapshot().canRedo, "rename/move resets stale history explicitly");
    std::cout << "Asset update host checks: " << checks << std::endl;
    return checks;
}
}