#include "MaterialGraphWidget.h"
#include "EditorLocale.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

namespace
{
    // Определение пинов по типам нод
    QVector<GraphPin> GetNodePins(const QString& type)
    {
        QVector<GraphPin> pins;
        if (type == QStringLiteral("PBRMasterOutput"))
        {
            pins.append({ QStringLiteral("BaseColor"), true, QStringLiteral("float3"), {} });
            pins.append({ QStringLiteral("Metallic"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Roughness"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Normal"), true, QStringLiteral("float3"), {} });
            pins.append({ QStringLiteral("Emissive"), true, QStringLiteral("float3"), {} });
            pins.append({ QStringLiteral("AO"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Alpha"), true, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("ColorParam"))
        {
            pins.append({ QStringLiteral("RGBA"), false, QStringLiteral("float4"), {} });
            pins.append({ QStringLiteral("RGB"), false, QStringLiteral("float3"), {} });
            pins.append({ QStringLiteral("A"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("ScalarParam"))
        {
            pins.append({ QStringLiteral("Value"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("Texture2D"))
        {
            pins.append({ QStringLiteral("UV"), true, QStringLiteral("float2"), {} });
            pins.append({ QStringLiteral("RGBA"), false, QStringLiteral("float4"), {} });
            pins.append({ QStringLiteral("RGB"), false, QStringLiteral("float3"), {} });
            pins.append({ QStringLiteral("A"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("ORMTexture"))
        {
            pins.append({ QStringLiteral("UV"), true, QStringLiteral("float2"), {} });
            pins.append({ QStringLiteral("AO"), false, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Roughness"), false, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Metallic"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("Time"))
        {
            pins.append({ QStringLiteral("Time"), false, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("SineTime"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("UVCoord"))
        {
            pins.append({ QStringLiteral("UV"), false, QStringLiteral("float2"), {} });
        }
        else if (type == QStringLiteral("Panner"))
        {
            pins.append({ QStringLiteral("UV"), true, QStringLiteral("float2"), {} });
            pins.append({ QStringLiteral("UV_Out"), false, QStringLiteral("float2"), {} });
        }
        else if (type == QStringLiteral("Multiply") || type == QStringLiteral("Add"))
        {
            pins.append({ QStringLiteral("A"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("B"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Out"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("Lerp"))
        {
            pins.append({ QStringLiteral("A"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("B"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Alpha"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Out"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("OneMinus") || type == QStringLiteral("Sine"))
        {
            pins.append({ QStringLiteral("In"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Out"), false, QStringLiteral("float"), {} });
        }
        else if (type == QStringLiteral("Fresnel"))
        {
            pins.append({ QStringLiteral("Power"), true, QStringLiteral("float"), {} });
            pins.append({ QStringLiteral("Out"), false, QStringLiteral("float"), {} });
        }
        return pins;
    }

    QColor GetCategoryColor(const QString& type)
    {
        if (type == QStringLiteral("PBRMasterOutput")) return QColor(0xC2, 0x41, 0x0C); // Dark orange/red
        if (type == QStringLiteral("ColorParam") || type == QStringLiteral("ScalarParam")) return QColor(0x15, 0x80, 0x3D); // Green
        if (type == QStringLiteral("Texture2D") || type == QStringLiteral("ORMTexture")) return QColor(0x1D, 0x4E, 0xD8); // Blue
        if (type == QStringLiteral("Multiply") || type == QStringLiteral("Add") || type == QStringLiteral("Lerp") ||
            type == QStringLiteral("OneMinus") || type == QStringLiteral("Sine") || type == QStringLiteral("Fresnel"))
            return QColor(0x7E, 0x22, 0xCE); // Purple
        return QColor(0x0E, 0x74, 0x90); // Cyan (Time, UVCoord, Panner)
    }

    class NodeGraphicsItem : public QGraphicsItem
    {
    public:
        NodeGraphicsItem(const GraphNodeData& data, MaterialGraphWidget* owner)
            : data_(data), owner_(owner)
        {
            setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
            setPos(data.pos);
            ComputeLayout();
        }

        QRectF boundingRect() const override
        {
            return QRectF(0, 0, width_, height_);
        }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
        {
            painter->setRenderHint(QPainter::Antialiasing);

            // 1. Тело ноды
            QPainterPath bodyPath;
            bodyPath.addRoundedRect(0, 0, width_, height_, 6, 6);
            painter->fillPath(bodyPath, QColor(0x18, 0x1B, 0x22));

            // Обводка при выделении
            if (isSelected())
            {
                painter->setPen(QPen(QColor(0x38, 0xBD, 0xF8), 2.0));
                painter->drawPath(bodyPath);
            }
            else
            {
                painter->setPen(QPen(QColor(0x2D, 0x33, 0x42), 1.0));
                painter->drawPath(bodyPath);
            }

            // 2. Шапка
            QPainterPath headerPath;
            headerPath.moveTo(0, 24);
            headerPath.lineTo(0, 6);
            headerPath.arcTo(0, 0, 12, 12, 180, -90);
            headerPath.lineTo(width_ - 6, 0);
            headerPath.arcTo(width_ - 12, 0, 12, 12, 90, -90);
            headerPath.lineTo(width_, 24);
            headerPath.closeSubpath();

            painter->fillPath(headerPath, GetCategoryColor(data_.type));

            // Заголовок
            painter->setPen(Qt::white);
            QFont titleFont = painter->font();
            titleFont.setBold(true);
            titleFont.setPixelSize(11);
            painter->setFont(titleFont);
            painter->drawText(QRectF(8, 2, width_ - 16, 20), Qt::AlignLeft | Qt::AlignVCenter, data_.title);

            // 3. Пины и подписи
            QFont pinFont = painter->font();
            pinFont.setBold(false);
            pinFont.setPixelSize(10);
            painter->setFont(pinFont);

            for (const auto& pin : pins_)
            {
                const QPointF pinCenter = pin.localPos;
                const QRectF circleRect(pinCenter.x() - 4.5, pinCenter.y() - 4.5, 9, 9);

                QColor pinColor = pin.isInput ? QColor(0x38, 0xBD, 0xF8) : QColor(0x34, 0xD3, 0x99);
                painter->setBrush(pinColor);
                painter->setPen(QPen(pinColor.darker(150), 1.0));
                painter->drawEllipse(circleRect);

                painter->setPen(QColor(0xCB, 0xD5, 0xE1));
                if (pin.isInput)
                {
                    painter->drawText(QRectF(pinCenter.x() + 8, pinCenter.y() - 8, width_ / 2 - 12, 16),
                                      Qt::AlignLeft | Qt::AlignVCenter, pin.name);
                }
                else
                {
                    painter->drawText(QRectF(width_ / 2, pinCenter.y() - 8, width_ / 2 - 12, 16),
                                      Qt::AlignRight | Qt::AlignVCenter, pin.name);
                }
            }

            // 4. Дополнительные свойства в теле ноды
            if (data_.type == QStringLiteral("ColorParam"))
            {
                QColor col = data_.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
                QRectF colorBox(width_ / 2 - 20, height_ - 22, 40, 14);
                painter->setBrush(col);
                painter->setPen(QPen(QColor(0x64, 0x74, 0x8B), 1.0));
                painter->drawRoundedRect(colorBox, 3, 3);
            }
            else if (data_.type == QStringLiteral("ScalarParam"))
            {
                double val = data_.properties.value(QStringLiteral("value"), 0.5).toDouble();
                painter->setPen(QColor(0x94, 0xA3, 0xB8));
                painter->drawText(QRectF(8, height_ - 20, width_ - 16, 16), Qt::AlignCenter,
                                  QString::number(val, 'f', 2));
            }
        }

        QPointF PinScenePos(const QString& pinName) const
        {
            for (const auto& pin : pins_)
            {
                if (pin.name == pinName)
                {
                    return scenePos() + pin.localPos;
                }
            }
            return scenePos();
        }

        const GraphNodeData& Data() const { return data_; }
        void UpdateData(const GraphNodeData& data)
        {
            data_ = data;
            ComputeLayout();
            update();
        }

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
        {
            if (change == ItemPositionChange && scene() && owner_)
            {
                owner_->OnNodeMoved(data_.id, value.toPointF());
            }
            return QGraphicsItem::itemChange(change, value);
        }

        void mousePressEvent(QGraphicsSceneMouseEvent* event) override
        {
            for (const auto& pin : pins_)
            {
                if (QLineF(event->pos(), pin.localPos).length() <= 8.0)
                {
                    if (owner_)
                    {
                        owner_->OnPinClicked(data_.id, pin.name, pin.isInput, scenePos() + pin.localPos);
                    }
                    event->accept();
                    return;
                }
            }
            QGraphicsItem::mousePressEvent(event);
        }

        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
        {
            if (owner_) owner_->OnNodeDoubleClicked(data_.id);
            QGraphicsItem::mouseDoubleClickEvent(event);
        }

    private:
        void ComputeLayout()
        {
            pins_ = GetNodePins(data_.type);
            int inCount = 0;
            int outCount = 0;
            for (const auto& pin : pins_)
            {
                if (pin.isInput) inCount++;
                else outCount++;
            }

            width_ = 140;
            if (data_.type == QStringLiteral("PBRMasterOutput")) width_ = 160;

            const int maxRows = std::max(inCount, outCount);
            const int rowHeight = 20;
            const int extraHeight = (data_.type == QStringLiteral("ColorParam") || data_.type == QStringLiteral("ScalarParam")) ? 24 : 8;
            height_ = 28 + std::max(1, maxRows) * rowHeight + extraHeight;

            int currentIn = 0;
            int currentOut = 0;
            for (auto& pin : pins_)
            {
                if (pin.isInput)
                {
                    pin.localPos = QPointF(0, 36 + currentIn * rowHeight);
                    currentIn++;
                }
                else
                {
                    pin.localPos = QPointF(width_, 36 + currentOut * rowHeight);
                    currentOut++;
                }
            }
        }

        GraphNodeData data_;
        MaterialGraphWidget* owner_ = nullptr;
        QVector<GraphPin> pins_;
        qreal width_ = 140;
        qreal height_ = 80;
    };
}

MaterialGraphWidget::MaterialGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
    SetupDefaultMasterNode();
}

MaterialGraphWidget::~MaterialGraphWidget() = default;

void MaterialGraphWidget::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Верхний тулбар графа
    auto* toolbarWidget = new QWidget(this);
    toolbarWidget->setStyleSheet(QStringLiteral("background: #181B20; border-bottom: 1px solid #282C34;"));
    auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(4, 3, 4, 3);
    toolbarLayout->setSpacing(4);

    btnAddNode_ = new QToolButton(toolbarWidget);
    btnAddNode_->setText(Tr("+ Нода ▾", "+ Add Node ▾"));
    btnAddNode_->setPopupMode(QToolButton::InstantPopup);
    btnAddNode_->setStyleSheet(QStringLiteral(
        "QToolButton { background: #2563EB; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QToolButton:hover { background: #1D4ED8; }"
    ));
    menuAddNode_ = new QMenu(btnAddNode_);

    auto* mOutput = menuAddNode_->addMenu(Tr("Выход", "Output"));
    mOutput->addAction(QStringLiteral("PBRMasterOutput"), [this] { AddNode(QStringLiteral("PBRMasterOutput"), view_->mapToScene(view_->viewport()->rect().center())); });

    auto* mParams = menuAddNode_->addMenu(Tr("Параметры", "Parameters"));
    mParams->addAction(Tr("Цвет (ColorParam)", "Color Parameter"), [this] { AddNode(QStringLiteral("ColorParam"), view_->mapToScene(view_->viewport()->rect().center())); });
    mParams->addAction(Tr("Число (ScalarParam)", "Scalar Parameter"), [this] { AddNode(QStringLiteral("ScalarParam"), view_->mapToScene(view_->viewport()->rect().center())); });

    auto* mTextures = menuAddNode_->addMenu(Tr("Текстуры", "Textures"));
    mTextures->addAction(Tr("Текстура 2D (Texture2D)", "Texture 2D"), [this] { AddNode(QStringLiteral("Texture2D"), view_->mapToScene(view_->viewport()->rect().center())); });
    mTextures->addAction(Tr("ORM Карта (ORMTexture)", "ORM Texture (AO/R/M)"), [this] { AddNode(QStringLiteral("ORMTexture"), view_->mapToScene(view_->viewport()->rect().center())); });

    auto* mCoord = menuAddNode_->addMenu(Tr("Координаты и Время", "Coordinates & Time"));
    mCoord->addAction(QStringLiteral("Time"), [this] { AddNode(QStringLiteral("Time"), view_->mapToScene(view_->viewport()->rect().center())); });
    mCoord->addAction(QStringLiteral("UVCoord"), [this] { AddNode(QStringLiteral("UVCoord"), view_->mapToScene(view_->viewport()->rect().center())); });
    mCoord->addAction(QStringLiteral("Panner"), [this] { AddNode(QStringLiteral("Panner"), view_->mapToScene(view_->viewport()->rect().center())); });

    auto* mMath = menuAddNode_->addMenu(Tr("Математика", "Math"));
    mMath->addAction(QStringLiteral("Multiply (*)"), [this] { AddNode(QStringLiteral("Multiply"), view_->mapToScene(view_->viewport()->rect().center())); });
    mMath->addAction(QStringLiteral("Add (+)"), [this] { AddNode(QStringLiteral("Add"), view_->mapToScene(view_->viewport()->rect().center())); });
    mMath->addAction(QStringLiteral("Lerp"), [this] { AddNode(QStringLiteral("Lerp"), view_->mapToScene(view_->viewport()->rect().center())); });
    mMath->addAction(QStringLiteral("OneMinus (1 - x)"), [this] { AddNode(QStringLiteral("OneMinus"), view_->mapToScene(view_->viewport()->rect().center())); });
    mMath->addAction(QStringLiteral("Sine"), [this] { AddNode(QStringLiteral("Sine"), view_->mapToScene(view_->viewport()->rect().center())); });
    mMath->addAction(QStringLiteral("Fresnel"), [this] { AddNode(QStringLiteral("Fresnel"), view_->mapToScene(view_->viewport()->rect().center())); });

    btnAddNode_->setMenu(menuAddNode_);
    toolbarLayout->addWidget(btnAddNode_);

    btnCompileHlsl_ = new QPushButton(Tr("⚡ Скомпилировать граф в HLSL", "⚡ Compile Graph -> HLSL"), toolbarWidget);
    btnCompileHlsl_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #10B981; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #059669; }"
    ));
    connect(btnCompileHlsl_, &QPushButton::clicked, this, [this] { CompileGraphToHlsl(); });
    toolbarLayout->addWidget(btnCompileHlsl_);

    toolbarLayout->addSpacing(10);

    // AI Workflow
    btnCopyAi_ = new QPushButton(Tr("📋 Copy Graph + Prompt for AI", "📋 Copy Graph + Prompt for AI"), toolbarWidget);
    btnCopyAi_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #8B5CF6; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #7C3AED; }"
    ));
    connect(btnCopyAi_, &QPushButton::clicked, this, [this] { CopyGraphForAi(); });
    toolbarLayout->addWidget(btnCopyAi_);

    btnPasteAi_ = new QPushButton(Tr("📥 Paste Graph from AI", "📥 Paste Graph from AI"), toolbarWidget);
    btnPasteAi_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #6366F1; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #4F46E5; }"
    ));
    connect(btnPasteAi_, &QPushButton::clicked, this, [this] { PasteGraphFromAi(); });
    toolbarLayout->addWidget(btnPasteAi_);

    toolbarLayout->addStretch(1);

    btnPresets_ = new QToolButton(toolbarWidget);
    btnPresets_->setText(Tr("🎲 Готовые графы ▾", "🎲 Presets ▾"));
    btnPresets_->setPopupMode(QToolButton::InstantPopup);
    btnPresets_->setStyleSheet(QStringLiteral(
        "QToolButton { background: #1E293B; color: #E2E8F0; border: 1px solid #334155; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QToolButton:hover { background: #334155; }"
    ));
    menuPresets_ = new QMenu(btnPresets_);
    menuPresets_->addAction(QStringLiteral("Standard PBR + ORM"), [this] { LoadPreset(QStringLiteral("Standard PBR + ORM")); });
    menuPresets_->addAction(QStringLiteral("Animated Neon / Cyber"), [this] { LoadPreset(QStringLiteral("Animated Neon / Cyber")); });
    menuPresets_->addAction(QStringLiteral("Gold Metallic"), [this] { LoadPreset(QStringLiteral("Gold Metallic")); });
    menuPresets_->addAction(QStringLiteral("Scrolling Energy"), [this] { LoadPreset(QStringLiteral("Scrolling Energy")); });
    btnPresets_->setMenu(menuPresets_);
    toolbarLayout->addWidget(btnPresets_);

    mainLayout->addWidget(toolbarWidget);

    // 2. Сцена и Вьюпорт
    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-5000, -5000, 10000, 10000);

    view_ = new QGraphicsView(scene_, this);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setBackgroundBrush(QColor(0x12, 0x14, 0x18));
    view_->viewport()->installEventFilter(this);

    mainLayout->addWidget(view_, 1);
}

void MaterialGraphWidget::SetupDefaultMasterNode()
{
    nodes_.clear();
    connections_.clear();
    nextNodeId_ = 1;

    // Master Output
    GraphNodeData master;
    master.id = nextNodeId_++;
    master.type = QStringLiteral("PBRMasterOutput");
    master.title = QStringLiteral("PBR Master Output");
    master.pos = QPointF(550, 180);
    nodes_.append(master);

    // ColorParam
    GraphNodeData colorNode;
    colorNode.id = nextNodeId_++;
    colorNode.type = QStringLiteral("ColorParam");
    colorNode.title = QStringLiteral("BaseColor");
    colorNode.pos = QPointF(200, 120);
    colorNode.properties[QStringLiteral("name")] = QStringLiteral("BaseColor");
    colorNode.properties[QStringLiteral("color")] = QColor(220, 220, 225);
    nodes_.append(colorNode);

    // ScalarParam Roughness
    GraphNodeData roughNode;
    roughNode.id = nextNodeId_++;
    roughNode.type = QStringLiteral("ScalarParam");
    roughNode.title = QStringLiteral("Roughness");
    roughNode.pos = QPointF(200, 260);
    roughNode.properties[QStringLiteral("name")] = QStringLiteral("Roughness");
    roughNode.properties[QStringLiteral("value")] = 0.45;
    nodes_.append(roughNode);

    // ScalarParam Metallic
    GraphNodeData metalNode;
    metalNode.id = nextNodeId_++;
    metalNode.type = QStringLiteral("ScalarParam");
    metalNode.title = QStringLiteral("Metallic");
    metalNode.pos = QPointF(200, 360);
    metalNode.properties[QStringLiteral("name")] = QStringLiteral("Metallic");
    metalNode.properties[QStringLiteral("value")] = 0.0;
    nodes_.append(metalNode);

    // Connections
    connections_.append({ colorNode.id, QStringLiteral("RGB"), master.id, QStringLiteral("BaseColor") });
    connections_.append({ roughNode.id, QStringLiteral("Value"), master.id, QStringLiteral("Roughness") });
    connections_.append({ metalNode.id, QStringLiteral("Value"), master.id, QStringLiteral("Metallic") });

    RebuildScene();
}

void MaterialGraphWidget::AddNode(const QString& type, const QPointF& pos, const QVariantMap& props)
{
    GraphNodeData node;
    node.id = nextNodeId_++;
    node.type = type;
    node.title = type;
    node.pos = pos;
    node.properties = props;

    if (type == QStringLiteral("ColorParam"))
    {
        node.title = QStringLiteral("Color_%1").arg(node.id);
        if (!node.properties.contains(QStringLiteral("name"))) node.properties[QStringLiteral("name")] = node.title;
        if (!node.properties.contains(QStringLiteral("color"))) node.properties[QStringLiteral("color")] = QColor(Qt::white);
    }
    else if (type == QStringLiteral("ScalarParam"))
    {
        node.title = QStringLiteral("Param_%1").arg(node.id);
        if (!node.properties.contains(QStringLiteral("name"))) node.properties[QStringLiteral("name")] = node.title;
        if (!node.properties.contains(QStringLiteral("value"))) node.properties[QStringLiteral("value")] = 0.5;
    }
    else if (type == QStringLiteral("Texture2D"))
    {
        node.title = QStringLiteral("Texture2D (BaseMap)");
        if (!node.properties.contains(QStringLiteral("textureName"))) node.properties[QStringLiteral("textureName")] = QStringLiteral("BaseMap");
        if (!node.properties.contains(QStringLiteral("slot"))) node.properties[QStringLiteral("slot")] = 6;
    }
    else if (type == QStringLiteral("ORMTexture"))
    {
        node.title = QStringLiteral("ORM Map (AO/R/M)");
        if (!node.properties.contains(QStringLiteral("textureName"))) node.properties[QStringLiteral("textureName")] = QStringLiteral("OrmMap");
        if (!node.properties.contains(QStringLiteral("slot"))) node.properties[QStringLiteral("slot")] = 7;
    }
    else if (type == QStringLiteral("UVCoord"))
    {
        if (!node.properties.contains(QStringLiteral("tilingX"))) node.properties[QStringLiteral("tilingX")] = 1.0;
        if (!node.properties.contains(QStringLiteral("tilingY"))) node.properties[QStringLiteral("tilingY")] = 1.0;
    }
    else if (type == QStringLiteral("Panner"))
    {
        if (!node.properties.contains(QStringLiteral("speedX"))) node.properties[QStringLiteral("speedX")] = 0.5;
        if (!node.properties.contains(QStringLiteral("speedY"))) node.properties[QStringLiteral("speedY")] = 0.0;
    }

    nodes_.append(node);
    RebuildScene();
}

void MaterialGraphWidget::DeleteSelected()
{
    QList<QGraphicsItem*> selected = scene_->selectedItems();
    for (auto* item : selected)
    {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item))
        {
            if (nodeItem->Data().type == QStringLiteral("PBRMasterOutput"))
                continue; // Не удалять главный выход

            const int id = nodeItem->Data().id;
            for (int i = nodes_.size() - 1; i >= 0; --i)
            {
                if (nodes_[i].id == id) nodes_.removeAt(i);
            }
            for (int i = connections_.size() - 1; i >= 0; --i)
            {
                if (connections_[i].fromNode == id || connections_[i].toNode == id)
                    connections_.removeAt(i);
            }
        }
    }
    RebuildScene();
}

void MaterialGraphWidget::RebuildScene()
{
    scene_->clear();

    for (const auto& node : nodes_)
    {
        auto* item = new NodeGraphicsItem(node, this);
        scene_->addItem(item);
    }

    UpdateConnectionsVisual();
}

void MaterialGraphWidget::UpdateConnectionsVisual()
{
    // Очистить существующие линии связей
    const auto items = scene_->items();
    for (auto* item : items)
    {
        if (item->type() == QGraphicsPathItem::Type)
        {
            scene_->removeItem(item);
            delete item;
        }
    }

    // Построить словарь элементов
    QHash<int, NodeGraphicsItem*> nodeMap;
    for (auto* item : scene_->items())
    {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item))
        {
            nodeMap[nodeItem->Data().id] = nodeItem;
        }
    }

    // Нарисовать плавные кривые Безье
    for (const auto& conn : connections_)
    {
        if (!nodeMap.contains(conn.fromNode) || !nodeMap.contains(conn.toNode))
            continue;

        const QPointF p1 = nodeMap[conn.fromNode]->PinScenePos(conn.fromPin);
        const QPointF p2 = nodeMap[conn.toNode]->PinScenePos(conn.toPin);

        QPainterPath path;
        path.moveTo(p1);
        const qreal dx = std::abs(p2.x() - p1.x()) * 0.5;
        path.cubicTo(p1.x() + std::max(40.0, dx), p1.y(),
                     p2.x() - std::max(40.0, dx), p2.y(),
                     p2.x(), p2.y());

        auto* pathItem = new QGraphicsPathItem(path);
        pathItem->setPen(QPen(QColor(0x38, 0xBD, 0xF8), 2.5));
        pathItem->setZValue(-1);
        scene_->addItem(pathItem);
    }
}

void MaterialGraphWidget::OnNodeMoved(int nodeId, const QPointF& newPos)
{
    for (auto& n : nodes_)
    {
        if (n.id == nodeId)
        {
            n.pos = newPos;
            break;
        }
    }
    UpdateConnectionsVisual();
}

void MaterialGraphWidget::OnNodeDoubleClicked(int nodeId)
{
    for (auto& n : nodes_)
    {
        if (n.id == nodeId)
        {
            if (n.type == QStringLiteral("ColorParam"))
            {
                QColor cur = n.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
                QColor picked = QColorDialog::getColor(cur, this, Tr("Выбор цвета", "Choose Color"));
                if (picked.isValid())
                {
                    n.properties[QStringLiteral("color")] = picked;
                    RebuildScene();
                    CompileGraphToHlsl();
                }
            }
            else if (n.type == QStringLiteral("ScalarParam"))
            {
                bool ok = false;
                double cur = n.properties.value(QStringLiteral("value"), 0.5).toDouble();
                double val = QInputDialog::getDouble(this, Tr("Параметр", "Scalar Parameter"),
                    Tr("Значение:", "Value:"), cur, -10000.0, 10000.0, 3, &ok);
                if (ok)
                {
                    n.properties[QStringLiteral("value")] = val;
                    RebuildScene();
                    CompileGraphToHlsl();
                }
            }
            break;
        }
    }
}

void MaterialGraphWidget::OnPinClicked(int nodeId, const QString& pinName, bool isInput, const QPointF& scenePos)
{
    if (!isConnecting_)
    {
        isConnecting_ = true;
        connectStartNodeId_ = nodeId;
        connectStartPinName_ = pinName;
        connectStartIsInput_ = isInput;
        connectStartPos_ = scenePos;
    }
    else
    {
        if (connectStartNodeId_ != nodeId && connectStartIsInput_ != isInput)
        {
            int fromNode = isInput ? connectStartNodeId_ : nodeId;
            QString fromPin = isInput ? connectStartPinName_ : pinName;
            int toNode = isInput ? nodeId : connectStartNodeId_;
            QString toPin = isInput ? pinName : connectStartPinName_;

            // Удалить существующие соединения к этому же входному пину
            for (int i = connections_.size() - 1; i >= 0; --i)
            {
                if (connections_[i].toNode == toNode && connections_[i].toPin == toPin)
                {
                    connections_.removeAt(i);
                }
            }

            connections_.append({ fromNode, fromPin, toNode, toPin });
            UpdateConnectionsVisual();
        }
        isConnecting_ = false;
    }
}

void MaterialGraphWidget::OnCanvasContextMenu(const QPoint& screenPos, const QPointF& scenePos)
{
    QMenu menu(this);
    auto* actOutput = menu.addAction(QStringLiteral("PBR Master Output"));
    auto* actColor = menu.addAction(Tr("Цвет (ColorParam)", "Color Parameter"));
    auto* actScalar = menu.addAction(Tr("Число (ScalarParam)", "Scalar Parameter"));
    auto* actTex = menu.addAction(Tr("Текстура 2D (Texture2D)", "Texture 2D"));
    auto* actOrm = menu.addAction(Tr("ORM Карта (ORMTexture)", "ORM Texture (AO/R/M)"));
    menu.addSeparator();
    auto* actTime = menu.addAction(QStringLiteral("Time"));
    auto* actUv = menu.addAction(QStringLiteral("UVCoord"));
    auto* actPanner = menu.addAction(QStringLiteral("Panner"));
    menu.addSeparator();
    auto* actMul = menu.addAction(QStringLiteral("Multiply (*)"));
    auto* actAdd = menu.addAction(QStringLiteral("Add (+)"));
    auto* actLerp = menu.addAction(QStringLiteral("Lerp"));
    auto* actOneMinus = menu.addAction(QStringLiteral("OneMinus (1 - x)"));
    auto* actSine = menu.addAction(QStringLiteral("Sine"));
    auto* actFresnel = menu.addAction(QStringLiteral("Fresnel"));

    QAction* picked = menu.exec(screenPos);
    if (!picked) return;

    if (picked == actOutput) AddNode(QStringLiteral("PBRMasterOutput"), scenePos);
    else if (picked == actColor) AddNode(QStringLiteral("ColorParam"), scenePos);
    else if (picked == actScalar) AddNode(QStringLiteral("ScalarParam"), scenePos);
    else if (picked == actTex) AddNode(QStringLiteral("Texture2D"), scenePos);
    else if (picked == actOrm) AddNode(QStringLiteral("ORMTexture"), scenePos);
    else if (picked == actTime) AddNode(QStringLiteral("Time"), scenePos);
    else if (picked == actUv) AddNode(QStringLiteral("UVCoord"), scenePos);
    else if (picked == actPanner) AddNode(QStringLiteral("Panner"), scenePos);
    else if (picked == actMul) AddNode(QStringLiteral("Multiply"), scenePos);
    else if (picked == actAdd) AddNode(QStringLiteral("Add"), scenePos);
    else if (picked == actLerp) AddNode(QStringLiteral("Lerp"), scenePos);
    else if (picked == actOneMinus) AddNode(QStringLiteral("OneMinus"), scenePos);
    else if (picked == actSine) AddNode(QStringLiteral("Sine"), scenePos);
    else if (picked == actFresnel) AddNode(QStringLiteral("Fresnel"), scenePos);
}

bool MaterialGraphWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view_->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::MiddleButton || me->button() == Qt::RightButton)
            {
                isPanning_ = true;
                panStartPos_ = me->pos();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (isPanning_)
            {
                QPoint delta = me->pos() - panStartPos_;
                panStartPos_ = me->pos();
                view_->horizontalScrollBar()->setValue(view_->horizontalScrollBar()->value() - delta.x());
                view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->value() - delta.y());
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (isPanning_)
            {
                isPanning_ = false;
                if (me->button() == Qt::RightButton && (me->pos() - panStartPos_).manhattanLength() < 4)
                {
                    OnCanvasContextMenu(me->globalPosition().toPoint(), view_->mapToScene(me->pos()));
                }
                return true;
            }
        }
        else if (event->type() == QEvent::Wheel)
        {
            auto* we = static_cast<QWheelEvent*>(event);
            const double factor = (we->angleDelta().y() > 0) ? 1.15 : 0.85;
            view_->scale(factor, factor);
            return true;
        }
        else if (event->type() == QEvent::KeyPress)
        {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace)
            {
                DeleteSelected();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MaterialGraphWidget::SetMaterialPath(const QString& matPath, const QString& hlslPath)
{
    materialPath_ = matPath;
    shaderPath_ = hlslPath;

    QString graphFile = matPath + QStringLiteral(".graph.json");
    if (QFile::exists(graphFile))
    {
        LoadGraphFromFile(graphFile);
    }
    else
    {
        SetupDefaultMasterNode();
    }
}

void MaterialGraphWidget::LoadGraphFromFile(const QString& graphPath)
{
    QFile file(graphPath);
    if (file.open(QIODevice::ReadOnly))
    {
        ImportGraphFromJson(QString::fromUtf8(file.readAll()));
    }
}

void MaterialGraphWidget::SaveGraphToFile(const QString& graphPath)
{
    QFile file(graphPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(ExportGraphToJson().toUtf8());
    }
}

QString MaterialGraphWidget::ExportGraphToJson() const
{
    QJsonObject root;
    root[QStringLiteral("version")] = 1;

    QJsonArray nodesArr;
    for (const auto& n : nodes_)
    {
        QJsonObject no;
        no[QStringLiteral("id")] = n.id;
        no[QStringLiteral("type")] = n.type;
        no[QStringLiteral("title")] = n.title;
        no[QStringLiteral("x")] = n.pos.x();
        no[QStringLiteral("y")] = n.pos.y();

        QJsonObject props;
        for (auto it = n.properties.begin(); it != n.properties.end(); ++it)
        {
            if (it.value().canConvert<QColor>())
            {
                QColor c = it.value().value<QColor>();
                QJsonArray colArr = { c.redF(), c.greenF(), c.blueF(), c.alphaF() };
                props[it.key()] = colArr;
            }
            else
            {
                props[it.key()] = QJsonValue::fromVariant(it.value());
            }
        }
        no[QStringLiteral("properties")] = props;
        nodesArr.append(no);
    }
    root[QStringLiteral("nodes")] = nodesArr;

    QJsonArray connArr;
    for (const auto& c : connections_)
    {
        QJsonObject co;
        co[QStringLiteral("fromNode")] = c.fromNode;
        co[QStringLiteral("fromPin")] = c.fromPin;
        co[QStringLiteral("toNode")] = c.toNode;
        co[QStringLiteral("toPin")] = c.toPin;
        connArr.append(co);
    }
    root[QStringLiteral("connections")] = connArr;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool MaterialGraphWidget::ImportGraphFromJson(const QString& jsonString)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    nodes_.clear();
    connections_.clear();
    nextNodeId_ = 1;

    QJsonArray nodesArr = root[QStringLiteral("nodes")].toArray();
    for (const auto& val : nodesArr)
    {
        QJsonObject no = val.toObject();
        GraphNodeData n;
        n.id = no[QStringLiteral("id")].toInt();
        n.type = no[QStringLiteral("type")].toString();
        n.title = no[QStringLiteral("title")].toString();
        n.pos = QPointF(no[QStringLiteral("x")].toDouble(), no[QStringLiteral("y")].toDouble());

        QJsonObject props = no[QStringLiteral("properties")].toObject();
        for (auto it = props.begin(); it != props.end(); ++it)
        {
            if (it.value().isArray())
            {
                QJsonArray colArr = it.value().toArray();
                if (colArr.size() == 4)
                {
                    n.properties[it.key()] = QColor::fromRgbF(colArr[0].toDouble(), colArr[1].toDouble(), colArr[2].toDouble(), colArr[3].toDouble());
                    continue;
                }
            }
            n.properties[it.key()] = it.value().toVariant();
        }

        nodes_.append(n);
        if (n.id >= nextNodeId_) nextNodeId_ = n.id + 1;
    }

    QJsonArray connArr = root[QStringLiteral("connections")].toArray();
    for (const auto& val : connArr)
    {
        QJsonObject co = val.toObject();
        GraphConnectionData c;
        c.fromNode = co[QStringLiteral("fromNode")].toInt();
        c.fromPin = co[QStringLiteral("fromPin")].toString();
        c.toNode = co[QStringLiteral("toNode")].toInt();
        c.toPin = co[QStringLiteral("toPin")].toString();
        connections_.append(c);
    }

    RebuildScene();
    return true;
}

QString MaterialGraphWidget::CompileGraphToHlsl()
{
    QStringList cbufferParams;
    QStringList textureDecls;
    bool hasEmissive = false;

    // Сбор параметров и текстур
    for (const auto& n : nodes_)
    {
        if (n.type == QStringLiteral("ColorParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color_%1").arg(n.id)).toString();
            QColor col = n.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
            cbufferParams << QStringLiteral("    float4 %1 = float4(%2f, %3f, %4f, %5f);")
                                .arg(name)
                                .arg(col.redF(), 0, 'f', 4)
                                .arg(col.greenF(), 0, 'f', 4)
                                .arg(col.blueF(), 0, 'f', 4)
                                .arg(col.alphaF(), 0, 'f', 4);
        }
        else if (n.type == QStringLiteral("ScalarParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Param_%1").arg(n.id)).toString();
            double val = n.properties.value(QStringLiteral("value"), 0.5).toDouble();
            cbufferParams << QStringLiteral("    float %1 = %2f;").arg(name).arg(val, 0, 'f', 4);
        }
        else if (n.type == QStringLiteral("Texture2D"))
        {
            QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("BaseMap")).toString();
            int slot = n.properties.value(QStringLiteral("slot"), 6).toInt();
            textureDecls << QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot);
        }
        else if (n.type == QStringLiteral("ORMTexture"))
        {
            QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("OrmMap")).toString();
            int slot = n.properties.value(QStringLiteral("slot"), 7).toInt();
            textureDecls << QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot);
        }
    }

    // Рекурсивный генератор выражений для пина
    std::function<QString(int nodeId, const QString& pinName)> evalPin;
    evalPin = [this, &evalPin, &hasEmissive](int nodeId, const QString& pinName) -> QString
    {
        for (const auto& n : nodes_)
        {
            if (n.id == nodeId)
            {
                if (n.type == QStringLiteral("ColorParam"))
                {
                    QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color")).toString();
                    if (pinName == QStringLiteral("RGB")) return name + QStringLiteral(".rgb");
                    if (pinName == QStringLiteral("A")) return name + QStringLiteral(".a");
                    return name;
                }
                if (n.type == QStringLiteral("ScalarParam"))
                {
                    return n.properties.value(QStringLiteral("name"), QStringLiteral("Param")).toString();
                }
                if (n.type == QStringLiteral("Time"))
                {
                    if (pinName == QStringLiteral("SineTime")) return QStringLiteral("sin(i.Time)");
                    return QStringLiteral("i.Time");
                }
                if (n.type == QStringLiteral("UVCoord"))
                {
                    double tx = n.properties.value(QStringLiteral("tilingX"), 1.0).toDouble();
                    double ty = n.properties.value(QStringLiteral("tilingY"), 1.0).toDouble();
                    return QStringLiteral("(i.UV * float2(%1f, %2f))").arg(tx, 0, 'f', 2).arg(ty, 0, 'f', 2);
                }
                if (n.type == QStringLiteral("Texture2D"))
                {
                    QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("BaseMap")).toString();
                    QString uvExpr = QStringLiteral("i.UV");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV"))
                        {
                            uvExpr = evalPin(c.fromNode, c.fromPin);
                            break;
                        }
                    }
                    QString sampleExpr = QStringLiteral("%1.Sample(MaterialSampler, %2)").arg(texName, uvExpr);
                    if (pinName == QStringLiteral("RGB")) return sampleExpr + QStringLiteral(".rgb");
                    if (pinName == QStringLiteral("A")) return sampleExpr + QStringLiteral(".a");
                    return sampleExpr;
                }
                if (n.type == QStringLiteral("ORMTexture"))
                {
                    QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("OrmMap")).toString();
                    QString uvExpr = QStringLiteral("i.UV");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV"))
                        {
                            uvExpr = evalPin(c.fromNode, c.fromPin);
                            break;
                        }
                    }
                    QString sampleExpr = QStringLiteral("%1.Sample(MaterialSampler, %2)").arg(texName, uvExpr);
                    if (pinName == QStringLiteral("AO")) return sampleExpr + QStringLiteral(".r");
                    if (pinName == QStringLiteral("Roughness")) return sampleExpr + QStringLiteral(".g");
                    if (pinName == QStringLiteral("Metallic")) return sampleExpr + QStringLiteral(".b");
                    return sampleExpr;
                }
                if (n.type == QStringLiteral("Multiply"))
                {
                    QString a = QStringLiteral("1.0f"), b = QStringLiteral("1.0f");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(%1 * %2)").arg(a, b);
                }
                if (n.type == QStringLiteral("Add"))
                {
                    QString a = QStringLiteral("0.0f"), b = QStringLiteral("0.0f");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(%1 + %2)").arg(a, b);
                }
                if (n.type == QStringLiteral("Lerp"))
                {
                    QString a = QStringLiteral("0.0f"), b = QStringLiteral("1.0f"), alpha = QStringLiteral("0.5f");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("Alpha")) alpha = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("lerp(%1, %2, %3)").arg(a, b, alpha);
                }
                if (n.type == QStringLiteral("OneMinus"))
                {
                    QString inVal = QStringLiteral("0.0f");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("In")) inVal = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(1.0f - %1)").arg(inVal);
                }
                if (n.type == QStringLiteral("Sine"))
                {
                    QString inVal = QStringLiteral("i.Time");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("In")) inVal = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("sin(%1)").arg(inVal);
                }
                if (n.type == QStringLiteral("Fresnel"))
                {
                    QString power = QStringLiteral("3.0f");
                    for (const auto& c : connections_)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("Power")) power = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), %1)").arg(power);
                }
                break;
            }
        }
        return QStringLiteral("0.0f");
    };

    // Находим Master Node
    int masterId = 0;
    for (const auto& n : nodes_)
    {
        if (n.type == QStringLiteral("PBRMasterOutput"))
        {
            masterId = n.id;
            break;
        }
    }

    QString baseColorExpr = QStringLiteral("float3(0.8, 0.8, 0.8)");
    QString metallicExpr = QStringLiteral("0.0f");
    QString roughnessExpr = QStringLiteral("0.5f");
    QString normalExpr;
    QString emissiveExpr;
    QString aoExpr = QStringLiteral("1.0f");
    QString alphaExpr = QStringLiteral("1.0f");

    if (masterId != 0)
    {
        for (const auto& c : connections_)
        {
            if (c.toNode == masterId)
            {
                if (c.toPin == QStringLiteral("BaseColor")) baseColorExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Metallic")) metallicExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Roughness")) roughnessExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Normal")) normalExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Emissive"))
                {
                    emissiveExpr = evalPin(c.fromNode, c.fromPin);
                    hasEmissive = true;
                }
                else if (c.toPin == QStringLiteral("AO")) aoExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Alpha")) alphaExpr = evalPin(c.fromNode, c.fromPin);
            }
        }
    }

    QString hlsl;
    hlsl += QStringLiteral("// =========================================================\n");
    hlsl += QStringLiteral("// Generated by PainEngine Material Node Graph\n");
    hlsl += QStringLiteral("// =========================================================\n\n");

    if (hasEmissive)
    {
        hlsl += QStringLiteral("#define MATERIAL_EMISSIVE 1\n\n");
    }

    if (!cbufferParams.isEmpty())
    {
        hlsl += QStringLiteral("cbuffer MaterialParams : register(b1)\n{\n");
        hlsl += cbufferParams.join(QStringLiteral("\n")) + QStringLiteral("\n");
        hlsl += QStringLiteral("};\n\n");
    }

    if (!textureDecls.isEmpty())
    {
        hlsl += textureDecls.join(QStringLiteral("\n")) + QStringLiteral("\n\n");
    }

    hlsl += QStringLiteral("void Surface(MaterialInput i, inout MaterialOutput o)\n{\n");
    hlsl += QStringLiteral("    o.Albedo = saturate(%1);\n").arg(baseColorExpr);
    hlsl += QStringLiteral("    o.SpecularWeight = saturate(1.0f - (%1));\n").arg(roughnessExpr);
    hlsl += QStringLiteral("    o.AmbientModel = saturate(%1);\n").arg(aoExpr);
    hlsl += QStringLiteral("    o.Alpha = saturate(%1);\n").arg(alphaExpr);

    if (!normalExpr.isEmpty())
    {
        hlsl += QStringLiteral("    o.Normal = %1;\n").arg(normalExpr);
    }
    if (!emissiveExpr.isEmpty())
    {
        hlsl += QStringLiteral("    o.Emissive = %1;\n").arg(emissiveExpr);
    }

    hlsl += QStringLiteral("}\n");

    // Сохранить json графа
    if (!materialPath_.isEmpty())
    {
        SaveGraphToFile(materialPath_ + QStringLiteral(".graph.json"));
    }

    // Оповестить окно редактора материалов
    if (onHlslGenerated_)
    {
        onHlslGenerated_(hlsl);
    }

    return hlsl;
}

