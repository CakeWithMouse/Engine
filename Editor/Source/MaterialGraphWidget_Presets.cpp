#include "MaterialGraphWidget.h"
#include "HlslGraphCompiler.h"
#include "EditorLocale.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QRegularExpression>
#include <QInputDialog>
#include <QMessageBox>
#include <algorithm>

namespace
{
const EditorCore::GraphSchema& MaterialSchema()
{
    return MaterialGraphSchema::Instance();
}
}

QString MaterialGraphWidget::UserPresetFile(const QString& name) const
{
    return presetFolder_.isEmpty() ? QString() : QDir(presetFolder_).filePath(name + QStringLiteral(".graph.json"));
}

QStringList MaterialGraphWidget::UserPresets() const
{
    QStringList names;
    if (presetFolder_.isEmpty()) return names;
    for (const QFileInfo& file : QDir(presetFolder_).entryInfoList({QStringLiteral("*.graph.json")}, QDir::Files, QDir::Name | QDir::IgnoreCase))
        names << file.fileName().chopped(static_cast<int>(QStringLiteral(".graph.json").size()));
    return names;
}

bool MaterialGraphWidget::SaveUserPreset(const QString& name, QString& error)
{
    static const QRegularExpression valid(QStringLiteral(R"(^[\w][\w .\-]*$)"), QRegularExpression::UseUnicodePropertiesOption);
    if (presetFolder_.isEmpty())
        error = Tr("Папка готовых графов проекта не задана.", "The project preset folder is not set.");
    else if (!valid.match(name).hasMatch() || name.endsWith(QLatin1Char('.')) || name.size() > 64)
        error = Tr("Имя: буквы, цифры, пробел, «_», «-» и «.», до 64 символов.", "Name: letters, digits, space, _, - and ., up to 64 characters.");
    else if (!QDir().mkpath(presetFolder_))
        error = Tr("Не удалось создать папку %1.", "Cannot create the folder %1.").arg(presetFolder_);
    else
    {
        QSaveFile file(UserPresetFile(name));
        if (file.open(QIODevice::WriteOnly) && file.write(Document().ExportToJson().toUtf8()) >= 0 && file.commit()) return true;
        error = Tr("Не удалось записать %1: %2", "Cannot write %1: %2").arg(file.fileName(), file.errorString());
    }
    return false;
}

bool MaterialGraphWidget::LoadUserPreset(const QString& name, QString& error)
{
    QFile file(UserPresetFile(name));
    EditorCore::MaterialGraphModel model;
    if (!file.open(QIODevice::ReadOnly) || !model.ImportFromJson(QString::fromUtf8(file.readAll())))
    {
        error = Tr("Готовый граф «%1» не читается.", "Cannot read the preset \"%1\".").arg(name);
        return false;
    }
    Checkpoint();
    ReplaceDocument(model);
    NotifyStructure();
    return true;
}

bool MaterialGraphWidget::RemoveUserPreset(const QString& name)
{
    return QFile::remove(UserPresetFile(name));
}

void MaterialGraphWidget::RequestSaveUserPreset()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, Tr("Сохранить готовый граф", "Save preset"),
        Tr("Имя (файл в Content/MaterialGraphs, виден всем через git):", "Name (a file in Content/MaterialGraphs, shared through git):"),
        QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    if (QFile::exists(UserPresetFile(name)) &&
        QMessageBox::question(this, Tr("Сохранить готовый граф", "Save preset"),
            Tr("«%1» уже есть. Заменить?", "\"%1\" exists. Replace it?").arg(name)) != QMessageBox::Yes)
        return;
    QString error;
    if (!SaveUserPreset(name, error)) QMessageBox::warning(this, Tr("Готовый граф не сохранён", "Preset not saved"), error);
}

