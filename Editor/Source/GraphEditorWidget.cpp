#include "GraphEditorWidget.h"
#include "EditorLocale.h"
#include "Graph/GraphClipboard.h"

#include <QClipboard>
#include <QCursor>
#include <QFrame>
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
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <tuple>

using EditorCore::GraphDocument;
using EditorCore::GraphNodeData;
using EditorCore::GraphPin;
using EditorCore::GraphSchema;

namespace
{
constexpr qreal RowHeight = 20.0;
constexpr qreal FirstRow = 36.0;

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
    GraphView(QGraphicsScene* scene, QWidget* parent, qreal step) : QGraphicsView(scene, parent), step_(step) {}

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        painter->fillRect(rect, QColor(0x12, 0x14, 0x18));
        const bool minor = transform().m11() > 0.45;
        const qreal step = minor ? step_ : step_ * 5;
        const qreal left = std::floor(rect.left() / step) * step, top = std::floor(rect.top() / step) * step;
        QPen minorPen(QColor(0x1C, 0x1F, 0x26), 0), majorPen(QColor(0x27, 0x2B, 0x34), 0);
        for (qreal x = left; x < rect.right(); x += step)
        {
            painter->setPen(std::fmod(std::abs(x), step_ * 5) < 0.5 ? majorPen : minorPen);
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = top; y < rect.bottom(); y += step)
        {
            painter->setPen(std::fmod(std::abs(y), step_ * 5) < 0.5 ? majorPen : minorPen);
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }

private:
    qreal step_;
};

// Список нод с поиском: имя на языке редактора и имя типа, назначение, группа и синонимы.
// Без текста — все ноды по группам, как в меню; Enter или щелчок добавляет ноду.
class NodeSearchPopup final : public QFrame
{
public:
    static constexpr const char* Paste = "@paste";

    NodeSearchPopup(QWidget* parent, const GraphSchema& schema, bool canPaste, std::function<void(const QString&)> chosen)
        : QFrame(parent, Qt::Popup), schema_(schema), canPaste_(canPaste), chosen_(std::move(chosen))
    {
        const QString prefix = schema_.ObjectPrefix();
        setObjectName(prefix + QStringLiteral("NodeSearch"));
        setAttribute(Qt::WA_DeleteOnClose);
        setFrameShape(QFrame::StyledPanel);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);
        filter_ = new QLineEdit(this);
        filter_->setObjectName(prefix + QStringLiteral("NodeSearchText"));
        filter_->setPlaceholderText(schema_.SearchPlaceholder());
        filter_->setClearButtonEnabled(true);
        filter_->installEventFilter(this);
        list_ = new QListWidget(this);
        list_->setObjectName(prefix + QStringLiteral("NodeSearchList"));
        list_->setFocusPolicy(Qt::NoFocus); // ввод остаётся в поле поиска, стрелки ведут по списку
        layout->addWidget(filter_);
        layout->addWidget(list_, 1);
        resize(320, 440);
        connect(filter_, &QLineEdit::textChanged, this, [this] { Fill(); });
        connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { Choose(item); });
        Fill();
    }

    void Open(const QPoint& screenPos)
    {
        move(screenPos);
        show();
        filter_->setFocus();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != filter_ || event->type() != QEvent::KeyPress) return QFrame::eventFilter(watched, event);
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key())
        {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            Step(key->key() == Qt::Key_Up ? -1 : key->key() == Qt::Key_Down ? 1 : key->key() == Qt::Key_PageUp ? -8 : 8);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            Choose(list_->currentItem());
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            return QFrame::eventFilter(watched, event);
        }
    }

