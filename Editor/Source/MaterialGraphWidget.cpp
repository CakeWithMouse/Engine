#include "MaterialGraphWidget.h"
#include "EditorLocale.h"
#include "HlslGraphCompiler.h"
#include "MaterialGraphSchema.h"
#include "MaterialTextureTools.h"

#include <QClipboard>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <cmath>

using EditorCore::GraphDocument;
using EditorCore::GraphNodeData;
using EditorCore::MaterialGraphSchema;

namespace
{
constexpr int Thumbnail = 48;

const MaterialGraphSchema& MaterialSchema()
{
    return MaterialGraphSchema::Instance();
}

bool IsTextureType(const QString& type) { return MaterialGraphSchema::IsTextureType(type); }
bool IsParameterType(const QString& type) { return MaterialGraphSchema::IsParameterType(type); }
int VectorSize(const QString& type) { return MaterialGraphSchema::VectorSize(type); }
QString NodeName(const GraphNodeData& node) { return MaterialSchema().NodeName(node); }

QString VectorText(const QVariant& value, int size)
{
    const QVariantList values = value.toList();
    QStringList parts;
    for (int i = 0; i < size; ++i) parts << QString::number(i < values.size() ? values[i].toDouble() : 0.0, 'g', 3);
    return parts.join(QStringLiteral(", "));
}

// Значения параметра совпадают с точностью float: настройки материала хранят float, граф — double.
bool SameValue(const QVariant& a, const QVariant& b)
{
    const auto close = [](double x, double y) { return std::abs(x - y) <= 1e-5 * std::max(1.0, std::abs(x)); };
    if (a.metaType().id() == QMetaType::QColor && b.metaType().id() == QMetaType::QColor)
    {
        const QColor x = a.value<QColor>(), y = b.value<QColor>();
        return close(x.redF(), y.redF()) && close(x.greenF(), y.greenF()) && close(x.blueF(), y.blueF()) && close(x.alphaF(), y.alphaF());
    }
    if (a.metaType().id() == QMetaType::QVariantList || b.metaType().id() == QMetaType::QVariantList)
    {
        const QVariantList x = a.toList(), y = b.toList();
        if (x.size() != y.size()) return false;
        for (int i = 0; i < x.size(); ++i)
            if (!close(x[i].toDouble(), y[i].toDouble())) return false;
        return true;
    }
    bool okA = false, okB = false;
    const double x = a.toDouble(&okA), y = b.toDouble(&okB);
    return okA && okB && close(x, y);
}

// Значение параметра в виде, в котором его принимает материал.
std::array<float, 4> ParameterValue(const GraphNodeData& node)
{
    std::array<float, 4> value{};
    if (node.type == QStringLiteral("ColorParam"))
    {
        const QColor color = node.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
        return {static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())};
    }
    if (const int size = VectorSize(node.type))
    {
        const QVariantList stored = node.properties.value(QStringLiteral("value")).toList();
        for (int i = 0; i < size && i < stored.size(); ++i) value[i] = stored[i].toFloat();
        return value;
    }
    value[0] = node.properties.value(QStringLiteral("value"), 0.5).toFloat();
    return value;
}

// Несколько чисел одним диалогом: тайлинг, скорость, значения векторов.
bool EditValues(QWidget* parent, const QString& title, const QStringList& labels, QVector<double>& values)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("MaterialGraphValuesDialog"));
    dialog.setWindowTitle(title);
    auto* form = new QFormLayout(&dialog);
    QVector<QDoubleSpinBox*> boxes;
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* box = new QDoubleSpinBox(&dialog);
        box->setRange(-100000.0, 100000.0);
        box->setDecimals(3);
        box->setSingleStep(0.1);
        box->setValue(i < values.size() ? values[i] : 0.0);
        form->addRow(labels[i], box);
        boxes << box;
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    values.clear();
    for (auto* box : boxes) values << box->value();
    return true;
}

// Миниатюра ноды текстуры: считается при построении ноды, как и раньше при каждой пересборке сцены.
struct TextureDecoration
{
    QString file;
    QImage thumbnail;
};
} // namespace

Q_DECLARE_METATYPE(TextureDecoration)

QString MaterialGraphWidget::NodeDisplayName(const QString& type)
{
    return MaterialSchema().DisplayName(type);
}