QString MaterialGraphWidget::AiPrompt() const
{
    QString prompt;
    prompt += QStringLiteral("You edit a material node graph of a DirectX 11 engine. The graph compiles to an HLSL surface function.\n");
    prompt += QStringLiteral("Current graph JSON:\n\n```json\n%1```\n\n").arg(Document().ExportToJson());
    prompt += QStringLiteral(
        "Format: nodes have id, type, title, x, y and properties; connections link fromNode/fromPin (an output) to toNode/toPin (an input).\n"
        "An input takes one connection. Node types and pins:\n"
        "  * MaterialOutput, exactly one. Inputs are the surface of a Blinn-Phong lighting model, there is no PBR:\n"
        "    Albedo (float3 colour), Alpha (masked/translucent materials), Specular (0 matte .. 1 strong highlight),\n"
        "    AmbientModel (0 flat .. 1 hemispherical ambient), Emissive (float3, added after lighting), Normal (world normal).\n"
        "    An unconnected input keeps the default: the object's colour times its texture, no emission.\n"
        "Pin types are float, float2, float3, float4 or any (math nodes take the size of their largest input).\n"
        "Any output may feed any input: a scalar is broadcast, extra components are dropped, missing ones become 0 (alpha 1).\n");
    static const QHash<QString, QString> props{
        {QStringLiteral("ColorParam"), QStringLiteral("name, color [r,g,b,a] 0..1")},
        {QStringLiteral("ScalarParam"), QStringLiteral("name, value")},
        {QStringLiteral("Vector2Param"), QStringLiteral("name, value [x,y]")},
        {QStringLiteral("Vector3Param"), QStringLiteral("name, value [x,y,z]")},
        {QStringLiteral("Constant"), QStringLiteral("value")},
        {QStringLiteral("Texture2D"), QStringLiteral("textureName")},
        {QStringLiteral("NormalMap"), QStringLiteral("textureName")},
        {QStringLiteral("UVCoord"), QStringLiteral("tilingX, tilingY")},
        {QStringLiteral("Panner"), QStringLiteral("speedX, speedY")}};
    for (const auto& group : MaterialSchema().Groups())
        for (const QString& type : group.types)
        {
            QStringList inputs, outputs;
            for (const auto& pin : EditorCore::HlslGraphCompiler::NodePins(type)) (pin.isInput ? inputs : outputs) << pin.name + QLatin1Char(':') + pin.type;
            prompt += QStringLiteral("  * %1 (%2inputs: %3; outputs: %4)\n").arg(type,
                props.contains(type) ? QStringLiteral("props: %1; ").arg(props.value(type)) : QString(),
                inputs.isEmpty() ? QStringLiteral("none") : inputs.join(QStringLiteral(", ")), outputs.join(QStringLiteral(", ")));
        }
    prompt += QStringLiteral(
        "ObjectColor is the colour of the object; SurfaceDefaults gives the default surface (object colour times its texture).\n"
        "InstanceData is per-copy data of instanced meshes. NormalMap gives a world normal for MaterialOutput.Normal.\n"
        "Names of parameters and textures are HLSL identifiers and must be unique.\n"
        "Change the graph as the user asks and reply ONLY with the whole updated JSON in a ```json block.\n");
    return prompt;
}

bool MaterialGraphWidget::PasteFromAi(const QString& text, QString* error)
{
    if (error) *error = Tr("В буфере обмена нет корректного JSON графа с выходом материала.",
                           "The clipboard has no valid graph JSON with a material output.");
    QString json = text.trimmed();
    static const QRegularExpression block(QStringLiteral(R"(```(?:json)?\s*([\s\S]*?)\s*```)"));
    if (const auto match = block.match(json); match.hasMatch()) json = match.captured(1);
    EditorCore::MaterialGraphModel model;
    if (!model.ImportFromJson(json)) return false;
    const auto& nodes = model.Nodes();
    if (std::count_if(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return n.type == MaterialGraphSchema::MasterType(); }) != 1) return false;
    if (std::any_of(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return !EditorCore::HlslGraphCompiler::IsKnownNode(n.type); }))
        return false;
    const auto compiled = EditorCore::HlslGraphCompiler::Compile(model);
    if (!compiled.problems.isEmpty())
    {
        if (error) *error = compiled.problems.constBegin().value();
        return false;
    }
    if (error) error->clear();
    Checkpoint();
    ReplaceDocument(model);
    NotifyStructure();
    return true;
}

