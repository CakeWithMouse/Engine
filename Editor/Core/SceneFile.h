#pragma once

#include "FileService.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <cstdint>

namespace EditorCore
{

struct SceneObject
{
    uint64_t id = 0;
    QString name;
    QString type;
    uint64_t parent = 0;
    bool inheritParentScale = true;
    float position[3]{0.0f, 0.0f, 0.0f};
    float rotation[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float scale[3]{1.0f, 1.0f, 1.0f};
    QString modelAsset;
    QString textureAsset;
    QString materialAsset;
    QJsonObject properties;
};

struct SceneData
{
    int version = 1;
    QString name;
    QVector<SceneObject> objects;
};

class SceneFile
{
public:
    static QByteArray Serialize(const SceneData& scene);
    static bool Parse(const QByteArray& data, SceneData& outScene, QString* outError = nullptr);

    static bool Load(FileService& fileService, const QString& relPath, SceneData& outScene, QString* outError = nullptr);
    static bool Save(FileService& fileService, const QString& relPath, const SceneData& scene, QString* outError = nullptr);
};

} // namespace EditorCore
