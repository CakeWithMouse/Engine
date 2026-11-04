#include "MaterialGraphWidget.h"
#include "EditorLocale.h"
#include "HlslGraphCompiler.h"
#include "MaterialTextureTools.h"

#include <QAction>
#include <QClipboard>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

using EditorCore::GraphNodeData;
using EditorCore::GraphPin;

namespace
{
constexpr qreal GridStep = 20.0;
constexpr qreal RowHeight = 20.0;
constexpr qreal FirstRow = 36.0;
constexpr int Thumbnail = 48;

const QString MasterType = QStringLiteral("MaterialOutput");

bool IsTextureType(const QString& type)
{
    return type == QStringLiteral("Texture2D") || type == QStringLiteral("NormalMap");
}

bool IsParameterType(const QString& type)
{
    return type == QStringLiteral("ColorParam") || type == QStringLiteral("ScalarParam");
}

// Имя, под которым нода видна в HLSL: параметр cbuffer или Texture2D. Пустое у остальных.
QString NodeName(const GraphNodeData& node)
{
    if (IsParameterType(node.type)) return node.properties.value(QStringLiteral("name")).toString();
    if (IsTextureType(node.type)) return node.properties.value(QStringLiteral("textureName")).toString();
    return {};
}

QVector<GraphPin> NodePins(const QString& type)
{
    QVector<GraphPin> pins;
    auto in = [&](const char* name, const char* pinType) { pins.append({QString::fromLatin1(name), true, QString::fromLatin1(pinType), {}}); };
    auto out = [&](const char* name, const char* pinType) { pins.append({QString::fromLatin1(name), false, QString::fromLatin1(pinType), {}}); };
    if (type == MasterType)
    {
        // Поля MaterialOutput освещения движка (Блинн-Фонг); неподключённое поле остаётся как в шаблоне
        in("Albedo", "float3"); in("Alpha", "float"); in("Specular", "float"); in("AmbientModel", "float");
        in("Emissive", "float3"); in("Normal", "float3");
    }
    else if (type == QStringLiteral("ColorParam")) { out("RGBA", "float4"); out("RGB", "float3"); out("A", "float"); }
    else if (type == QStringLiteral("ScalarParam")) out("Value", "float");
    else if (type == QStringLiteral("Texture2D")) { in("UV", "float2"); out("RGBA", "float4"); out("RGB", "float3"); out("A", "float"); }
    else if (type == QStringLiteral("NormalMap")) { in("UV", "float2"); in("Strength", "float"); out("Normal", "float3"); }
    else if (type == QStringLiteral("Time")) { out("Time", "float"); out("SineTime", "float"); }
    else if (type == QStringLiteral("UVCoord")) out("UV", "float2");
    else if (type == QStringLiteral("Panner")) { in("UV", "float2"); out("UV_Out", "float2"); }
    else if (type == QStringLiteral("Multiply") || type == QStringLiteral("Add")) { in("A", "float"); in("B", "float"); out("Out", "float"); }
    else if (type == QStringLiteral("Lerp")) { in("A", "float"); in("B", "float"); in("Alpha", "float"); out("Out", "float"); }
    else if (type == QStringLiteral("OneMinus") || type == QStringLiteral("Sine")) { in("In", "float"); out("Out", "float"); }
    else if (type == QStringLiteral("Fresnel")) { in("Power", "float"); out("Out", "float"); }
    return pins;
}

QColor CategoryColor(const QString& type)
{
    if (type == MasterType) return QColor(0xC2, 0x41, 0x0C);
    if (IsParameterType(type)) return QColor(0x15, 0x80, 0x3D);
    if (IsTextureType(type)) return QColor(0x1D, 0x4E, 0xD8);
    if (type == QStringLiteral("Multiply") || type == QStringLiteral("Add") || type == QStringLiteral("Lerp") ||
        type == QStringLiteral("OneMinus") || type == QStringLiteral("Sine") || type == QStringLiteral("Fresnel"))
        return QColor(0x7E, 0x22, 0xCE);
    return QColor(0x0E, 0x74, 0x90); // Time, UVCoord, Panner
}

QPointF Snap(const QPointF& point)
{
    return {std::round(point.x() / GridStep) * GridStep, std::round(point.y() / GridStep) * GridStep};
}

QPainterPath LinkPath(const QPointF& from, const QPointF& to)
{
    QPainterPath path(from);
    const qreal dx = std::max(40.0, std::abs(to.x() - from.x()) * 0.5);
    path.cubicTo(from.x() + dx, from.y(), to.x() - dx, to.y(), to.x(), to.y());
    return path;
}

// Сетка за нодами: мелкий шаг совпадает с привязкой, каждая пятая линия ярче.
class GraphView final : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        painter->fillRect(rect, QColor(0x12, 0x14, 0x18));
        const bool minor = transform().m11() > 0.45;
        const qreal step = minor ? GridStep : GridStep * 5;
        const qreal left = std::floor(rect.left() / step) * step, top = std::floor(rect.top() / step) * step;
        QPen minorPen(QColor(0x1C, 0x1F, 0x26), 0), majorPen(QColor(0x27, 0x2B, 0x34), 0);
        for (qreal x = left; x < rect.right(); x += step)
        {
            painter->setPen(std::fmod(std::abs(x), GridStep * 5) < 0.5 ? majorPen : minorPen);
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = top; y < rect.bottom(); y += step)
        {
            painter->setPen(std::fmod(std::abs(y), GridStep * 5) < 0.5 ? majorPen : minorPen);
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }
};