void MaterialGraphWidget::CopyGraphForAi()
{
    const QString jsonStr = ExportGraphToJson();
    QString prompt;
    prompt += QStringLiteral("You are an expert shader engineer for PainEngine (DirectX 11 HLSL PBR material system).\n");
    prompt += QStringLiteral("Here is the JSON representation of the current Material Graph:\n\n");
    prompt += QStringLiteral("```json\n%1\n```\n\n").arg(jsonStr);
    prompt += QStringLiteral("PainEngine Material Graph Specification:\n");
    prompt += QStringLiteral("- Nodes have an 'id', 'type', 'title', 'x', 'y', and 'properties'.\n");
    prompt += QStringLiteral("- Node Types and Pins:\n");
    prompt += QStringLiteral("  * PBRMasterOutput (inputs: BaseColor, Metallic, Roughness, Normal, Emissive, AO, Alpha)\n");
    prompt += QStringLiteral("  * ColorParam (props: name, color: [r,g,b,a]; outputs: RGBA, RGB, A)\n");
    prompt += QStringLiteral("  * ScalarParam (props: name, value; outputs: Value)\n");
    prompt += QStringLiteral("  * Texture2D (props: textureName, slot; inputs: UV; outputs: RGBA, RGB, A)\n");
    prompt += QStringLiteral("  * ORMTexture (props: textureName, slot; inputs: UV; outputs: AO, Roughness, Metallic)\n");
    prompt += QStringLiteral("  * Time (outputs: Time, SineTime)\n");
    prompt += QStringLiteral("  * UVCoord (props: tilingX, tilingY; outputs: UV)\n");
    prompt += QStringLiteral("  * Panner (props: speedX, speedY; inputs: UV; outputs: UV_Out)\n");
    prompt += QStringLiteral("  * Multiply, Add (inputs: A, B; outputs: Out)\n");
    prompt += QStringLiteral("  * Lerp (inputs: A, B, Alpha; outputs: Out)\n");
    prompt += QStringLiteral("  * OneMinus, Sine (inputs: In; outputs: Out)\n");
    prompt += QStringLiteral("  * Fresnel (inputs: Power; outputs: Out)\n\n");
    prompt += QStringLiteral("Please modify or create nodes and connections as requested by the user, and reply ONLY with the updated JSON code block ```json ... ```.\n");

    QGuiApplication::clipboard()->setText(prompt);
    QMessageBox::information(this, Tr("Граф скопирован для ИИ", "Graph Copied for AI"),
        Tr("Промпт со схемой графа и текущим JSON скопирован в буфер обмена!\n"
           "Вставьте его в ChatGPT / Claude / Gemini, запросите нужный эффект, а затем нажмите «Paste Graph from AI».",
           "Prompt with graph schema and current JSON copied to clipboard!\n"
           "Paste it into ChatGPT / Claude / Gemini, request desired shader effect, and click 'Paste Graph from AI'."));
}