private:
    void Add(const QString& text, const QString& type, const QString& tip)
    {
        auto* item = new QListWidgetItem(text, list_);
        item->setData(Qt::UserRole, type);
        item->setToolTip(tip);
    }

    void Fill()
    {
        list_->clear();
        const QStringList words = filter_->text().toCaseFolded().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (canPaste_ && words.isEmpty())
            Add(Tr("Вставить скопированные ноды (Ctrl+V)", "Paste copied nodes (Ctrl+V)"), QString::fromLatin1(Paste), {});
        struct Match
        {
            int score;
            int order;
            QString type;
        };
        QVector<Match> matches;
        int order = 0;
        for (const auto& group : schema_.Groups())
        {
            if (words.isEmpty())
            {
                auto* header = new QListWidgetItem(group.name, list_);
                header->setFlags(Qt::NoItemFlags);
                QFont font = header->font();
                font.setBold(true);
                header->setFont(font);
            }
            for (const QString& type : group.types)
            {
                const QString name = schema_.DisplayName(type);
                if (words.isEmpty())
                {
                    Add(QStringLiteral("    ") + Label(type), type, schema_.Hint(type));
                    continue;
                }
                const QString hay = QStringList{type, group.name, group.searchText, schema_.SearchKeywords(type)}.join(QLatin1Char(' ')).toCaseFolded();
                if (!std::all_of(words.begin(), words.end(), [&](const QString& word) { return hay.contains(word); })) continue;
                // Выше те, чьё имя начинается с запроса, затем совпадения в имени, затем в описании.
                const QString first = words.first();
                const QString names = (type + QLatin1Char(' ') + name).toCaseFolded();
                const int score = type.toCaseFolded().startsWith(first) || name.toCaseFolded().startsWith(first) ? 0
                                : names.contains(first) ? 1 : 2;
                matches.append({score, order++, type});
            }
        }
        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return std::tie(a.score, a.order) < std::tie(b.score, b.order); });
        for (const auto& match : matches) Add(Label(match.type), match.type, schema_.Hint(match.type));
        if (!words.isEmpty() && matches.isEmpty())
            (new QListWidgetItem(Tr("Ничего не найдено", "Nothing found"), list_))->setFlags(Qt::NoItemFlags);
        for (int row = 0; row < list_->count(); ++row)
            if (list_->item(row)->flags() & Qt::ItemIsSelectable)
            {
                list_->setCurrentRow(row);
                break;
            }
    }

    QString Label(const QString& type) const
    {
        const QString name = schema_.DisplayName(type);
        return name == type ? type : QStringLiteral("%1   ·  %2").arg(name, type);
    }

    void Step(int delta)
    {
        int row = list_->currentRow();
        for (int moved = 0, step = delta > 0 ? 1 : -1; moved < std::abs(delta);)
        {
            const int next = row + step;
            if (next < 0 || next >= list_->count()) break;
            row = next;
            if (list_->item(row)->flags() & Qt::ItemIsSelectable) ++moved;
        }
        if (row >= 0 && row < list_->count() && (list_->item(row)->flags() & Qt::ItemIsSelectable)) list_->setCurrentRow(row);
    }

    void Choose(QListWidgetItem* item)
    {
        if (done_ || !item || !(item->flags() & Qt::ItemIsSelectable)) return;
        const QString type = item->data(Qt::UserRole).toString();
        if (type.isEmpty()) return;
        done_ = true;
        auto chosen = chosen_;
        close();
        chosen(type);
    }

    const GraphSchema& schema_;
    QLineEdit* filter_ = nullptr;
    QListWidget* list_ = nullptr;
    bool canPaste_ = false, done_ = false;
    std::function<void(const QString&)> chosen_;
};
} // namespace

class GraphEditorWidget::NodeItem final : public QGraphicsItem
{
public:
    NodeItem(const GraphNodeData& data, GraphEditorWidget* owner, const QString& problem)
        : data_(data), owner_(owner), problem_(problem)
    {
        const GraphSchema& schema = owner_->schema_;
        decoration_ = owner_->NodeDecoration(data_);
        title_ = schema.NodeTitle(data_);
        color_ = schema.NodeColor(data_);
        const QString hint = schema.Hint(data_.type);
        setToolTip(problem_.isEmpty() ? hint : problem_ + (hint.isEmpty() ? QString() : QStringLiteral("\n\n") + hint));
        Layout();
        setPos(data.pos);
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(1);
    }

    bool HasProblem() const { return !problem_.isEmpty(); }

    const GraphNodeData& Data() const { return data_; }

    QRectF boundingRect() const override { return QRectF(-6, 0, width_ + 12, height_); }

    QPointF PinPos(const QString& name) const
    {
        for (const auto& pin : pins_)
            if (pin.name == name) return pin.localPos;
        return {};
    }