MaterialGraphWidget::MaterialGraphWidget(QWidget* parent)
    : GraphEditorWidget(MaterialGraphSchema::Instance(), parent)
{
    setObjectName(QStringLiteral("MaterialGraph"));
    BuildMaterialToolbar();
    EditorCore::MaterialGraphModel model;
    model.SetupDefaultMasterNode();
    MutableDocument() = model;
    RebuildScene();
}

QStringList MaterialGraphWidget::PresetNames()
{
    return {QStringLiteral("Textured Color"), QStringLiteral("Animated Neon"),
            QStringLiteral("Gold"), QStringLiteral("Scrolling Energy")};
}

void MaterialGraphWidget::BuildMaterialToolbar()
{
    auto* presets = new QToolButton(this);
    presets->setObjectName(QStringLiteral("MaterialGraphPresets"));
    presets->setText(Tr("Готовые графы", "Presets"));
    presets->setPopupMode(QToolButton::InstantPopup);
    auto* presetMenu = new QMenu(presets);
    presetMenu->setObjectName(QStringLiteral("MaterialGraphPresetMenu"));
    // Список собирается при каждом открытии: свои графы могли добавить другие окна или git.
    connect(presetMenu, &QMenu::aboutToShow, this, [this, presetMenu] {
        presetMenu->clear();
        auto confirm = [this] {
            return QMessageBox::question(this, Tr("Готовый граф", "Preset"),
                Tr("Заменить текущий граф готовым?", "Replace the current graph with the preset?")) == QMessageBox::Yes;
        };
        presetMenu->addSection(Tr("Встроенные", "Built-in"));
        for (int i = 0; i < PresetNames().size(); ++i)
            presetMenu->addAction(PresetNames()[i], this, [this, i, confirm] { if (confirm()) LoadPreset(i); });

        const QStringList own = UserPresets();
        presetMenu->addSection(Tr("Проекта", "Project"));
        if (own.isEmpty())
            presetMenu->addAction(Tr("пока нет: сохраните текущий граф", "none yet: save the current graph"))->setEnabled(false);
        for (const QString& name : own)
            presetMenu->addAction(name, this, [this, name, confirm] {
                QString error;
                if (confirm() && !LoadUserPreset(name, error))
                    QMessageBox::warning(this, Tr("Готовый граф", "Preset"), error);
            });
        presetMenu->addSeparator();
        presetMenu->addAction(Tr("Сохранить текущий как готовый…", "Save current as preset…"), this, [this] {
            // Диалог после закрытия меню, а не из его цикла событий.
            QTimer::singleShot(0, this, [this] { RequestSaveUserPreset(); });
        })->setEnabled(!presetFolder_.isEmpty());
        if (!own.isEmpty())
        {
            auto* remove = presetMenu->addMenu(Tr("Удалить", "Delete"));
            for (const QString& name : own)
                remove->addAction(name, this, [this, name] {
                    if (QMessageBox::question(this, Tr("Удалить готовый граф", "Delete preset"),
                            Tr("Удалить «%1» из проекта?", "Delete \"%1\" from the project?").arg(name)) == QMessageBox::Yes)
                        RemoveUserPreset(name);
                });
        }
    });
    presets->setMenu(presetMenu);
    AddToolbarWidget(presets);

    auto* copy = new QToolButton(this);
    copy->setText(Tr("Копировать для ИИ", "Copy for AI"));
    copy->setToolTip(Tr("Описание нод и JSON графа в буфер обмена: вставьте в чат с нейросетью",
                        "Node reference and graph JSON to the clipboard, for a chat with an AI"));
    connect(copy, &QToolButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(AiPrompt());
        QMessageBox::information(this, Tr("Граф скопирован", "Graph copied"),
            Tr("Вставьте текст в чат с нейросетью, опишите нужный эффект, скопируйте ответ и нажмите «Вставить из ИИ».",
               "Paste it into an AI chat, describe the effect, copy the answer and press \"Paste from AI\"."));
    });
    AddToolbarWidget(copy);

    auto* paste = new QToolButton(this);
    paste->setText(Tr("Вставить из ИИ", "Paste from AI"));
    connect(paste, &QToolButton::clicked, this, [this] {
        QString error;
        if (!PasteFromAi(QGuiApplication::clipboard()->text(), &error))
            QMessageBox::warning(this, Tr("Граф не вставлен", "Graph not pasted"),
                error);
    });
    AddToolbarWidget(paste);
}