bool EditPair(QWidget* parent, const QString& title, const QString& firstLabel, double& first,
              const QString& secondLabel, double& second)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("MaterialGraphPairDialog"));
    dialog.setWindowTitle(title);
    auto* form = new QFormLayout(&dialog);
    auto spin = [&](double value) {
        auto* box = new QDoubleSpinBox(&dialog);
        box->setRange(-1000.0, 1000.0);
        box->setDecimals(3);
        box->setSingleStep(0.1);
        box->setValue(value);
        return box;
    };
    auto* a = spin(first);
    auto* b = spin(second);
    form->addRow(firstLabel, a);
    form->addRow(secondLabel, b);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    first = a->value();
    second = b->value();
    return true;
}
}

class MaterialGraphWidget::NodeItem final : public QGraphicsItem
{
public:
    NodeItem(const GraphNodeData& data, MaterialGraphWidget* owner, const QString& textureFile)
        : data_(data), owner_(owner)
    {
        if (IsTextureType(data_.type) && !textureFile.isEmpty())
            thumbnail_ = MaterialTextures::Thumbnail(textureFile, Thumbnail);
        textureFile_ = textureFile;
        Layout();
        setPos(data.pos);
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(1);
    }

    const GraphNodeData& Data() const { return data_; }

    QRectF boundingRect() const override { return QRectF(-6, 0, width_ + 12, height_); }

    QPointF PinPos(const QString& name) const
    {
        for (const auto& pin : pins_)
            if (pin.name == name) return pin.localPos;
        return {};
    }

    bool HasPin(const QString& name, bool input) const
    {
        return std::any_of(pins_.begin(), pins_.end(), [&](const GraphPin& pin) { return pin.name == name && pin.isInput == input; });
    }

