#pragma once
#include "EditorHistory.h"
#include "../Core/SceneObjectType.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <stdexcept>

namespace EditorHost {
// Work on a checkpoint, never the user's scene file. Preserve its format/version.
inline uint64_t ReplaceModel(const EngineEditSessionApi& api, EditorHistory& history,
                            const QString& checkpoint, const EditorCommand& cmd)
{
    if (cmd.path.trimmed().isEmpty()) throw std::runtime_error("Model path is empty.");
    QFile input(checkpoint);
    if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read model change checkpoint");
    QJsonParseError parseError;
    auto parsed = QJsonDocument::fromJson(input.readAll(), &parseError);
    input.close();
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject())
        throw std::runtime_error("Invalid scene JSON in model change checkpoint");
    auto document = parsed.object();
    auto objects = document["objects"].toArray();
    int targetIndex = -1;
    for (int i = 0; i < objects.size(); ++i) {
        auto object = objects[i].toObject();
        if (object["id"].toInteger() != static_cast<qint64>(cmd.objectId)) continue;
        const auto type = object["type"].toString().toStdString();
        const int version = document["version"].toInt(-1);
        if (!EditorCore::IsModelObjectType(type)) {
            if (EditorCore::IsPrimitiveObjectType(type))
                throw std::runtime_error("Replacing primitive geometry with a model requires Engine C-ABI.");
            throw std::runtime_error("Model replacement unsupported for type '" + type +
                                     "', scene version " + std::to_string(version) + ".");
        }
        if (version < 1 || version > 3 || !object["assets"].isObject() ||
            !object["assets"].toObject()["model"].isString() ||
            object["assets"].toObject()["model"].toString().isEmpty())
            throw std::runtime_error("Cannot preserve model representation for type '" + type +
                "', scene version " + std::to_string(version) +
                ": expected version 1-3 and assets.model string. Scene was not changed.");
        auto assets = object["assets"].toObject();
        assets["model"] = cmd.path;
        object["assets"] = assets;
        objects[i] = object;
        targetIndex = i;
        break;
    }
    if (targetIndex < 0) throw std::runtime_error("Object no longer exists");
    document["objects"] = objects;
    QTemporaryFile candidate(history.GetTemporaryFilePath(QStringLiteral("model-XXXXXX.scene.json")));
    const auto bytes = QJsonDocument(document).toJson();
    if (!candidate.open() || candidate.write(bytes) != bytes.size() || !candidate.flush())
        throw std::runtime_error("Cannot write model change candidate");
    candidate.close();
    uint64_t sceneId = 0;
    char error[1024]{};
    if (api.loadScene(api.context, candidate.fileName().toUtf8().constData(), &sceneId, error, sizeof(error)) != EngineSceneOk)
        throw std::runtime_error(error[0] ? error : "Cannot load model change candidate");
    // SceneDocument restores in document order; IDs may be reassigned. Names need
    // not be unique, and the current selection need not be the command's target.
    struct Target { int index; uint64_t id = 0; } target{targetIndex};
    EngineSceneInfo info{};
    api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
        auto& target = *static_cast<Target*>(p);
        if (target.index-- == 0) target.id = o->id;
    }, &target);
    return target.id;
}
}