qreal MaterialGraphWidget::NodeWidth(const GraphNodeData& node) const
{
    return node.type == MaterialGraphSchema::MasterType() ? 170 : IsTextureType(node.type) ? 180 : 150;
}

qreal MaterialGraphWidget::NodeContentHeight(const GraphNodeData& node) const
{
    if (IsTextureType(node.type)) return Thumbnail + 6;
    return IsParameterType(node.type) || node.type == QStringLiteral("Constant") || node.type == QStringLiteral("UVCoord") ||
                   node.type == QStringLiteral("Panner") ? 22 : 0;
}

QVariant MaterialGraphWidget::NodeDecoration(const GraphNodeData& node) const
{
    if (!IsTextureType(node.type)) return {};
    TextureDecoration decoration;
    decoration.file = textures_.value(NodeName(node));
    if (!decoration.file.isEmpty()) decoration.thumbnail = MaterialTextures::Thumbnail(decoration.file, Thumbnail);
    return QVariant::fromValue(decoration);
}

void MaterialGraphWidget::PaintNodeContent(QPainter* painter, const GraphNodeData& node, const QRectF& content, const QVariant& decoration) const
{
    if (node.type == QStringLiteral("ColorParam"))
    {
        painter->setBrush(node.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>());
        painter->setPen(QPen(QColor(0x64, 0x74, 0x8B), 1.0));
        painter->drawRoundedRect(QRectF(content.center().x() - 24, content.top() + 2, 48, 14), 3, 3);
    }
    else if (node.type == QStringLiteral("ScalarParam") || node.type == QStringLiteral("Constant"))
        painter->drawText(content, Qt::AlignCenter, QString::number(node.properties.value(QStringLiteral("value"), 0.5).toDouble(), 'f', 3));
    else if (VectorSize(node.type))
        painter->drawText(content, Qt::AlignCenter, VectorText(node.properties.value(QStringLiteral("value")), VectorSize(node.type)));
    else if (node.type == QStringLiteral("UVCoord"))
        painter->drawText(content, Qt::AlignCenter, Tr("Тайлинг %1 × %2", "Tiling %1 x %2")
            .arg(node.properties.value(QStringLiteral("tilingX"), 1.0).toDouble())
            .arg(node.properties.value(QStringLiteral("tilingY"), 1.0).toDouble()));
    else if (node.type == QStringLiteral("Panner"))
        painter->drawText(content, Qt::AlignCenter, Tr("Скорость %1, %2", "Speed %1, %2")
            .arg(node.properties.value(QStringLiteral("speedX"), 0.5).toDouble())
            .arg(node.properties.value(QStringLiteral("speedY"), 0.0).toDouble()));
    else if (IsTextureType(node.type))
    {
        const TextureDecoration texture = decoration.value<TextureDecoration>();
        const QRectF image(content.left(), content.top(), Thumbnail, Thumbnail);
        if (!texture.thumbnail.isNull())
            painter->drawImage(QPointF(image.left() + (Thumbnail - texture.thumbnail.width()) / 2.0,
                                       image.top() + (Thumbnail - texture.thumbnail.height()) / 2.0), texture.thumbnail);
        else
            painter->fillRect(image, texture.file.isEmpty() ? QColor(Qt::white) : QColor(70, 30, 30));
        painter->setPen(QColor(0x94, 0xA3, 0xB8));
        const QString text = texture.file.isEmpty() ? Tr("нет текстуры", "no texture") : QFileInfo(texture.file).fileName();
        painter->drawText(QRectF(image.right() + 6, image.top(), content.width() - Thumbnail - 6, Thumbnail),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text);
    }
}

QString MaterialGraphWidget::EditActionText(const GraphNodeData& node) const
{
    return IsTextureType(node.type) ? Tr("Выбрать текстуру…", "Choose texture…") : GraphEditorWidget::EditActionText(node);
}

QString MaterialGraphWidget::RenameTitle() const
{
    return Tr("Имя в HLSL", "HLSL name");
}