    bool PinAt(const QPointF& local, QString& name, bool& input) const
    {
        for (const auto& pin : pins_)
            if (QLineF(local, pin.localPos).length() <= 9.0)
            {
                name = pin.name;
                input = pin.isInput;
                return true;
            }
        return false;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath body;
        body.addRoundedRect(0, 0, width_, height_, 6, 6);
        painter->fillPath(body, QColor(0x18, 0x1B, 0x22));
        painter->setPen(isSelected() ? QPen(QColor(0x38, 0xBD, 0xF8), 2.0) : QPen(QColor(0x2D, 0x33, 0x42), 1.0));
        painter->drawPath(body);

        QPainterPath header;
        header.moveTo(0, 24);
        header.lineTo(0, 6);
        header.arcTo(0, 0, 12, 12, 180, -90);
        header.lineTo(width_ - 6, 0);
        header.arcTo(width_ - 12, 0, 12, 12, 90, -90);
        header.lineTo(width_, 24);
        header.closeSubpath();
        painter->fillPath(header, CategoryColor(data_.type));

        QFont font = painter->font();
        font.setBold(true);
        font.setPixelSize(11);
        painter->setFont(font);
        painter->setPen(Qt::white);
        painter->drawText(QRectF(8, 2, width_ - 16, 20), Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(data_.title, Qt::ElideRight, static_cast<int>(width_ - 16)));

        font.setBold(false);
        font.setPixelSize(10);
        painter->setFont(font);
        for (const auto& pin : pins_)
        {
            const QColor color = pin.isInput ? QColor(0x38, 0xBD, 0xF8) : QColor(0x34, 0xD3, 0x99);
            painter->setBrush(color);
            painter->setPen(QPen(color.darker(150), 1.0));
            painter->drawEllipse(pin.localPos, 4.5, 4.5);
            painter->setPen(QColor(0xCB, 0xD5, 0xE1));
            const QRectF label = pin.isInput ? QRectF(pin.localPos.x() + 8, pin.localPos.y() - 8, width_ / 2 + 10, 16)
                                             : QRectF(width_ / 2 - 20, pin.localPos.y() - 8, width_ / 2 + 8, 16);
            painter->drawText(label, (pin.isInput ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, pin.name);
        }

        // Значение ноды под пинами
        const QRectF content(8, contentTop_, width_ - 16, height_ - contentTop_ - 6);
        painter->setPen(QColor(0x94, 0xA3, 0xB8));
        if (data_.type == QStringLiteral("ColorParam"))
        {
            painter->setBrush(data_.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>());
            painter->setPen(QPen(QColor(0x64, 0x74, 0x8B), 1.0));
            painter->drawRoundedRect(QRectF(content.center().x() - 24, content.top() + 2, 48, 14), 3, 3);
        }
        else if (data_.type == QStringLiteral("ScalarParam"))
            painter->drawText(content, Qt::AlignCenter, QString::number(data_.properties.value(QStringLiteral("value"), 0.5).toDouble(), 'f', 3));
        else if (data_.type == QStringLiteral("UVCoord"))
            painter->drawText(content, Qt::AlignCenter, Tr("Тайлинг %1 × %2", "Tiling %1 x %2")
                .arg(data_.properties.value(QStringLiteral("tilingX"), 1.0).toDouble())
                .arg(data_.properties.value(QStringLiteral("tilingY"), 1.0).toDouble()));
        else if (data_.type == QStringLiteral("Panner"))
            painter->drawText(content, Qt::AlignCenter, Tr("Скорость %1, %2", "Speed %1, %2")
                .arg(data_.properties.value(QStringLiteral("speedX"), 0.5).toDouble())
                .arg(data_.properties.value(QStringLiteral("speedY"), 0.0).toDouble()));
        else if (IsTextureType(data_.type))
        {
            const QRectF image(content.left(), content.top(), Thumbnail, Thumbnail);
            if (!thumbnail_.isNull())
                painter->drawImage(QPointF(image.left() + (Thumbnail - thumbnail_.width()) / 2.0,
                                           image.top() + (Thumbnail - thumbnail_.height()) / 2.0), thumbnail_);
            else
                painter->fillRect(image, textureFile_.isEmpty() ? QColor(Qt::white) : QColor(70, 30, 30));
            painter->setPen(QColor(0x94, 0xA3, 0xB8));
            const QString text = textureFile_.isEmpty() ? Tr("нет текстуры", "no texture") : QFileInfo(textureFile_).fileName();
            painter->drawText(QRectF(image.right() + 6, image.top(), content.width() - Thumbnail - 6, Thumbnail),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text);
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionChange && scene()) return Snap(value.toPointF());
        if (change == ItemPositionHasChanged && scene() && owner_) owner_->NodeMoved(data_.id, pos());
        return QGraphicsItem::itemChange(change, value);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        // Диалог после события: правка перестраивает сцену и удаляет этот элемент.
        QTimer::singleShot(0, owner_, [owner = owner_, id = data_.id] { owner->EditNode(id); });
        event->accept();
    }

private:
    void Layout()
    {
        pins_ = NodePins(data_.type);
        int inputs = 0, outputs = 0;
        for (auto& pin : pins_)
        {
            int& row = pin.isInput ? inputs : outputs;
            pin.localPos = QPointF(0, FirstRow + row * RowHeight);
            ++row;
        }
        width_ = data_.type == MasterType ? 170 : IsTextureType(data_.type) ? 180 : 150;
        for (auto& pin : pins_)
            if (!pin.isInput) pin.localPos.setX(width_);
        contentTop_ = FirstRow + std::max(1, std::max(inputs, outputs)) * RowHeight - 6;
        const qreal content = IsTextureType(data_.type) ? Thumbnail + 6
                            : IsParameterType(data_.type) || data_.type == QStringLiteral("UVCoord") || data_.type == QStringLiteral("Panner") ? 22 : 0;
        height_ = contentTop_ + content + 6;
    }

    GraphNodeData data_;
    MaterialGraphWidget* owner_ = nullptr;
    QVector<GraphPin> pins_;
    QImage thumbnail_;
    QString textureFile_;
    qreal width_ = 150, height_ = 80, contentTop_ = 40;
};

MaterialGraphWidget::MaterialGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("MaterialGraph"));
    BuildUi();
    model_.SetupDefaultMasterNode();
    RebuildScene();
}

QStringList MaterialGraphWidget::PresetNames()
{
    return {QStringLiteral("Textured Color"), QStringLiteral("Animated Neon"),
            QStringLiteral("Gold"), QStringLiteral("Scrolling Energy")};
}

