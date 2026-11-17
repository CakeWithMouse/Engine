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
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int base = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(100, 100),
                                   ColorProp(QStringLiteral("BaseColor"), QColor(220, 220, 225)));
    const int specular = model.AddNode(QStringLiteral("ScalarParam"), QStringLiteral("Specular"), QPointF(100, 260),
                                       ScalarProp(QStringLiteral("Specular"), 0.5));
    const int defaults = model.AddNode(QStringLiteral("SurfaceDefaults"), QStringLiteral("Surface Defaults"), QPointF(100, 420));
    model.AddConnection(base, QStringLiteral("RGB"), master, QStringLiteral("Albedo"));
    model.AddConnection(specular, QStringLiteral("Value"), master, QStringLiteral("Specular"));
    model.AddConnection(defaults, QStringLiteral("Alpha"), master, QStringLiteral("Alpha"));
    model.AddConnection(defaults, QStringLiteral("Normal"), master, QStringLiteral("Normal"));
    return model;
}

MaterialGraphModel MaterialGraphPresets::BuildOrm()
{
    MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(620, 180));
    const int orm = model.AddNode(QStringLiteral("Texture2D"), OrmTextureName(), QPointF(-200, 120),
                                  TextureProp(OrmTextureName(), 6));
    const int split = model.AddNode(QStringLiteral("Split"), QStringLiteral("Unpack ORM"), QPointF(60, 120));
    const int base = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(-200, 340),
                                   ColorProp(QStringLiteral("BaseColor"), QColor(255, 255, 255)));
    const int inv = model.AddNode(QStringLiteral("OneMinus"), QStringLiteral("1 - Roughness"), QPointF(320, 240));
    const int defaults = model.AddNode(QStringLiteral("SurfaceDefaults"), QStringLiteral("Surface Defaults"), QPointF(-200, 500));
    model.AddConnection(orm, QStringLiteral("RGBA"), split, QStringLiteral("In"));
    model.AddConnection(base, QStringLiteral("RGB"), master, QStringLiteral("Albedo"));
    // R = Ambient Occlusion -> AmbientModel; G = Roughness -> (1 - roughness) -> SpecularWeight.
    model.AddConnection(split, QStringLiteral("R"), master, QStringLiteral("AmbientModel"));
    model.AddConnection(split, QStringLiteral("G"), inv, QStringLiteral("In"));
    model.AddConnection(inv, QStringLiteral("Out"), master, QStringLiteral("Specular"));
    // B = Metallic распакован (пин Split.B свободен): входа Metallic в движке Блинн-Фонг нет.
    model.AddConnection(defaults, QStringLiteral("Alpha"), master, QStringLiteral("Alpha"));
    model.AddConnection(defaults, QStringLiteral("Normal"), master, QStringLiteral("Normal"));
    return model;
}

} // namespace EditorCore