    const GraphPin* Pin(const QString& name, bool input) const
    {
        for (const auto& pin : pins_)
            if (pin.name == name && pin.isInput == input) return &pin;
        return nullptr;
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
        const QColor problemColor(0xEF, 0x44, 0x44);
        painter->setPen(isSelected() ? QPen(QColor(0x38, 0xBD, 0xF8), 2.0)
                        : !problem_.isEmpty() ? QPen(problemColor, 2.0) : QPen(QColor(0x2D, 0x33, 0x42), 1.0));
        painter->drawPath(body);

        QPainterPath header;
        header.moveTo(0, 24);
        header.lineTo(0, 6);
        header.arcTo(0, 0, 12, 12, 180, -90);
        header.lineTo(width_ - 6, 0);
        header.arcTo(width_ - 12, 0, 12, 12, 90, -90);
        header.lineTo(width_, 24);
        header.closeSubpath();
        painter->fillPath(header, color_);

        QFont font = painter->font();
        font.setBold(true);
        font.setPixelSize(11);
        painter->setFont(font);
        painter->setPen(Qt::white);
        const qreal badge = problem_.isEmpty() ? 0.0 : 18.0;
        painter->drawText(QRectF(8, 2, width_ - 16 - badge, 20), Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(title_, Qt::ElideRight, static_cast<int>(width_ - 16 - badge)));
        if (!problem_.isEmpty())
        {
            // Значок ошибки в заголовке: подробности в подсказке и в списке ошибок окна.
            const QRectF mark(width_ - 21, 4, 16, 16);
            painter->setBrush(problemColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(mark);
            painter->setPen(Qt::white);
            painter->drawText(mark, Qt::AlignCenter, QStringLiteral("!"));
        }

        font.setBold(false);
        font.setPixelSize(10);
        painter->setFont(font);
        for (const auto& pin : pins_)
        {
            const QColor color = owner_->schema_.PinColor(pin);
            painter->setBrush(color);
            painter->setPen(QPen(color.darker(150), 1.0));
            painter->drawEllipse(pin.localPos, 4.5, 4.5);
            painter->setPen(QColor(0xCB, 0xD5, 0xE1));
            const QRectF label = pin.isInput ? QRectF(pin.localPos.x() + 8, pin.localPos.y() - 8, width_ / 2 + 10, 16)
                                             : QRectF(width_ / 2 - 20, pin.localPos.y() - 8, width_ / 2 + 8, 16);
            painter->drawText(label, (pin.isInput ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, pin.name);
        }

        // Содержимое ноды под пинами рисует наследник виджета.
        const QRectF content(8, contentTop_, width_ - 16, height_ - contentTop_ - 6);
        painter->setPen(QColor(0x94, 0xA3, 0xB8));
        owner_->PaintNodeContent(painter, data_, content, decoration_);
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionChange && scene()) return GraphEditorWidget::Snap(value.toPointF());
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
        pins_ = owner_->schema_.Pins(data_);
        int inputs = 0, outputs = 0;
        for (auto& pin : pins_)
        {
            int& row = pin.isInput ? inputs : outputs;
            pin.localPos = QPointF(0, FirstRow + row * RowHeight);
            ++row;
        }
        width_ = owner_->NodeWidth(data_);
        for (auto& pin : pins_)
            if (!pin.isInput) pin.localPos.setX(width_);
        contentTop_ = FirstRow + std::max(1, std::max(inputs, outputs)) * RowHeight - 6;
        height_ = contentTop_ + owner_->NodeContentHeight(data_) + 6;
    }

    GraphNodeData data_;
    GraphEditorWidget* owner_ = nullptr;
    QString problem_, title_;
    QColor color_;
    QVector<GraphPin> pins_;
    QVariant decoration_;
    qreal width_ = 150, height_ = 80, contentTop_ = 40;
};

GraphEditorWidget::GraphEditorWidget(const GraphSchema& schema, QWidget* parent)
    : QWidget(parent), schema_(schema)
{
    if (!schema_.Id().isEmpty()) document_.SetSchema(schema_.Id(), schema_.Version());
    BuildUi();
}

GraphEditorWidget::~GraphEditorWidget() = default;

QPointF GraphEditorWidget::Snap(const QPointF& point)
{
    return {std::round(point.x() / GridStep) * GridStep, std::round(point.y() / GridStep) * GridStep};
}

QString GraphEditorWidget::EditActionText(const GraphNodeData&) const
{
    return Tr("Свойства…", "Properties…");
}

QString GraphEditorWidget::RenameTitle() const
{
    return Tr("Имя ноды", "Node name");
}

void GraphEditorWidget::BuildUi()
{
    const QString prefix = schema_.ObjectPrefix();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    toolbar_ = new QHBoxLayout;
    toolbar_->setContentsMargins(4, 3, 4, 3);
    auto* add = new QToolButton(this);
    add->setObjectName(prefix + QStringLiteral("AddNode"));
    add->setText(Tr("+ Нода", "+ Node"));
    add->setToolTip(Tr("Добавить ноду: список с поиском (также ПКМ по холсту или Tab)",
                       "Add a node: a searchable list (also RMB on the canvas or Tab)"));
    connect(add, &QToolButton::clicked, this, [this, add] {
        ShowNodeSearch(add->mapToGlobal(QPoint(0, add->height())), view_->mapToScene(view_->viewport()->rect().center()));
    });
    toolbar_->addWidget(add);

    auto history = [this, &prefix](const char* name, const char* glyph, const QString& tip, void (GraphEditorWidget::*slot)()) {
        auto* button = new QToolButton(this);
        button->setObjectName(prefix + QString::fromLatin1(name));
        button->setText(QString::fromUtf8(glyph));
        QFont font = button->font();
        // Стрелки в обычном размере шрифта почти не видны.
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * 1.4);
        else if (font.pixelSize() > 0) font.setPixelSize(font.pixelSize() * 7 / 5);
        button->setFont(font);
        button->setToolTip(tip);
        button->setEnabled(false);
        connect(button, &QToolButton::clicked, this, slot);
        toolbar_->addWidget(button);
        return button;
    };
    undoButton_ = history("Undo", "↶", Tr("Отменить (Ctrl+Z)", "Undo (Ctrl+Z)"), &GraphEditorWidget::Undo);
    redoButton_ = history("Redo", "↷", Tr("Повторить (Ctrl+Y)", "Redo (Ctrl+Y)"), &GraphEditorWidget::Redo);
    auto* frame = new QToolButton(this);
    frame->setText(Tr("Показать всё", "Frame all"));
    frame->setToolTip(Tr("Уместить граф в окно, а при выделении — выделенные ноды (F)", "Fit the graph, or the selection, into view (F)"));
    connect(frame, &QToolButton::clicked, this, [this] { FrameNodes(); });
    toolbar_->addWidget(frame);
    toolbarIndex_ = toolbar_->count();