void MaterialGraphWidget::BuildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(4, 3, 4, 3);
    auto* add = new QToolButton(this);
    add->setObjectName(QStringLiteral("MaterialGraphAddNode"));
    add->setText(Tr("+ Нода", "+ Node"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto* addMenu = new QMenu(add);
    connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu] {
        addMenu->clear();
        const QPointF center = view_->mapToScene(view_->viewport()->rect().center());
        const QList<QPair<QString, QStringList>> groups{
            {Tr("Параметры", "Parameters"), {QStringLiteral("ColorParam"), QStringLiteral("ScalarParam")}},
            {Tr("Текстуры", "Textures"), {QStringLiteral("Texture2D"), QStringLiteral("NormalMap")}},
            {Tr("Координаты и время", "Coordinates and time"), {QStringLiteral("UVCoord"), QStringLiteral("Panner"), QStringLiteral("Time")}},
            {Tr("Математика", "Math"), {QStringLiteral("Multiply"), QStringLiteral("Add"), QStringLiteral("Lerp"),
                                         QStringLiteral("OneMinus"), QStringLiteral("Sine"), QStringLiteral("Fresnel")}}};
        for (const auto& group : groups)
        {
            auto* sub = addMenu->addMenu(group.first);
            for (const QString& type : group.second)
                sub->addAction(type, this, [this, type, center] { AddNode(type, center); });
        }
    });
    add->setMenu(addMenu);
    bar->addWidget(add);

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
    bar->addWidget(presets);

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
    bar->addWidget(copy);

    auto* paste = new QToolButton(this);
    paste->setText(Tr("Вставить из ИИ", "Paste from AI"));
    connect(paste, &QToolButton::clicked, this, [this] {
        if (!PasteFromAi(QGuiApplication::clipboard()->text()))
            QMessageBox::warning(this, Tr("Граф не вставлен", "Graph not pasted"),
                Tr("В буфере обмена нет JSON графа с выходом материала.", "The clipboard has no graph JSON with a material output."));
    });
    bar->addWidget(paste);
    bar->addStretch(1);
    auto* hint = new QLabel(Tr("ЛКМ по пину — связь · ПКМ — меню · колесо — масштаб · Del — удалить",
                               "LMB on a pin: link · RMB: menu · wheel: zoom · Del: delete"), this);
    hint->setEnabled(false);
    bar->addWidget(hint);
    layout->addLayout(bar);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-5000, -5000, 10000, 10000);
    view_ = new GraphView(scene_, this);
    view_->setObjectName(QStringLiteral("MaterialGraphView"));
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view_->viewport()->installEventFilter(this);
    view_->installEventFilter(this);
    layout->addWidget(view_, 1);
    view_->centerOn(400, 250);
}

void MaterialGraphWidget::SetModel(const EditorCore::MaterialGraphModel& model)
{
    model_ = model;
    // Граф без выхода (например, из старого формата) получает его: из меню выход не добавляется.
    const auto& nodes = model_.Nodes();
    if (std::none_of(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return n.type == MasterType; }))
        model_.AddNode(MasterType, QStringLiteral("Material Output"), QPointF(600, 180));
    RebuildScene();
    QRectF bounds;
    for (const auto& node : model_.Nodes()) bounds |= QRectF(node.pos, QSizeF(180, 160));
    if (!bounds.isNull()) view_->centerOn(bounds.center());
}

void MaterialGraphWidget::SetParameterValue(const QString& name, const std::array<float, 4>& value)
{
    for (const auto& node : model_.Nodes())
    {
        if (!IsParameterType(node.type) || NodeName(node) != name) continue;
        GraphNodeData* data = model_.FindNode(node.id);
        if (node.type == QStringLiteral("ColorParam"))
            data->properties[QStringLiteral("color")] = QColor::fromRgbF(value[0], value[1], value[2], value[3]);
        else
            data->properties[QStringLiteral("value")] = static_cast<double>(value[0]);
    }
    RebuildScene();
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
    for (const auto& node : model_.Nodes())
    {
        if (!IsTextureType(node.type) || NodeName(node) != textureName) continue;
        GraphNodeData* data = model_.FindNode(node.id);
        if (data->properties.value(QStringLiteral("texture")).toString() == gamePath) continue;
        if (gamePath.isEmpty()) data->properties.remove(QStringLiteral("texture"));
        else data->properties[QStringLiteral("texture")] = gamePath;
        changed = true;
    }
    if (changed) RebuildScene();
}

