#include "MaterialGraphPresets.h"

#include <QColor>
#include <QPointF>
#include <QVariantMap>

namespace EditorCore
{

namespace
{
QVariantMap ColorProp(const QString& name, const QColor& value)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("color"), value}};
}

QVariantMap ScalarProp(const QString& name, double value)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("value"), value}};
}

QVariantMap TextureProp(const QString& name, int slot)
{
    return {{QStringLiteral("textureName"), name}, {QStringLiteral("slot"), slot}};
}
} // namespace

QString MaterialGraphPresets::OrmTextureName()
{
    return QStringLiteral("ORM");
}

MaterialGraphModel MaterialGraphPresets::BuildSimple()
{
    MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 200));
    const int base = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("BaseMap"), QPointF(-100, 60),
                                   TextureProp(QStringLiteral("BaseMap"), 0));
    const int normal = model.AddNode(QStringLiteral("NormalMap"), QStringLiteral("NormalMap"), QPointF(-100, 220),
                                     TextureProp(QStringLiteral("NormalMap"), 1));
    const int spec = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("SpecularMap"), QPointF(-100, 380),
                                   TextureProp(QStringLiteral("SpecularMap"), 2));
    const int ambient = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("AmbientMap"), QPointF(-100, 540),
                                     TextureProp(QStringLiteral("AmbientMap"), 3));
    const int emissive = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("EmissiveMap"), QPointF(-100, 700),
                                       TextureProp(QStringLiteral("EmissiveMap"), 4));
    const int alpha = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("AlphaMap"), QPointF(-100, 860),
                                    TextureProp(QStringLiteral("AlphaMap"), 5));

    model.AddConnection(base, QStringLiteral("RGB"), master, QStringLiteral("Albedo"));
    model.AddConnection(alpha, QStringLiteral("A"), master, QStringLiteral("Alpha"));
    model.AddConnection(spec, QStringLiteral("R"), master, QStringLiteral("Specular"));
    model.AddConnection(ambient, QStringLiteral("R"), master, QStringLiteral("AmbientModel"));
    model.AddConnection(emissive, QStringLiteral("RGB"), master, QStringLiteral("Emissive"));
    model.AddConnection(normal, QStringLiteral("Normal"), master, QStringLiteral("Normal"));
    return model;
}

MaterialGraphModel MaterialGraphPresets::BuildOrm()
{
    MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(620, 200));
    const int orm = model.AddNode(QStringLiteral("Texture2D"), OrmTextureName(), QPointF(-200, 220),
                                  TextureProp(OrmTextureName(), 6));
    const int base = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("BaseMap"), QPointF(-200, 60),
                                   TextureProp(QStringLiteral("BaseMap"), 7));
    const int split = model.AddNode(QStringLiteral("Split"), QStringLiteral("Unpack ORM"), QPointF(80, 220));
    const int inv = model.AddNode(QStringLiteral("OneMinus"), QStringLiteral("1 - Roughness"), QPointF(320, 300));
    const int normal = model.AddNode(QStringLiteral("NormalMap"), QStringLiteral("NormalMap"), QPointF(-200, 420),
                                     TextureProp(QStringLiteral("NormalMap"), 8));
    const int defaults = model.AddNode(QStringLiteral("SurfaceDefaults"), QStringLiteral("Surface Defaults"), QPointF(-200, 580));

    model.AddConnection(base, QStringLiteral("RGB"), master, QStringLiteral("Albedo"));
    model.AddConnection(orm, QStringLiteral("RGBA"), split, QStringLiteral("In"));
    // R = Ambient Occlusion -> AmbientModel; G = Roughness -> (1 - roughness) -> SpecularWeight.
    model.AddConnection(split, QStringLiteral("R"), master, QStringLiteral("AmbientModel"));
    model.AddConnection(split, QStringLiteral("G"), inv, QStringLiteral("In"));
    model.AddConnection(inv, QStringLiteral("Out"), master, QStringLiteral("Specular"));
    // NormalMap -> Normal
    model.AddConnection(normal, QStringLiteral("Normal"), master, QStringLiteral("Normal"));
    // B = Metallic распакован (пин Split.B свободен): входа Metallic в движке Блинн-Фонг нет.
    model.AddConnection(defaults, QStringLiteral("Alpha"), master, QStringLiteral("Alpha"));
    return model;
}

} // namespace EditorCore