    toolbar_->addStretch(1);
    auto* hint = new QLabel(Tr("ПКМ/Tab — ноды · ЛКМ по пину — связь · Ctrl+C/V/D/Z · Del",
                               "RMB/Tab: nodes · LMB on a pin: link · Ctrl+C/V/D/Z · Del"), this);
    hint->setToolTip(Tr("Пин → пин: связь; потянуть подключённый вход — снять связь\n"
                        "ПКМ по холсту или Tab — добавить ноду, по ноде — её меню\n"
                        "Колесо — масштаб, ПКМ/СКМ с движением — сдвиг, F — показать всё\n"
                        "Ctrl+Z / Ctrl+Y — отменить / повторить\n"
                        "Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+D — копировать / вырезать / вставить / дублировать\n"
                        "Ctrl+A — выделить всё, Del — удалить",
                        "Pin to pin: link; drag a connected input to detach it\n"
                        "RMB on the canvas or Tab: add a node; on a node: its menu\n"
                        "Wheel: zoom, RMB/MMB drag: pan, F: frame all\n"
                        "Ctrl+Z / Ctrl+Y: undo / redo\n"
                        "Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+D: copy / cut / paste / duplicate\n"
                        "Ctrl+A: select all, Del: delete"));
    QPalette hintPalette = hint->palette(); // серый, но не выключенный: у выключенной надписи нет подсказки
    hintPalette.setColor(QPalette::WindowText, hintPalette.color(QPalette::PlaceholderText));
    hint->setPalette(hintPalette);
    toolbar_->addWidget(hint);
    layout->addLayout(toolbar_);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-5000, -5000, 10000, 10000);
    view_ = new GraphView(scene_, this, GridStep);
    view_->setObjectName(prefix + QStringLiteral("View"));
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

void GraphEditorWidget::AddToolbarWidget(QWidget* widget)
{
    toolbar_->insertWidget(toolbarIndex_++, widget);
}

void GraphEditorWidget::SetDocument(const GraphDocument& document)
{
    history_.Clear();
    problems_.clear();
    ReplaceDocument(document);
    UpdateHistoryButtons();
}

void GraphEditorWidget::ReplaceDocument(const GraphDocument& document)
{
    document_ = document;
    schema_.EnsureRequiredNodes(document_);
    RebuildScene();
    QRectF bounds;
    for (const auto& node : document_.Nodes()) bounds |= QRectF(node.pos, QSizeF(180, 160));
    if (!bounds.isNull()) view_->centerOn(bounds.center());
}

void GraphEditorWidget::RebuildScene()
{
    // Выделение переживает пересборку: после правки свойства или вставки ноды остаются выделенными.
    const QList<int> selected = SelectedNodes();
    // Сначала связи: scene_->clear() удаляет и их.
    links_.clear();
    dragLink_ = nullptr;
    scene_->clear();
    for (const auto& node : document_.Nodes())
        scene_->addItem(new NodeItem(node, this, problems_.value(node.id)));
    UpdateConnections();
    SelectNodes(selected);
}

GraphEditorWidget::NodeItem* GraphEditorWidget::FindItem(int nodeId) const
{
    for (auto* item : scene_->items())
        if (auto* node = dynamic_cast<NodeItem*>(item); node && node->Data().id == nodeId) return node;
    return nullptr;
}

QPointF GraphEditorWidget::PinScenePos(int nodeId, const QString& pin) const
{
    const NodeItem* item = FindItem(nodeId);
    return item ? item->mapToScene(item->PinPos(pin)) : QPointF();
}