QHash<QString, QString> MaterialGraphWidget::NodeTextures() const
{
    QHash<QString, QString> textures;
    for (const auto& node : model_.Nodes())
    {
        const QString path = node.properties.value(QStringLiteral("texture")).toString();
        if (IsTextureType(node.type) && !path.isEmpty()) textures.insert(NodeName(node), path);
    }
    return textures;
}

void MaterialGraphWidget::RebuildScene()
{
    // Сначала связи: scene_->clear() удаляет и их.
    links_.clear();
    dragLink_ = nullptr;
    scene_->clear();
    for (const auto& node : model_.Nodes())
        scene_->addItem(new NodeItem(node, this, textures_.value(NodeName(node))));
    UpdateConnections();
}

MaterialGraphWidget::NodeItem* MaterialGraphWidget::FindItem(int nodeId) const
{
    for (auto* item : scene_->items())
        if (auto* node = dynamic_cast<NodeItem*>(item); node && node->Data().id == nodeId) return node;
    return nullptr;
}

QPointF MaterialGraphWidget::PinScenePos(int nodeId, const QString& pin) const
{
    const NodeItem* item = FindItem(nodeId);
    return item ? item->mapToScene(item->PinPos(pin)) : QPointF();
}

void MaterialGraphWidget::UpdateConnections()
{
    for (auto* link : links_)
    {
        scene_->removeItem(link);
        delete link;
    }
    links_.clear();
    QHash<int, NodeItem*> items;
    for (auto* item : scene_->items())
        if (auto* node = dynamic_cast<NodeItem*>(item)) items.insert(node->Data().id, node);
    for (const auto& c : model_.Connections())
    {
        NodeItem* from = items.value(c.fromNode);
        NodeItem* to = items.value(c.toNode);
        if (!from || !to) continue;
        auto* link = scene_->addPath(LinkPath(from->mapToScene(from->PinPos(c.fromPin)), to->mapToScene(to->PinPos(c.toPin))),
                                     QPen(QColor(0x38, 0xBD, 0xF8), 2.5));
        link->setZValue(0);
        links_.push_back(link);
    }
}

void MaterialGraphWidget::NodeMoved(int nodeId, const QPointF& pos)
{
    if (GraphNodeData* node = model_.FindNode(nodeId)) node->pos = pos;
    moved_ = true;
    UpdateConnections();
}

QString MaterialGraphWidget::UniqueName(const QString& base) const
{
    QStringList taken;
    for (const auto& node : model_.Nodes()) taken << NodeName(node);
    QString name = base;
    for (int i = 2; taken.contains(name); ++i) name = base + QString::number(i);
    return name;
}

int MaterialGraphWidget::AddNode(const QString& type, const QPointF& pos, const QVariantMap& props)
{
    QVariantMap properties = props;
    QString title = type;
    auto named = [&](const char* key, const QString& base) {
        if (!properties.contains(QLatin1String(key))) properties[QLatin1String(key)] = UniqueName(base);
        title = properties.value(QLatin1String(key)).toString();
    };
    if (type == QStringLiteral("ColorParam"))
    {
        named("name", QStringLiteral("Color"));
        if (!properties.contains(QStringLiteral("color"))) properties[QStringLiteral("color")] = QColor(Qt::white);
    }
    else if (type == QStringLiteral("ScalarParam"))
    {
        named("name", QStringLiteral("Scalar"));
        if (!properties.contains(QStringLiteral("value"))) properties[QStringLiteral("value")] = 0.5;
    }
    else if (type == QStringLiteral("Texture2D")) named("textureName", QStringLiteral("BaseMap"));
    else if (type == QStringLiteral("NormalMap")) named("textureName", QStringLiteral("NormalMap"));
    else if (type == QStringLiteral("UVCoord"))
    {
        if (!properties.contains(QStringLiteral("tilingX"))) properties[QStringLiteral("tilingX")] = 1.0;
        if (!properties.contains(QStringLiteral("tilingY"))) properties[QStringLiteral("tilingY")] = 1.0;
    }
    else if (type == QStringLiteral("Panner"))
    {
        if (!properties.contains(QStringLiteral("speedX"))) properties[QStringLiteral("speedX")] = 0.5;
        if (!properties.contains(QStringLiteral("speedY"))) properties[QStringLiteral("speedY")] = 0.0;
    }
    const int id = model_.AddNode(type, title, Snap(pos), properties);
    RebuildScene();
    NotifyStructure();
    return id;
}