void MaterialGraphWidget::SetParameterValue(const QString& name, const std::array<float, 4>& value)
{
    bool changed = false;
    for (const auto& node : Document().Nodes())
    {
        if (!IsParameterType(node.type) || NodeName(node) != name) continue;
        QString key = QStringLiteral("value");
        QVariant stored;
        if (node.type == QStringLiteral("ColorParam"))
        {
            key = QStringLiteral("color");
            stored = QColor::fromRgbF(value[0], value[1], value[2], value[3]);
        }
        else if (const int size = VectorSize(node.type))
        {
            QVariantList list;
            for (int i = 0; i < size; ++i) list << static_cast<double>(value[i]);
            stored = list;
        }
        else
            stored = static_cast<double>(value[0]);
        if (SameValue(node.properties.value(key), stored)) continue; // эхо своей же правки после пересборки настроек
        // Ползунок в настройках шлёт значения подряд: вся протяжка — один шаг отмены.
        if (!changed) Checkpoint(QStringLiteral("param:") + name);
        changed = true;
        MutableDocument().FindNode(node.id)->properties[key] = stored;
    }
    if (changed) RebuildScene();
}

void MaterialGraphWidget::SetTextures(const QHash<QString, QString>& files)
{
    if (files == textures_) return;
    textures_ = files;
    RebuildScene();
}

void MaterialGraphWidget::SetNodeTexture(const QString& textureName, const QString& gamePath)
{
    bool changed = false;
    for (const auto& node : Document().Nodes())
    {
        if (!IsTextureType(node.type) || NodeName(node) != textureName) continue;
        if (node.properties.value(QStringLiteral("texture")).toString() == gamePath) continue;
        if (!changed) Checkpoint();
        GraphNodeData* data = MutableDocument().FindNode(node.id);
        if (gamePath.isEmpty()) data->properties.remove(QStringLiteral("texture"));
        else data->properties[QStringLiteral("texture")] = gamePath;
        changed = true;
    }
    if (changed) RebuildScene();
}

QHash<QString, QString> MaterialGraphWidget::NodeTextures() const
{
    QHash<QString, QString> textures;
    for (const auto& node : Document().Nodes())
    {
        const QString path = node.properties.value(QStringLiteral("texture")).toString();
        if (IsTextureType(node.type) && !path.isEmpty()) textures.insert(NodeName(node), path);
    }
    return textures;
}

void MaterialGraphWidget::OnRestored(const GraphDocument& before)
{
    // Значения параметров и текстуры живут и в материале: отмена возвращает их и туда.
    for (const auto& node : Document().Nodes())
    {
        const GraphNodeData* old = before.FindNode(node.id);
        if (!old || old->type != node.type || NodeName(*old) != NodeName(node) || NodeName(node).isEmpty()) continue;
        if (IsParameterType(node.type) && old->properties != node.properties && parameterEdited)
            parameterEdited(NodeName(node), ParameterValue(node));
        const QString was = old->properties.value(QStringLiteral("texture")).toString();
        const QString now = node.properties.value(QStringLiteral("texture")).toString();
        if (IsTextureType(node.type) && was != now && textureRestored) textureRestored(NodeName(node), now);
    }
}