void MaterialGraphWidget::PasteGraphFromAi()
{
    QString text = QGuiApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) return;

    // Извлечение JSON из markdown-блока
    static const QRegularExpression codeBlockRegex(QStringLiteral(R"(```(?:json)?\s*([\s\S]*?)\s*```)"));
    const auto match = codeBlockRegex.match(text);
    if (match.hasMatch())
    {
        text = match.captured(1).trimmed();
    }

    if (ImportGraphFromJson(text))
    {
        CompileGraphToHlsl();
        QMessageBox::information(this, Tr("Импорт графа от ИИ", "AI Graph Imported"),
            Tr("Граф успешно загружен, ноды перестроены и HLSL скомпилирован!",
               "Graph successfully loaded, nodes reconstructed, and HLSL recompiled!"));
    }
    else
    {
        QMessageBox::warning(this, Tr("Ошибка импорта", "Import Error"),
            Tr("Не удалось распарсить JSON графа из буфера обмена.",
               "Failed to parse valid graph JSON from clipboard."));
    }
}

void MaterialGraphWidget::LoadPreset(const QString& presetName)
{
    nodes_.clear();
    connections_.clear();
    nextNodeId_ = 1;

    if (presetName == QStringLiteral("Standard PBR + ORM"))
    {
        GraphNodeData master{ nextNodeId_++, QStringLiteral("PBRMasterOutput"), QStringLiteral("PBR Master Output"), QPointF(600, 180), {} };
        nodes_.append(master);

        GraphNodeData col{ nextNodeId_++, QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(100, 100),
            { { QStringLiteral("name"), QStringLiteral("BaseColor") }, { QStringLiteral("color"), QColor(220, 220, 225) } } };
        nodes_.append(col);

        GraphNodeData tex{ nextNodeId_++, QStringLiteral("Texture2D"), QStringLiteral("BaseMap"), QPointF(-150, 100),
            { { QStringLiteral("textureName"), QStringLiteral("BaseMap") }, { QStringLiteral("slot"), 6 } } };
        nodes_.append(tex);

        GraphNodeData mul{ nextNodeId_++, QStringLiteral("Multiply"), QStringLiteral("Multiply"), QPointF(350, 120), {} };
        nodes_.append(mul);

        GraphNodeData orm{ nextNodeId_++, QStringLiteral("ORMTexture"), QStringLiteral("OrmMap"), QPointF(100, 260),
            { { QStringLiteral("textureName"), QStringLiteral("OrmMap") }, { QStringLiteral("slot"), 7 } } };
        nodes_.append(orm);

        connections_.append({ tex.id, QStringLiteral("RGB"), mul.id, QStringLiteral("A") });
        connections_.append({ col.id, QStringLiteral("RGB"), mul.id, QStringLiteral("B") });
        connections_.append({ mul.id, QStringLiteral("Out"), master.id, QStringLiteral("BaseColor") });
        connections_.append({ orm.id, QStringLiteral("AO"), master.id, QStringLiteral("AO") });
        connections_.append({ orm.id, QStringLiteral("Roughness"), master.id, QStringLiteral("Roughness") });
        connections_.append({ orm.id, QStringLiteral("Metallic"), master.id, QStringLiteral("Metallic") });
    }
    else if (presetName == QStringLiteral("Animated Neon / Cyber"))
    {
        GraphNodeData master{ nextNodeId_++, QStringLiteral("PBRMasterOutput"), QStringLiteral("PBR Master Output"), QPointF(600, 180), {} };
        nodes_.append(master);

        GraphNodeData darkCol{ nextNodeId_++, QStringLiteral("ColorParam"), QStringLiteral("DarkBase"), QPointF(200, 80),
            { { QStringLiteral("name"), QStringLiteral("DarkBase") }, { QStringLiteral("color"), QColor(20, 22, 28) } } };
        nodes_.append(darkCol);

        GraphNodeData neonCol{ nextNodeId_++, QStringLiteral("ColorParam"), QStringLiteral("NeonColor"), QPointF(50, 320),
            { { QStringLiteral("name"), QStringLiteral("NeonColor") }, { QStringLiteral("color"), QColor(0, 220, 255) } } };
        nodes_.append(neonCol);

        GraphNodeData timeNode{ nextNodeId_++, QStringLiteral("Time"), QStringLiteral("Time"), QPointF(-150, 420), {} };
        nodes_.append(timeNode);

        GraphNodeData mul{ nextNodeId_++, QStringLiteral("Multiply"), QStringLiteral("Pulse"), QPointF(320, 350), {} };
        nodes_.append(mul);

        GraphNodeData rough{ nextNodeId_++, QStringLiteral("ScalarParam"), QStringLiteral("Roughness"), QPointF(200, 180),
            { { QStringLiteral("name"), QStringLiteral("Roughness") }, { QStringLiteral("value"), 0.15 } } };
        nodes_.append(rough);

        GraphNodeData metal{ nextNodeId_++, QStringLiteral("ScalarParam"), QStringLiteral("Metallic"), QPointF(200, 240),
            { { QStringLiteral("name"), QStringLiteral("Metallic") }, { QStringLiteral("value"), 0.9 } } };
        nodes_.append(metal);

        connections_.append({ darkCol.id, QStringLiteral("RGB"), master.id, QStringLiteral("BaseColor") });
        connections_.append({ neonCol.id, QStringLiteral("RGB"), mul.id, QStringLiteral("A") });
        connections_.append({ timeNode.id, QStringLiteral("SineTime"), mul.id, QStringLiteral("B") });
        connections_.append({ mul.id, QStringLiteral("Out"), master.id, QStringLiteral("Emissive") });
        connections_.append({ rough.id, QStringLiteral("Value"), master.id, QStringLiteral("Roughness") });
        connections_.append({ metal.id, QStringLiteral("Value"), master.id, QStringLiteral("Metallic") });
    }
    else if (presetName == QStringLiteral("Gold Metallic"))
    {
        GraphNodeData master{ nextNodeId_++, QStringLiteral("PBRMasterOutput"), QStringLiteral("PBR Master Output"), QPointF(600, 180), {} };
        nodes_.append(master);

        GraphNodeData goldCol{ nextNodeId_++, QStringLiteral("ColorParam"), QStringLiteral("GoldTint"), QPointF(100, 100),
            { { QStringLiteral("name"), QStringLiteral("GoldTint") }, { QStringLiteral("color"), QColor(255, 195, 85) } } };
        nodes_.append(goldCol);

        GraphNodeData fresnel{ nextNodeId_++, QStringLiteral("Fresnel"), QStringLiteral("Fresnel"), QPointF(100, 220), {} };
        nodes_.append(fresnel);

        GraphNodeData addNode{ nextNodeId_++, QStringLiteral("Add"), QStringLiteral("Add Rim"), QPointF(350, 140), {} };
        nodes_.append(addNode);

        GraphNodeData metal{ nextNodeId_++, QStringLiteral("ScalarParam"), QStringLiteral("Metallic"), QPointF(250, 280),
            { { QStringLiteral("name"), QStringLiteral("Metallic") }, { QStringLiteral("value"), 1.0 } } };
        nodes_.append(metal);

        GraphNodeData rough{ nextNodeId_++, QStringLiteral("ScalarParam"), QStringLiteral("Roughness"), QPointF(250, 360),
            { { QStringLiteral("name"), QStringLiteral("Roughness") }, { QStringLiteral("value"), 0.18 } } };
        nodes_.append(rough);

        connections_.append({ goldCol.id, QStringLiteral("RGB"), addNode.id, QStringLiteral("A") });
        connections_.append({ fresnel.id, QStringLiteral("Out"), addNode.id, QStringLiteral("B") });
        connections_.append({ addNode.id, QStringLiteral("Out"), master.id, QStringLiteral("BaseColor") });
        connections_.append({ metal.id, QStringLiteral("Value"), master.id, QStringLiteral("Metallic") });
        connections_.append({ rough.id, QStringLiteral("Value"), master.id, QStringLiteral("Roughness") });
    }
    else if (presetName == QStringLiteral("Scrolling Energy"))
    {
        GraphNodeData master{ nextNodeId_++, QStringLiteral("PBRMasterOutput"), QStringLiteral("PBR Master Output"), QPointF(650, 180), {} };
        nodes_.append(master);

        GraphNodeData uvNode{ nextNodeId_++, QStringLiteral("UVCoord"), QStringLiteral("UVCoord"), QPointF(-300, 200),
            { { QStringLiteral("tilingX"), 4.0 }, { QStringLiteral("tilingY"), 4.0 } } };
        nodes_.append(uvNode);

        GraphNodeData panner{ nextNodeId_++, QStringLiteral("Panner"), QStringLiteral("Panner"), QPointF(-100, 200),
            { { QStringLiteral("speedX"), 0.4 }, { QStringLiteral("speedY"), 0.1 } } };
        nodes_.append(panner);

        GraphNodeData tex{ nextNodeId_++, QStringLiteral("Texture2D"), QStringLiteral("EnergyMap"), QPointF(100, 200),
            { { QStringLiteral("textureName"), QStringLiteral("BaseMap") }, { QStringLiteral("slot"), 6 } } };
        nodes_.append(tex);

        GraphNodeData col{ nextNodeId_++, QStringLiteral("ColorParam"), QStringLiteral("EnergyColor"), QPointF(100, 360),
            { { QStringLiteral("name"), QStringLiteral("EnergyColor") }, { QStringLiteral("color"), QColor(50, 255, 120) } } };
        nodes_.append(col);

        GraphNodeData mul{ nextNodeId_++, QStringLiteral("Multiply"), QStringLiteral("Glow"), QPointF(350, 250), {} };
        nodes_.append(mul);

        connections_.append({ uvNode.id, QStringLiteral("UV"), panner.id, QStringLiteral("UV") });
        connections_.append({ panner.id, QStringLiteral("UV_Out"), tex.id, QStringLiteral("UV") });
        connections_.append({ tex.id, QStringLiteral("RGB"), mul.id, QStringLiteral("A") });
        connections_.append({ col.id, QStringLiteral("RGB"), mul.id, QStringLiteral("B") });
        connections_.append({ mul.id, QStringLiteral("Out"), master.id, QStringLiteral("Emissive") });
    }

    RebuildScene();
    CompileGraphToHlsl();
}

void MaterialGraphWidget::RetranslateUi()
{
    if (btnAddNode_) btnAddNode_->setText(Tr("+ Нода ▾", "+ Add Node ▾"));
    if (btnCompileHlsl_) btnCompileHlsl_->setText(Tr("⚡ Скомпилировать граф в HLSL", "⚡ Compile Graph -> HLSL"));
    if (btnCopyAi_) btnCopyAi_->setText(Tr("📋 Copy Graph + Prompt for AI", "📋 Copy Graph + Prompt for AI"));
    if (btnPasteAi_) btnPasteAi_->setText(Tr("📥 Paste Graph from AI", "📥 Paste Graph from AI"));
    if (btnPresets_) btnPresets_->setText(Tr("🎲 Готовые графы ▾", "🎲 Presets ▾"));
}