void GraphEditorWidget::UpdateConnections()
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
    for (const auto& c : document_.Connections())
    {
        NodeItem* from = items.value(c.fromNode);
        NodeItem* to = items.value(c.toNode);
        if (!from || !to) continue;
        const GraphPin* output = from->Pin(c.fromPin, false);
        const QColor color = output ? schema_.LinkColor(*output) : QColor(0x38, 0xBD, 0xF8);
        auto* link = scene_->addPath(LinkPath(from->mapToScene(from->PinPos(c.fromPin)), to->mapToScene(to->PinPos(c.toPin))),
                                     QPen(color, 2.5));
        link->setZValue(0);
        links_.push_back(link);
    }
}

void GraphEditorWidget::NodeMoved(int nodeId, const QPointF& pos)
{
    if (GraphNodeData* node = document_.FindNode(nodeId)) node->pos = pos;
    moved_ = true;
    UpdateConnections();
}

bool GraphEditorWidget::IsFixed(int nodeId) const
{
    const GraphNodeData* node = document_.FindNode(nodeId);
    return !node || schema_.IsFixed(*node);
}

int GraphEditorWidget::AddNode(const QString& type, const QPointF& pos, const QVariantMap& props)
{
    QVariantMap properties = props;
    QString title = type;
    schema_.PrepareNewNode(document_, type, properties, title);
    Checkpoint();
    const int id = document_.AddNode(type, title, Snap(pos), properties);
    RebuildScene();
    NotifyStructure();
    return id;
}

bool GraphEditorWidget::Connect(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
{
    if (!schema_.ConnectionProblem(document_, fromNode, fromPin, toNode, toPin).isEmpty()) return false;
    for (const auto& c : document_.Connections())
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin) return true; // уже есть
    Checkpoint(linkKey_);

    const QVector<GraphPin> fromPins = schema_.Pins(*document_.FindNode(fromNode));
    const QVector<GraphPin> toPins = schema_.Pins(*document_.FindNode(toNode));
    const bool singleInput = schema_.InputTakesOneLink(*schema_.FindPin(toPins, toPin, true));
    const bool singleOutput = schema_.OutputTakesOneLink(*schema_.FindPin(fromPins, fromPin, false));
    const QVector<EditorCore::GraphConnectionData> links = document_.Connections();
    for (const auto& c : links)
        if ((singleInput && c.toNode == toNode && c.toPin == toPin) || (singleOutput && c.fromNode == fromNode && c.fromPin == fromPin))
            document_.RemoveConnection(c.fromNode, c.fromPin, c.toNode, c.toPin);
    document_.AddConnection(fromNode, fromPin, toNode, toPin);
    UpdateConnections();
    NotifyStructure();
    return true;
}

void GraphEditorWidget::RemoveNodes(const QList<int>& ids)
{
    if (std::all_of(ids.begin(), ids.end(), [this](int id) { return IsFixed(id); })) return;
    Checkpoint();
    for (const int id : ids)
        if (!IsFixed(id)) document_.RemoveNode(id);
    RebuildScene();
    NotifyStructure();
}

bool GraphEditorWidget::RenameNode(int nodeId, const QString& name)
{
    GraphNodeData* node = document_.FindNode(nodeId);
    if (!node || schema_.NodeName(*node).isEmpty() || !schema_.NameProblem(name).isEmpty()) return false;
    if (name == schema_.NodeName(*node)) return true;
    if (schema_.UniqueName(document_, name) != name) return false;
    Checkpoint();
    node = document_.FindNode(nodeId);
    schema_.SetNodeName(*node, name);
    node->title = name;
    RebuildScene();
    NotifyStructure();
    return true;
}

void GraphEditorWidget::DeleteSelected()
{
    RemoveNodes(SelectedNodes());
}

void GraphEditorWidget::NotifyStructure()
{
    if (structureChanged) structureChanged();
}

bool GraphEditorWidget::PinAt(const QPointF& scenePos, int& nodeId, QString& pin, bool& input) const
{
    for (auto* item : scene_->items(QRectF(scenePos - QPointF(10, 10), QSizeF(20, 20))))
        if (auto* node = dynamic_cast<NodeItem*>(item); node && node->PinAt(node->mapFromScene(scenePos), pin, input))
        {
            nodeId = node->Data().id;
            return true;
        }
    return false;
}

