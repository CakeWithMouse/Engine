#include "SceneFile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace EditorCore
{

QByteArray SceneFile::Serialize(const SceneData& scene)
{
    QJsonObject root;
    root[QStringLiteral("version")] = scene.version;
    root[QStringLiteral("name")] = scene.name;

    QJsonArray objectsArr;
    for (const auto& obj : scene.objects)
    {
        QJsonObject o;
        o[QStringLiteral("id")] = static_cast<qint64>(obj.id);
        o[QStringLiteral("name")] = obj.name;
        o[QStringLiteral("type")] = obj.type;
        o[QStringLiteral("parent")] = static_cast<qint64>(obj.parent);
        o[QStringLiteral("inheritParentScale")] = obj.inheritParentScale;

        QJsonObject transform;
        QJsonArray posArr = { obj.position[0], obj.position[1], obj.position[2] };
        QJsonArray rotArr = { obj.rotation[0], obj.rotation[1], obj.rotation[2], obj.rotation[3] };
        QJsonArray scaleArr = { obj.scale[0], obj.scale[1], obj.scale[2] };
        transform[QStringLiteral("position")] = posArr;
        transform[QStringLiteral("rotation")] = rotArr;
        transform[QStringLiteral("scale")] = scaleArr;
        o[QStringLiteral("transform")] = transform;

        if (!obj.modelAsset.isEmpty())
            o[QStringLiteral("modelAsset")] = obj.modelAsset;
        if (!obj.textureAsset.isEmpty())
            o[QStringLiteral("textureAsset")] = obj.textureAsset;
        if (!obj.materialAsset.isEmpty())
            o[QStringLiteral("materialAsset")] = obj.materialAsset;
        if (!obj.properties.isEmpty())
            o[QStringLiteral("properties")] = obj.properties;

        objectsArr.append(o);
    }
    root[QStringLiteral("objects")] = objectsArr;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool SceneFile::Parse(const QByteArray& data, SceneData& outScene, QString* outError)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject())
    {
        if (outError)
            *outError = QStringLiteral("JSON parse error: %1").arg(err.errorString());
        return false;
    }

    QJsonObject root = doc.object();
    outScene.version = root.value(QStringLiteral("version")).toInt(1);
    outScene.name = root.value(QStringLiteral("name")).toString();
    outScene.objects.clear();

    QJsonArray objectsArr = root.value(QStringLiteral("objects")).toArray();
    for (const auto& val : objectsArr)
    {
        if (!val.isObject())
            continue;

        QJsonObject o = val.toObject();
        SceneObject obj;
        obj.id = static_cast<uint64_t>(o.value(QStringLiteral("id")).toVariant().toULongLong());
        obj.name = o.value(QStringLiteral("name")).toString();
        obj.type = o.value(QStringLiteral("type")).toString();
        obj.parent = static_cast<uint64_t>(o.value(QStringLiteral("parent")).toVariant().toULongLong());
        obj.inheritParentScale = o.value(QStringLiteral("inheritParentScale")).toBool(true);

        QJsonObject transform = o.value(QStringLiteral("transform")).toObject();
        QJsonArray posArr = transform.value(QStringLiteral("position")).toArray();
        if (posArr.size() == 3)
        {
            obj.position[0] = static_cast<float>(posArr[0].toDouble());
            obj.position[1] = static_cast<float>(posArr[1].toDouble());
            obj.position[2] = static_cast<float>(posArr[2].toDouble());
        }

        QJsonArray rotArr = transform.value(QStringLiteral("rotation")).toArray();
        if (rotArr.size() == 4)
        {
            obj.rotation[0] = static_cast<float>(rotArr[0].toDouble());
            obj.rotation[1] = static_cast<float>(rotArr[1].toDouble());
            obj.rotation[2] = static_cast<float>(rotArr[2].toDouble());
            obj.rotation[3] = static_cast<float>(rotArr[3].toDouble());
        }

        QJsonArray scaleArr = transform.value(QStringLiteral("scale")).toArray();
        if (scaleArr.size() == 3)
        {
            obj.scale[0] = static_cast<float>(scaleArr[0].toDouble());
            obj.scale[1] = static_cast<float>(scaleArr[1].toDouble());
            obj.scale[2] = static_cast<float>(scaleArr[2].toDouble());
        }

        obj.modelAsset = o.value(QStringLiteral("modelAsset")).toString();
        obj.textureAsset = o.value(QStringLiteral("textureAsset")).toString();
        obj.materialAsset = o.value(QStringLiteral("materialAsset")).toString();
        obj.properties = o.value(QStringLiteral("properties")).toObject();

        outScene.objects.append(obj);
    }

    return true;
}

bool SceneFile::Load(FileService& fileService, const QString& relPath, SceneData& outScene, QString* outError)
{
    QByteArray data;
    if (!fileService.ReadFile(relPath, data, outError))
        return false;

    return Parse(data, outScene, outError);
}

bool SceneFile::Save(FileService& fileService, const QString& relPath, const SceneData& scene, QString* outError)
{
    const QByteArray data = Serialize(scene);
    return fileService.WriteFileAtomic(relPath, data, outError);
}

} // namespace EditorCore