bool MaterialGraphWidget::Connect(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
{
    const NodeItem* from = FindItem(fromNode);
    const NodeItem* to = FindItem(toNode);
    if (!from || !to || fromNode == toNode || !from->HasPin(fromPin, false) || !to->HasPin(toPin, true)) return false;

    // Цикл: вход источника уже зависит от узла назначения.
    QList<int> pending{fromNode}, seen;
    while (!pending.isEmpty())
    {
        const int id = pending.takeLast();
        if (id == toNode) return false;
        if (seen.contains(id)) continue;
        seen << id;
        for (const auto& c : model_.Connections())
            if (c.toNode == id) pending << c.fromNode;
    }

    for (const auto& c : model_.Connections())
        if (c.toNode == toNode && c.toPin == toPin)
        {
            model_.RemoveConnection(c.fromNode, c.fromPin, c.toNode, c.toPin);
            break;
        }
    model_.AddConnection(fromNode, fromPin, toNode, toPin);
    UpdateConnections();
    NotifyStructure();
    return true;
}

void MaterialGraphWidget::RemoveNodes(const QList<int>& ids)
{
    bool removed = false;
    for (const int id : ids)
    {
        const GraphNodeData* node = model_.FindNode(id);
        if (node && node->type != MasterType) removed = model_.RemoveNode(id) || removed; // выход материала не удаляется
    }
    if (!removed) return;
    RebuildScene();
    NotifyStructure();
}

bool MaterialGraphWidget::RenameNode(int nodeId, const QString& name)
{
    static const QRegularExpression valid(QStringLiteral(R"(^[A-Za-z_]\w*$)"));
    GraphNodeData* node = model_.FindNode(nodeId);
    if (!node || NodeName(*node).isEmpty() || !valid.match(name).hasMatch()) return false;
    if (name == NodeName(*node)) return true;
    if (UniqueName(name) != name) return false;
    node->properties[IsTextureType(node->type) ? QStringLiteral("textureName") : QStringLiteral("name")] = name;
    node->title = name;
    RebuildScene();
    NotifyStructure();
    return true;
}

void MaterialGraphWidget::DeleteSelected()
{
    QList<int> ids;
    for (auto* item : scene_->selectedItems())
        if (auto* node = dynamic_cast<NodeItem*>(item)) ids << node->Data().id;
    RemoveNodes(ids);
}

void MaterialGraphWidget::NotifyStructure()
{
    if (structureChanged) structureChanged();
}

bool MaterialGraphWidget::PinAt(const QPointF& scenePos, int& nodeId, QString& pin, bool& input) const
{
    for (auto* item : scene_->items(QRectF(scenePos - QPointF(10, 10), QSizeF(20, 20))))
        if (auto* node = dynamic_cast<NodeItem*>(item); node && node->PinAt(node->mapFromScene(scenePos), pin, input))
        {
            nodeId = node->Data().id;
            return true;
        }
    return false;
}

void MaterialGraphWidget::StartLink(const QPointF& scenePos)
{
    int nodeId = 0;
    QString pin;
    bool input = false;
    if (!PinAt(scenePos, nodeId, pin, input)) return;
    linkNode_ = nodeId;
    linkPin_ = pin;
    linkFromInput_ = input;
    if (input)
    {
        // Подключённый вход: связь снимается и тянется дальше от своего выхода, как в Unreal.
        for (const auto& c : model_.Connections())
            if (c.toNode == nodeId && c.toPin == pin)
            {
                model_.RemoveConnection(c.fromNode, c.fromPin, c.toNode, c.toPin);
                linkNode_ = c.fromNode;
                linkPin_ = c.fromPin;
                linkFromInput_ = false;
                UpdateConnections();
                NotifyStructure();
                break;
            }
    }
    linkStart_ = PinScenePos(linkNode_, linkPin_);
    dragLink_ = scene_->addPath(LinkPath(linkStart_, scenePos), QPen(QColor(0xE2, 0xE8, 0xF0), 2.0, Qt::DashLine));
    dragLink_->setZValue(2);
}

void MaterialGraphWidget::FinishLink(const QPointF& scenePos)
{
    scene_->removeItem(dragLink_);
    delete dragLink_;
    dragLink_ = nullptr;
    int nodeId = 0;
    QString pin;
    bool input = false;
    if (!PinAt(scenePos, nodeId, pin, input) || input == linkFromInput_) return;
    if (linkFromInput_) Connect(nodeId, pin, linkNode_, linkPin_);
    else Connect(linkNode_, linkPin_, nodeId, pin);
}

bool MaterialGraphWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view_ && event->type() == QEvent::KeyPress)
    {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
        {
            DeleteSelected();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }
    if (watched != view_->viewport()) return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(event);
        view_->setFocus();
        if (mouse->button() == Qt::LeftButton)
        {
            int nodeId = 0;
            QString pin;
            bool input = false;
            const QPointF scenePos = view_->mapToScene(mouse->position().toPoint());
            if (PinAt(scenePos, nodeId, pin, input))
            {
                StartLink(scenePos);
                return true;
            }
            moved_ = false;
        }
        else if (mouse->button() == Qt::RightButton || mouse->button() == Qt::MiddleButton)
        {
            panning_ = true;
            panMoved_ = false;
            panStart_ = panLast_ = mouse->position().toPoint();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (dragLink_)
        {
            const QPointF scenePos = view_->mapToScene(mouse->position().toPoint());
            dragLink_->setPath(linkFromInput_ ? LinkPath(scenePos, linkStart_) : LinkPath(linkStart_, scenePos));
            return true;
        }
        if (panning_)
        {
            const QPoint delta = mouse->position().toPoint() - panLast_;
            panLast_ = mouse->position().toPoint();
            panMoved_ = panMoved_ || (panLast_ - panStart_).manhattanLength() > 4;
            view_->horizontalScrollBar()->setValue(view_->horizontalScrollBar()->value() - delta.x());
            view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->value() - delta.y());
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (dragLink_ && mouse->button() == Qt::LeftButton)
        {
            FinishLink(view_->mapToScene(mouse->position().toPoint()));
            return true;
        }
        if (panning_)
        {
            panning_ = false;
            if (mouse->button() == Qt::RightButton && !panMoved_)
            {
                const QPointF scenePos = view_->mapToScene(mouse->position().toPoint());
                const QPoint screen = mouse->globalPosition().toPoint();
                NodeItem* node = nullptr;
                for (auto* item : scene_->items(scenePos))
                    if ((node = dynamic_cast<NodeItem*>(item))) break;
                // Меню из очереди: команда меню перестраивает сцену.
                if (node)
                    QTimer::singleShot(0, this, [this, screen, id = node->Data().id] { ShowNodeMenu(screen, id); });
                else
                    QTimer::singleShot(0, this, [this, screen, scenePos] { ShowCanvasMenu(screen, scenePos); });
            }
            return true;
        }
        if (mouse->button() == Qt::LeftButton && moved_)
        {
            moved_ = false;
            if (layoutChanged) layoutChanged();
        }
    }
    else if (event->type() == QEvent::Wheel)
    {
        const double factor = static_cast<QWheelEvent*>(event)->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        const double scale = view_->transform().m11() * factor;
        if (scale > 0.2 && scale < 3.0) view_->scale(factor, factor);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MaterialGraphWidget::ShowCanvasMenu(const QPoint& screenPos, const QPointF& scenePos)
{
    QMenu menu(this);
    const QList<QPair<QString, QStringList>> groups{
        {Tr("Параметры", "Parameters"), {QStringLiteral("ColorParam"), QStringLiteral("ScalarParam")}},
        {Tr("Текстуры", "Textures"), {QStringLiteral("Texture2D"), QStringLiteral("NormalMap")}},
        {Tr("Координаты и время", "Coordinates and time"), {QStringLiteral("UVCoord"), QStringLiteral("Panner"), QStringLiteral("Time")}},
        {Tr("Математика", "Math"), {QStringLiteral("Multiply"), QStringLiteral("Add"), QStringLiteral("Lerp"),
                                     QStringLiteral("OneMinus"), QStringLiteral("Sine"), QStringLiteral("Fresnel")}}};
    for (const auto& group : groups)
    {
        menu.addSection(group.first);
        for (const QString& type : group.second)
            menu.addAction(type, this, [this, type, scenePos] { AddNode(type, scenePos); });
    }
    menu.exec(screenPos);
}

void MaterialGraphWidget::ShowNodeMenu(const QPoint& screenPos, int nodeId)
{
    const GraphNodeData* node = model_.FindNode(nodeId);
    if (!node) return;
    QMenu menu(this);
    menu.addAction(IsTextureType(node->type) ? Tr("Выбрать текстуру…", "Choose texture…") : Tr("Свойства…", "Properties…"),
                   this, [this, nodeId] { EditNode(nodeId); });
    if (!NodeName(*node).isEmpty())
        menu.addAction(Tr("Переименовать…", "Rename…"), this, [this, nodeId] { RequestRename(nodeId); });
    if (node->type != MasterType)
        menu.addAction(Tr("Удалить", "Delete"), this, [this, nodeId] { RemoveNodes({nodeId}); });
    menu.exec(screenPos);
}

void MaterialGraphWidget::RequestRename(int nodeId)
{
    const GraphNodeData* node = model_.FindNode(nodeId);
    if (!node) return;
    bool accepted = false;
    const QString name = QInputDialog::getText(this, Tr("Имя в HLSL", "HLSL name"), Tr("Имя:", "Name:"),
                                               QLineEdit::Normal, NodeName(*node), &accepted).trimmed();
    if (accepted && !RenameNode(nodeId, name))
        QMessageBox::warning(this, Tr("Имя не подходит", "Invalid name"),
            Tr("Латинские буквы, цифры и «_», не с цифры; имя не должно повторяться в графе.",
               "Latin letters, digits and _, not starting with a digit; unique in the graph."));
}

void MaterialGraphWidget::EditNode(int nodeId)
{
    GraphNodeData* node = model_.FindNode(nodeId);
    if (!node) return;
    const QString name = NodeName(*node);
    if (node->type == QStringLiteral("ColorParam"))
    {
        const QColor current = node->properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
        const QColor picked = QColorDialog::getColor(current, this, name, QColorDialog::ShowAlphaChannel);
        if (!picked.isValid() || !(node = model_.FindNode(nodeId))) return;
        node->properties[QStringLiteral("color")] = picked;
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
        if (!accepted || !(node = model_.FindNode(nodeId))) return;
        node->properties[QStringLiteral("value")] = value;
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
        double x = node->properties.value(keyX, uv ? 1.0 : 0.5).toDouble(), y = node->properties.value(keyY, uv ? 1.0 : 0.0).toDouble();
        if (!EditPair(this, uv ? Tr("Тайлинг UV", "UV tiling") : Tr("Скорость сдвига UV", "UV pan speed"),
                      QStringLiteral("U"), x, QStringLiteral("V"), y) || !(node = model_.FindNode(nodeId)))
            return;
        node->properties[keyX] = x;
        node->properties[keyY] = y;
        RebuildScene();
        NotifyStructure();
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
    SetModel(model);
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
        if (file.open(QIODevice::WriteOnly) && file.write(model_.ExportToJson().toUtf8()) >= 0 && file.commit()) return true;
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
    SetModel(model);
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
    prompt += QStringLiteral("Current graph JSON:\n\n```json\n%1```\n\n").arg(model_.ExportToJson());
    prompt += QStringLiteral(
        "Format: nodes have id, type, title, x, y and properties; connections link fromNode/fromPin (an output) to toNode/toPin (an input).\n"
        "An input takes one connection. Node types and pins:\n"
        "  * MaterialOutput, exactly one. Inputs are the surface of a Blinn-Phong lighting model, there is no PBR:\n"
        "    Albedo (float3 colour), Alpha (masked/translucent materials), Specular (0 matte .. 1 strong highlight),\n"
        "    AmbientModel (0 flat .. 1 hemispherical ambient), Emissive (float3, added after lighting), Normal (world normal).\n"
        "    An unconnected input keeps the default: the object's colour times its texture, no emission.\n"
        "  * ColorParam (props: name, color [r,g,b,a] 0..1; outputs: RGBA, RGB, A)\n"
        "  * ScalarParam (props: name, value; outputs: Value)\n"
        "  * Texture2D (props: textureName; inputs: UV; outputs: RGBA, RGB, A)\n"
        "  * NormalMap (props: textureName; inputs: UV, Strength; outputs: Normal, a world normal for MaterialOutput.Normal)\n"
        "  * Time (outputs: Time, SineTime); UVCoord (props: tilingX, tilingY; outputs: UV)\n"
        "  * Panner (props: speedX, speedY; inputs: UV; outputs: UV_Out)\n"
        "  * Multiply, Add (inputs: A, B; outputs: Out); Lerp (inputs: A, B, Alpha; outputs: Out)\n"
        "  * OneMinus, Sine (inputs: In; outputs: Out); Fresnel (inputs: Power; outputs: Out)\n"
        "Names of parameters and textures are HLSL identifiers and must be unique.\n"
        "Change the graph as the user asks and reply ONLY with the whole updated JSON in a ```json block.\n");
    return prompt;
}

bool MaterialGraphWidget::PasteFromAi(const QString& text)
{
    QString json = text.trimmed();
    static const QRegularExpression block(QStringLiteral(R"(```(?:json)?\s*([\s\S]*?)\s*```)"));
    if (const auto match = block.match(json); match.hasMatch()) json = match.captured(1);
    EditorCore::MaterialGraphModel model;
    if (!model.ImportFromJson(json)) return false;
    const auto& nodes = model.Nodes();
    if (std::count_if(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return n.type == MasterType; }) != 1) return false;
    SetModel(model);
    NotifyStructure();
    return true;
}