void GraphEditorWidget::StartLink(const QPointF& scenePos)
{
    int nodeId = 0;
    QString pin;
    bool input = false;
    if (!PinAt(scenePos, nodeId, pin, input)) return;
    linkNode_ = nodeId;
    linkPin_ = pin;
    linkFromInput_ = input;
    // Снятие связи и её новое подключение — один шаг отмены.
    linkKey_ = QStringLiteral("link:%1").arg(++linkSerial_);
    const NodeItem* item = FindItem(nodeId);
    const GraphPin* pressed = item ? item->Pin(pin, input) : nullptr;
    // Подключённый одиночный пин: связь снимается и тянется дальше от другого конца, как в Unreal.
    const bool single = pressed && (input ? schema_.InputTakesOneLink(*pressed) : schema_.OutputTakesOneLink(*pressed));
    if (single)
    {
        for (const auto& c : document_.Connections())
            if (input ? (c.toNode == nodeId && c.toPin == pin) : (c.fromNode == nodeId && c.fromPin == pin))
            {
                Checkpoint(linkKey_);
                const EditorCore::GraphConnectionData link = c;
                document_.RemoveConnection(link.fromNode, link.fromPin, link.toNode, link.toPin);
                linkNode_ = input ? link.fromNode : link.toNode;
                linkPin_ = input ? link.fromPin : link.toPin;
                linkFromInput_ = !input;
                UpdateConnections();
                NotifyStructure();
                break;
            }
    }
    linkStart_ = PinScenePos(linkNode_, linkPin_);
    dragLink_ = scene_->addPath(LinkPath(linkStart_, scenePos), QPen(QColor(0xE2, 0xE8, 0xF0), 2.0, Qt::DashLine));
    dragLink_->setZValue(2);
}

void GraphEditorWidget::FinishLink(const QPointF& scenePos)
{
    scene_->removeItem(dragLink_);
    delete dragLink_;
    dragLink_ = nullptr;
    int nodeId = 0;
    QString pin;
    bool input = false;
    if (PinAt(scenePos, nodeId, pin, input) && input != linkFromInput_)
    {
        if (linkFromInput_) Connect(nodeId, pin, linkNode_, linkPin_);
        else Connect(linkNode_, linkPin_, nodeId, pin);
    }
    // Связь сняли и вернули на тот же пин: граф прежний, пустой шаг отмены не нужен.
    if (history_.DropEmptyStep(linkKey_, document_.ExportToJson())) UpdateHistoryButtons();
    linkKey_.clear();
}