void MaterialGraphWidget::EditNode(int nodeId)
{
    GraphNodeData* node = MutableDocument().FindNode(nodeId);
    if (!node) return;
    const QString name = NodeName(*node);
    if (node->type == QStringLiteral("ColorParam"))
    {
        const QColor current = node->properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
        const QColor picked = QColorDialog::getColor(current, this, name, QColorDialog::ShowAlphaChannel);
        if (!picked.isValid() || !MutableDocument().FindNode(nodeId)) return;
        Checkpoint();
        MutableDocument().FindNode(nodeId)->properties[QStringLiteral("color")] = picked;
        RebuildScene();
        if (parameterEdited)
            parameterEdited(name, {static_cast<float>(picked.redF()), static_cast<float>(picked.greenF()),
                                   static_cast<float>(picked.blueF()), static_cast<float>(picked.alphaF())});
    }
    else if (node->type == QStringLiteral("ScalarParam"))
    {
        bool accepted = false;
        const double value = QInputDialog::getDouble(this, name, Tr("Значение:", "Value:"),
            node->properties.value(QStringLiteral("value"), 0.5).toDouble(), -100000.0, 100000.0, 3, &accepted);
        if (!accepted || !MutableDocument().FindNode(nodeId)) return;
        Checkpoint();
        MutableDocument().FindNode(nodeId)->properties[QStringLiteral("value")] = value;
        RebuildScene();
        if (parameterEdited) parameterEdited(name, {static_cast<float>(value), 0.0f, 0.0f, 0.0f});
    }
    else if (IsTextureType(node->type))
    {
        if (textureRequested) textureRequested(name);
    }
    else if (node->type == QStringLiteral("UVCoord") || node->type == QStringLiteral("Panner"))
    {
        const bool uv = node->type == QStringLiteral("UVCoord");
        const QString keyX = uv ? QStringLiteral("tilingX") : QStringLiteral("speedX");
        const QString keyY = uv ? QStringLiteral("tilingY") : QStringLiteral("speedY");
        QVector<double> values{node->properties.value(keyX, uv ? 1.0 : 0.5).toDouble(), node->properties.value(keyY, uv ? 1.0 : 0.0).toDouble()};
        if (!EditValues(this, uv ? Tr("Тайлинг UV", "UV tiling") : Tr("Скорость сдвига UV", "UV pan speed"),
                        {QStringLiteral("U"), QStringLiteral("V")}, values) || !MutableDocument().FindNode(nodeId))
            return;
        Checkpoint();
        node = MutableDocument().FindNode(nodeId);
        node->properties[keyX] = values[0];
        node->properties[keyY] = values[1];
        RebuildScene();
        NotifyStructure();
    }
    else if (node->type == QStringLiteral("Constant"))
    {
        // Константа вписана в HLSL числом: правка пересобирает шейдер.
        bool accepted = false;
        const double value = QInputDialog::getDouble(this, Tr("Константа", "Constant"), Tr("Значение:", "Value:"),
            node->properties.value(QStringLiteral("value"), 1.0).toDouble(), -100000.0, 100000.0, 4, &accepted);
        if (!accepted || !MutableDocument().FindNode(nodeId)) return;
        Checkpoint();
        MutableDocument().FindNode(nodeId)->properties[QStringLiteral("value")] = value;
        RebuildScene();
        NotifyStructure();
    }
    else if (const int size = VectorSize(node->type))
    {
        const QVariantList stored = node->properties.value(QStringLiteral("value")).toList();
        QVector<double> values;
        for (int i = 0; i < size; ++i) values << (i < stored.size() ? stored[i].toDouble() : 0.0);
        if (!EditValues(this, name, QStringList{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")}.mid(0, size), values) ||
            !MutableDocument().FindNode(nodeId))
            return;
        QVariantList list;
        std::array<float, 4> value{};
        for (int i = 0; i < size; ++i)
        {
            list << values[i];
            value[i] = static_cast<float>(values[i]);
        }
        Checkpoint();
        MutableDocument().FindNode(nodeId)->properties[QStringLiteral("value")] = list;
        RebuildScene();
        if (parameterEdited) parameterEdited(name, value);
    }
}

void MaterialGraphWidget::LoadPreset(int index)
{
    EditorCore::MaterialGraphModel model;
    auto node = [&](const char* type, const char* title, qreal x, qreal y, const QVariantMap& props = {}) {
        return model.AddNode(QString::fromLatin1(type), QString::fromLatin1(title), QPointF(x, y), props);
    };
    auto link = [&](int from, const char* fromPin, int to, const char* toPin) {
        model.AddConnection(from, QString::fromLatin1(fromPin), to, QString::fromLatin1(toPin));
    };
    auto color = [](const char* name, const QColor& value) { return QVariantMap{{QStringLiteral("name"), QString::fromLatin1(name)}, {QStringLiteral("color"), value}}; };
    auto scalar = [](const char* name, double value) { return QVariantMap{{QStringLiteral("name"), QString::fromLatin1(name)}, {QStringLiteral("value"), value}}; };
    auto texture = [](const char* name, int slot) { return QVariantMap{{QStringLiteral("textureName"), QString::fromLatin1(name)}, {QStringLiteral("slot"), slot}}; };

    const int master = node("MaterialOutput", "Material Output", 600, 180);
    if (index == 0)
    {
        const int base = node("ColorParam", "BaseColor", 100, 100, color("BaseColor", QColor(220, 220, 225)));
        const int map = node("Texture2D", "BaseMap", -160, 100, texture("BaseMap", 6));
        const int mul = node("Multiply", "Multiply", 360, 120);
        const int specular = node("ScalarParam", "Specular", 100, 260, scalar("Specular", 0.5));
        link(map, "RGB", mul, "A");
        link(base, "RGB", mul, "B");
        link(mul, "Out", master, "Albedo");
        link(specular, "Value", master, "Specular");
    }
    else if (index == 1)
    {
        const int dark = node("ColorParam", "DarkBase", 200, 80, color("DarkBase", QColor(20, 22, 28)));
        const int neon = node("ColorParam", "NeonColor", 40, 320, color("NeonColor", QColor(0, 220, 255)));
        const int time = node("Time", "Time", -160, 420);
        const int pulse = node("Multiply", "Pulse", 320, 340);
        const int specular = node("ScalarParam", "Specular", 200, 200, scalar("Specular", 0.85));
        link(dark, "RGB", master, "Albedo");
        link(neon, "RGB", pulse, "A");
        link(time, "SineTime", pulse, "B");
        link(pulse, "Out", master, "Emissive");
        link(specular, "Value", master, "Specular");
    }
    else if (index == 2)
    {
        // Как Gold.mat из BasicExamples: сильный блик, яркий край, плоское окружение.
        const int gold = node("ColorParam", "GoldTint", 100, 100, color("GoldTint", QColor(255, 184, 71)));
        const int fresnel = node("Fresnel", "Fresnel", 100, 220);
        const int rim = node("Add", "Add Rim", 360, 140);
        const int specular = node("ScalarParam", "Specular", 240, 300, scalar("Specular", 1.0));
        const int ambient = node("ScalarParam", "FlatAmbient", 240, 380, scalar("FlatAmbient", 0.0));
        link(gold, "RGB", rim, "A");
        link(fresnel, "Out", rim, "B");
        link(rim, "Out", master, "Albedo");
        link(specular, "Value", master, "Specular");
        link(ambient, "Value", master, "AmbientModel");
    }
    else
    {
        const int uv = node("UVCoord", "UVCoord", -300, 200, {{QStringLiteral("tilingX"), 4.0}, {QStringLiteral("tilingY"), 4.0}});
        const int panner = node("Panner", "Panner", -100, 200, {{QStringLiteral("speedX"), 0.4}, {QStringLiteral("speedY"), 0.1}});
        const int map = node("Texture2D", "EnergyMap", 100, 200, texture("BaseMap", 6));
        const int energy = node("ColorParam", "EnergyColor", 100, 360, color("EnergyColor", QColor(50, 255, 120)));
        const int glow = node("Multiply", "Glow", 360, 260);
        link(uv, "UV", panner, "UV");
        link(panner, "UV_Out", map, "UV");
        link(map, "RGB", glow, "A");
        link(energy, "RGB", glow, "B");
        link(glow, "Out", master, "Emissive");
    }
    Checkpoint(); // готовый граф и ответ ИИ отменяются, как любая правка
    ReplaceDocument(model);
    NotifyStructure();
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
    // Имя становится именем файла: буквы любого алфавита, цифры, пробел, «_», «-», «.».
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
    Checkpoint(); // готовый граф и ответ ИИ отменяются, как любая правка
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
    // Ноды и пины — из того же описания, что у компилятора: подсказка не расходится с графом.
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
    // Выдуманная нейросетью нода молча дала бы 0 в HLSL: такой ответ не принимается.
    if (std::any_of(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return !EditorCore::HlslGraphCompiler::IsKnownNode(n.type); }))
        return false;
    const auto compiled = EditorCore::HlslGraphCompiler::Compile(model);
    if (!compiled.problems.isEmpty())
    {
        if (error) *error = compiled.problems.constBegin().value();
        return false;
    }
    if (error) error->clear();
    Checkpoint(); // готовый граф и ответ ИИ отменяются, как любая правка
    ReplaceDocument(model);
    NotifyStructure();
    return true;
}