bool GraphEditorWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride))
    {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool ctrl = key->modifiers() & Qt::ControlModifier;
        std::function<void()> action;
        if (key->matches(QKeySequence::Undo))
            action = [this] { Undo(); };
        else if (key->matches(QKeySequence::Redo) || (ctrl && key->key() == Qt::Key_Y) ||
                 (ctrl && (key->modifiers() & Qt::ShiftModifier) && key->key() == Qt::Key_Z))
            action = [this] { Redo(); };
        else if (key->matches(QKeySequence::Copy))
            action = [this] { CopyNodes(SelectedNodes()); };
        else if (key->matches(QKeySequence::Cut))
            action = [this] { if (CopyNodes(SelectedNodes())) DeleteSelected(); };
        else if (key->matches(QKeySequence::Paste))
            action = [this] { PasteNodes(CursorScenePos()); };
        else if (key->matches(QKeySequence::SelectAll))
            action = [this] { for (auto* item : scene_->items()) if (dynamic_cast<NodeItem*>(item)) item->setSelected(true); };
        else if (ctrl && key->key() == Qt::Key_D)
            action = [this] { DuplicateNodes(SelectedNodes()); };
        else if (key->modifiers() == Qt::NoModifier && (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace))
            action = [this] { DeleteSelected(); };
        else if (key->modifiers() == Qt::NoModifier && key->key() == Qt::Key_F)
            action = [this] { FrameNodes(); };
        else if (key->modifiers() == Qt::NoModifier && (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Space))
            action = [this] { ShowNodeSearch(QCursor::pos(), CursorScenePos()); };
        if (!action) return QWidget::eventFilter(watched, event);
        // Сочетания графа важнее сочетаний окна (Ctrl+Z и др.), пока фокус на холсте.
        if (event->type() == QEvent::ShortcutOverride)
        {
            event->accept();
            return true;
        }
        if (!dragLink_) action();
        return true;
    }
    if (watched != view_->viewport()) return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(event);
        view_->setFocus();
        history_.BreakMerge(); // новая правка мышью не сливается с правкой из настроек
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
            pressSnapshot_ = document_.ExportToJson();
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
                    QTimer::singleShot(0, this, [this, screen, scenePos] { ShowNodeSearch(screen, scenePos); });
            }
            return true;
        }
        if (mouse->button() == Qt::LeftButton && moved_)
        {
            moved_ = false;
            // Перетаскивание нод — один шаг отмены: граф до нажатия.
            if (!pressSnapshot_.isEmpty())
            {
                history_.Push(pressSnapshot_);
                UpdateHistoryButtons();
            }
            pressSnapshot_.clear();
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

void GraphEditorWidget::ShowNodeSearch(const QPoint& screenPos, const QPointF& scenePos)
{
    auto* popup = new NodeSearchPopup(this, schema_, CanPaste(), [this, scenePos](const QString& type) {
        if (type == QLatin1String(NodeSearchPopup::Paste)) PasteNodes(scenePos);
        else SelectNodes({AddNode(type, scenePos)});
    });
    popup->Open(screenPos);
}

void GraphEditorWidget::ShowNodeMenu(const QPoint& screenPos, int nodeId)
{
    const GraphNodeData* node = document_.FindNode(nodeId);
    if (!node) return;
    QMenu menu(this);
    const QString edit = EditActionText(*node);
    if (!edit.isEmpty()) menu.addAction(edit, this, [this, nodeId] { EditNode(nodeId); });
    if (!schema_.NodeName(*node).isEmpty())
        menu.addAction(Tr("Переименовать…", "Rename…"), this, [this, nodeId] { RequestRename(nodeId); });
    // Команды правки — для выделения, если нода в нём, иначе для одной ноды.
    if (!SelectedNodes().contains(nodeId)) SelectNodes({nodeId});
    const QList<int> chosen = SelectedNodes();
    const bool onlyFixed = std::all_of(chosen.begin(), chosen.end(), [this](int id) { return IsFixed(id); });
    menu.addSeparator();
    menu.addAction(Tr("Копировать\tCtrl+C", "Copy\tCtrl+C"), this, [this, chosen] { CopyNodes(chosen); })->setEnabled(!onlyFixed);
    menu.addAction(Tr("Дублировать\tCtrl+D", "Duplicate\tCtrl+D"), this, [this, chosen] { DuplicateNodes(chosen); })->setEnabled(!onlyFixed);
    menu.addAction(Tr("Удалить\tDel", "Delete\tDel"), this, [this, chosen] { RemoveNodes(chosen); })->setEnabled(!onlyFixed);
    menu.exec(screenPos);
}

void GraphEditorWidget::RequestRename(int nodeId)
{
    const GraphNodeData* node = document_.FindNode(nodeId);
    if (!node) return;
    bool accepted = false;
    const QString name = QInputDialog::getText(this, RenameTitle(), Tr("Имя:", "Name:"),
                                               QLineEdit::Normal, schema_.NodeName(*node), &accepted).trimmed();
    if (!accepted || RenameNode(nodeId, name)) return;
    QString problem = schema_.NameProblem(name);
    if (problem.isEmpty()) problem = Tr("Имя «%1» уже есть в графе.", "The name \"%1\" is already in the graph.").arg(name);
    QMessageBox::warning(this, Tr("Имя не подходит", "Invalid name"), problem);
}

void GraphEditorWidget::Checkpoint(const QString& mergeKey)
{
    if (history_.Merges(mergeKey)) return;
    history_.Push(document_.ExportToJson(), mergeKey);
    UpdateHistoryButtons();
}

void GraphEditorWidget::UpdateHistoryButtons()
{
    if (undoButton_) undoButton_->setEnabled(CanUndo());
    if (redoButton_) redoButton_->setEnabled(CanRedo());
}

void GraphEditorWidget::Undo()
{
    QString snapshot;
    if (dragLink_ || !history_.Undo(document_.ExportToJson(), snapshot)) return;
    RestoreSnapshot(snapshot);
}

void GraphEditorWidget::Redo()
{
    QString snapshot;
    if (dragLink_ || !history_.Redo(document_.ExportToJson(), snapshot)) return;
    RestoreSnapshot(snapshot);
}

void GraphEditorWidget::RestoreSnapshot(const QString& snapshot)
{
    GraphDocument restored;
    if (!restored.ImportFromJson(snapshot)) return;
    const GraphDocument before = document_;
    document_ = restored;
    RebuildScene();
    UpdateHistoryButtons();
    OnRestored(before);
    NotifyStructure();
}

QList<int> GraphEditorWidget::SelectedNodes() const
{
    QList<int> ids;
    for (auto* item : scene_->selectedItems())
        if (auto* node = dynamic_cast<NodeItem*>(item)) ids << node->Data().id;
    std::sort(ids.begin(), ids.end());
    return ids;
}

void GraphEditorWidget::SelectNodes(const QList<int>& ids)
{
    scene_->clearSelection();
    for (auto* item : scene_->items())
        if (auto* node = dynamic_cast<NodeItem*>(item); node && ids.contains(node->Data().id)) node->setSelected(true);
}

QString GraphEditorWidget::ClipboardJson(const QList<int>& ids) const
{
    return EditorCore::GraphClipboard::Encode(document_, ids, schema_.ClipboardMarker(),
                                              [this](const GraphNodeData& node) { return !schema_.IsFixed(node); });
}

QString GraphEditorWidget::ClipboardGraph() const
{
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    if (!mime) return {};
    const QString format = schema_.ClipboardMime();
    const QByteArray data = mime->hasFormat(format) ? mime->data(format) : mime->text().toUtf8();
    return EditorCore::GraphClipboard::Decode(data, schema_.ClipboardMarker());
}

bool GraphEditorWidget::CopyNodes(const QList<int>& ids) const
{
    const QString json = ClipboardJson(ids);
    if (json.isEmpty()) return false;
    auto* mime = new QMimeData;
    mime->setData(schema_.ClipboardMime(), json.toUtf8());
    mime->setText(json);
    QGuiApplication::clipboard()->setMimeData(mime);
    return true;
}

bool GraphEditorWidget::CanPaste() const
{
    return !ClipboardGraph().isEmpty();
}

QList<int> GraphEditorWidget::PasteNodes(const QPointF& topLeft)
{
    return InsertNodes(ClipboardGraph(), topLeft);
}

QList<int> GraphEditorWidget::DuplicateNodes(const QList<int>& ids)
{
    QRectF bounds;
    for (const int id : ids)
        if (const GraphNodeData* node = document_.FindNode(id); node && !schema_.IsFixed(*node))
            bounds = bounds.isNull() ? QRectF(node->pos, QSizeF(1, 1)) : bounds.united(QRectF(node->pos, QSizeF(1, 1)));
    return bounds.isNull() ? QList<int>() : InsertNodes(ClipboardJson(ids), bounds.topLeft() + QPointF(2 * GridStep, 2 * GridStep));
}

QList<int> GraphEditorWidget::InsertNodes(const QString& json, const QPointF& topLeft)
{
    GraphDocument clip;
    if (json.isEmpty() || !clip.ImportFromJson(json)) return {};
    QVector<GraphNodeData> nodes;
    QRectF bounds;
    for (const auto& node : clip.Nodes())
    {
        if (schema_.IsFixed(node) || !schema_.IsKnownNode(node.type)) continue;
        nodes << node;
        bounds = bounds.isNull() ? QRectF(node.pos, QSizeF(1, 1)) : bounds.united(QRectF(node.pos, QSizeF(1, 1)));
    }
    if (nodes.isEmpty()) return {};

    Checkpoint();
    const QPointF offset = Snap(topLeft) - Snap(bounds.topLeft());
    QHash<int, int> ids; // id в буфере -> новый id
    for (GraphNodeData node : nodes)
    {
        schema_.PreparePastedNode(document_, node);
        const int id = document_.AddNode(node.type, node.title, Snap(node.pos + offset), node.properties);
        document_.FindNode(id)->extra = node.extra;
        ids.insert(node.id, id);
    }
    for (const auto& c : clip.Connections())
        if (ids.contains(c.fromNode) && ids.contains(c.toNode))
            document_.AddConnection(ids.value(c.fromNode), c.fromPin, ids.value(c.toNode), c.toPin, c.properties);
    RebuildScene();
    QList<int> created = ids.values();
    std::sort(created.begin(), created.end());
    SelectNodes(created);
    NotifyStructure();
    return created;
}

QPointF GraphEditorWidget::CursorScenePos() const
{
    const QPoint local = view_->viewport()->mapFromGlobal(QCursor::pos());
    return view_->mapToScene(view_->viewport()->rect().contains(local) ? local : view_->viewport()->rect().center());
}

void GraphEditorWidget::FrameNodes()
{
    QRectF bounds;
    const bool selection = !scene_->selectedItems().isEmpty();
    for (auto* item : scene_->items())
        if (dynamic_cast<NodeItem*>(item) && (!selection || item->isSelected())) bounds |= item->sceneBoundingRect();
    if (bounds.isNull()) return;
    bounds.adjust(-40, -40, 40, 40);
    const QRectF viewport = view_->viewport()->rect();
    // Пределы как у колеса мыши: одна нода не раздувается на весь экран, большой граф остаётся читаемым.
    const qreal scale = std::clamp(std::min(viewport.width() / bounds.width(), viewport.height() / bounds.height()), 0.25, 1.25);
    view_->resetTransform();
    view_->scale(scale, scale);
    view_->centerOn(bounds.center());
}

void GraphEditorWidget::SetNodeProblems(const QHash<int, QString>& problems)
{
    if (problems == problems_) return;
    problems_ = problems;
    RebuildScene();
}

void GraphEditorWidget::FocusNode(int nodeId)
{
    NodeItem* item = FindItem(nodeId);
    if (!item) return;
    SelectNodes({nodeId});
    view_->centerOn(item);
    view_->setFocus();
}
