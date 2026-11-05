#include "MaterialGraphWidget.h"
#include "EditorLocale.h"
#include "HlslGraphCompiler.h"
#include "MaterialTextureTools.h"

#include <QAction>
#include <QClipboard>
#include <QColorDialog>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <tuple>

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
    return type == QStringLiteral("ColorParam") || type == QStringLiteral("ScalarParam") ||
           type == QStringLiteral("Vector2Param") || type == QStringLiteral("Vector3Param");
}

int VectorSize(const QString& type)
{
    return type == QStringLiteral("Vector2Param") ? 2 : type == QStringLiteral("Vector3Param") ? 3 : 0;
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
    return EditorCore::HlslGraphCompiler::NodePins(type);
}

// Группы нод: из них строятся меню добавления и по ним красятся заголовки.
struct NodeGroup
{
    const char* ru;
    const char* en;
    QStringList types;
    QColor color;
};

const QVector<NodeGroup>& NodeGroups()
{
    static const QVector<NodeGroup> groups{
        {"Параметры", "Parameters", {"ColorParam", "ScalarParam", "Vector2Param", "Vector3Param"}, QColor(0x15, 0x80, 0x3D)},
        {"Константы и каналы", "Constants and channels", {"Constant", "Split", "Combine"}, QColor(0x4D, 0x7C, 0x0F)},
        {"Входы поверхности", "Surface inputs",
         {"ObjectColor", "SurfaceDefaults", "InstanceData", "WorldPosition", "SurfaceNormal", "ViewDirection", "FrontFace"}, QColor(0xA1, 0x62, 0x07)},
        {"Текстуры", "Textures", {"Texture2D", "NormalMap"}, QColor(0x1D, 0x4E, 0xD8)},
        {"Координаты и время", "Coordinates and time", {"UVCoord", "Panner", "Time"}, QColor(0x0E, 0x74, 0x90)},
        {"Математика", "Math",
         {"Add", "Subtract", "Multiply", "Divide", "Power", "Min", "Max", "Lerp", "Clamp", "Saturate", "OneMinus",
          "Abs", "Frac", "Sine", "Normalize", "Step", "Smoothstep", "Dot", "Fresnel"}, QColor(0x7E, 0x22, 0xCE)}};
    return groups;
}

QColor CategoryColor(const QString& type)
{
    if (type == MasterType) return QColor(0xC2, 0x41, 0x0C);
    for (const auto& group : NodeGroups())
        if (group.types.contains(type)) return group.color;
    return QColor(0x47, 0x55, 0x69);
}

// Цвет пина по типу: связь допустима любая, цвет подсказывает, что будет приведено.
QColor PinColor(const QString& type)
{
    if (type == QStringLiteral("float")) return QColor(0x94, 0xA3, 0xB8);
    if (type == QStringLiteral("float2")) return QColor(0x34, 0xD3, 0x99);
    if (type == QStringLiteral("float3")) return QColor(0xFA, 0xCC, 0x15);
    if (type == QStringLiteral("float4")) return QColor(0xF4, 0x72, 0xB6);
    return QColor(0xE2, 0xE8, 0xF0); // any: размер по входам
}

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

// Имя ноды для людей, что она делает и слова для поиска (кроме имени типа и группы).
struct NodeInfo
{
    const char* ru;
    const char* hintRu;
    const char* hintEn;
    const char* keywords;
};

const QHash<QString, NodeInfo>& NodeInfos()
{
    static const QHash<QString, NodeInfo> infos{
        {MasterType, {"Выход материала", "Поверхность материала: цвет, прозрачность, блик, свечение, нормаль.",
                      "The material surface: colour, opacity, highlight, glow, normal.", "output result"}},
        {"ColorParam", {"Цвет (параметр)", "Цвет, который настраивается в материале без перекомпиляции.",
                        "A colour tweaked in the material without recompiling.", "colour tint rgb оттенок"}},
        {"ScalarParam", {"Число (параметр)", "Число, которое настраивается в материале без перекомпиляции.",
                         "A number tweaked in the material without recompiling.", "float value scalar значение"}},
        {"Vector2Param", {"Вектор 2 (параметр)", "Два числа, которые настраиваются в материале.", "Two numbers tweaked in the material.", "float2 vector"}},
        {"Vector3Param", {"Вектор 3 (параметр)", "Три числа, которые настраиваются в материале.", "Three numbers tweaked in the material.", "float3 vector"}},
        {"Constant", {"Константа", "Число, вписанное в шейдер.", "A number written into the shader.", "number value число"}},
        {"Split", {"Разделить каналы", "Компоненты вектора по отдельности: R, G, B, A.", "Vector components one by one: R, G, B, A.", "channel mask swizzle канал"}},
        {"Combine", {"Собрать вектор", "Вектор из отдельных чисел.", "A vector from separate numbers.", "append merge channel канал"}},
        {"ObjectColor", {"Цвет объекта", "Цвет компонента объекта (у копий — цвет копии).", "Colour of the object's component (per copy when instanced).", "vertex colour tint"}},
        {"SurfaceDefaults", {"Поверхность по умолчанию", "Цвет объекта × его текстура, альфа и нормаль до правок графа.",
                             "Object colour times its texture, alpha and normal before the graph.", "default base"}},
        {"InstanceData", {"Данные копии", "Данные копии меша при инстансинге.", "Per-copy data of instanced meshes.", "instance custom"}},
        {"WorldPosition", {"Позиция в мире", "Мировые координаты точки поверхности.", "World position of the surface point.", "world position координаты"}},
        {"SurfaceNormal", {"Нормаль поверхности", "Нормаль в мировых координатах.", "World-space normal.", "normal"}},
        {"ViewDirection", {"Направление на камеру", "Единичный вектор от поверхности к камере.", "Unit vector from the surface to the camera.", "camera view eye"}},
        {"FrontFace", {"Лицевая сторона", "1 на лицевой стороне, 0 на обратной (двусторонние материалы).",
                       "1 on the front face, 0 on the back (two-sided materials).", "two sided back face"}},
        {"Texture2D", {"Текстура", "Цвет из текстуры по UV.", "Colour from a texture at UV.", "sample image map картинка"}},
        {"NormalMap", {"Карта нормалей", "Нормаль из карты нормалей, для входа Normal выхода материала.",
                       "Normal from a normal map, for the Normal input of the output.", "bump normal рельеф"}},
        {"UVCoord", {"UV-координаты", "Текстурные координаты с тайлингом.", "Texture coordinates with tiling.", "texcoord tiling тайлинг"}},
        {"Panner", {"Сдвиг UV", "UV, которые сдвигаются со временем: бегущая текстура.", "UV moving over time: a scrolling texture.", "scroll pan move прокрутка"}},
        {"Time", {"Время", "Секунды с запуска и их синус.", "Seconds since start and their sine.", "animation анимация"}},
        {"Add", {"Сложение", "A + B.", "A + B.", "plus sum плюс"}},
        {"Subtract", {"Вычитание", "A − B.", "A - B.", "minus минус"}},
        {"Multiply", {"Умножение", "A × B: например, цвет × текстура.", "A * B, e.g. colour times texture.", "times product"}},
        {"Divide", {"Деление", "A / B.", "A / B.", "ratio"}},
        {"Power", {"Степень", "Base в степени Exp.", "Base to the power Exp.", "pow exponent"}},
        {"Min", {"Минимум", "Меньшее из A и B.", "The smaller of A and B.", "minimum меньше"}},
        {"Max", {"Максимум", "Большее из A и B.", "The larger of A and B.", "maximum больше"}},
        {"Lerp", {"Смешивание (lerp)", "A при Alpha = 0, B при Alpha = 1, между ними — плавно.",
                  "A at Alpha 0, B at Alpha 1, blended in between.", "mix blend interpolate смешать"}},
        {"Clamp", {"Ограничение (clamp)", "In, зажатое между Min и Max.", "In clamped between Min and Max.", "limit предел"}},
        {"Saturate", {"Ограничение 0..1", "In, зажатое между 0 и 1.", "In clamped to 0..1.", "saturate clamp"}},
        {"OneMinus", {"1 − x", "Инверсия: 1 − In.", "Inversion: 1 - In.", "invert one minus инверсия"}},
        {"Abs", {"Модуль", "Абсолютное значение.", "Absolute value.", "absolute"}},
        {"Frac", {"Дробная часть", "Дробная часть: повторение 0..1.", "Fractional part: repeats 0..1.", "fraction repeat"}},
        {"Sine", {"Синус", "sin(In); без входа — пульсация по времени.", "sin(In); without input it pulses over time.", "sin wave волна"}},
        {"Normalize", {"Нормализация", "Вектор единичной длины.", "Vector of unit length.", "unit length"}},
        {"Step", {"Порог (step)", "0, если In < Edge, иначе 1.", "0 if In < Edge, else 1.", "threshold"}},
        {"Smoothstep", {"Плавный порог", "Плавный переход от 0 к 1 между Min и Max.", "Smooth 0 to 1 transition between Min and Max.", "smooth threshold"}},
        {"Dot", {"Скалярное произведение", "dot(A, B).", "dot(A, B).", "dot product"}},
        {"Fresnel", {"Френель", "Яркость края: сильнее там, где поверхность видна под углом.",
                     "Rim brightness: stronger where the surface is seen at a grazing angle.", "rim edge край ободок"}}};
    return infos;
}

QString NodeHint(const QString& type)
{
    const auto info = NodeInfos().find(type);
    return info == NodeInfos().end() ? QString() : Tr(info->hintRu, info->hintEn);
}

// Список нод с поиском: имя по-русски и по-английски, назначение, группа и синонимы.
// Без текста — все ноды по группам, как в меню; Enter или щелчок добавляет ноду.
class NodeSearchPopup final : public QFrame
{
public:
    static constexpr const char* Paste = "@paste";

    NodeSearchPopup(QWidget* parent, bool canPaste, std::function<void(const QString&)> chosen)
        : QFrame(parent, Qt::Popup), canPaste_(canPaste), chosen_(std::move(chosen))
    {
        setObjectName(QStringLiteral("MaterialGraphNodeSearch"));
        setAttribute(Qt::WA_DeleteOnClose);
        setFrameShape(QFrame::StyledPanel);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);
        filter_ = new QLineEdit(this);
        filter_->setObjectName(QStringLiteral("MaterialGraphNodeSearchText"));
        filter_->setPlaceholderText(Tr("Поиск: умножение, текстура, fresnel…", "Search: multiply, texture, fresnel…"));
        filter_->setClearButtonEnabled(true);
        filter_->installEventFilter(this);
        list_ = new QListWidget(this);
        list_->setObjectName(QStringLiteral("MaterialGraphNodeSearchList"));
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

    QLineEdit* Filter() const { return filter_; }
    QListWidget* List() const { return list_; }

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
        for (const auto& group : NodeGroups())
        {
            const QString groupName = Tr(group.ru, group.en);
            if (words.isEmpty())
            {
                auto* header = new QListWidgetItem(groupName, list_);
                header->setFlags(Qt::NoItemFlags);
                QFont font = header->font();
                font.setBold(true);
                header->setFont(font);
            }
            for (const QString& type : group.types)
            {
                const NodeInfo info = NodeInfos().value(type);
                const QString name = MaterialGraphWidget::NodeDisplayName(type);
                if (words.isEmpty())
                {
                    Add(QStringLiteral("    ") + Label(type), type, NodeHint(type));
                    continue;
                }
                const QString hay = QStringList{type, QString::fromUtf8(info.ru), groupName, QString::fromUtf8(group.ru),
                                                QString::fromUtf8(group.en), QString::fromUtf8(info.keywords),
                                                QString::fromUtf8(info.hintRu), QString::fromUtf8(info.hintEn)}.join(QLatin1Char(' ')).toCaseFolded();
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
        for (const auto& match : matches) Add(Label(match.type), match.type, NodeHint(match.type));
        if (!words.isEmpty() && matches.isEmpty())
            (new QListWidgetItem(Tr("Ничего не найдено", "Nothing found"), list_))->setFlags(Qt::NoItemFlags);
        for (int row = 0; row < list_->count(); ++row)
            if (list_->item(row)->flags() & Qt::ItemIsSelectable)
            {
                list_->setCurrentRow(row);
                break;
            }
    }

    static QString Label(const QString& type)
    {
        const QString name = MaterialGraphWidget::NodeDisplayName(type);
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

    QLineEdit* filter_ = nullptr;
    QListWidget* list_ = nullptr;
    bool canPaste_ = false, done_ = false;
    std::function<void(const QString&)> chosen_;
};

const QString ClipboardFormat = QStringLiteral("application/x-painengine-material-nodes");
}

QString MaterialGraphWidget::NodeDisplayName(const QString& type)
{
    const auto info = NodeInfos().find(type);
    return info == NodeInfos().end() ? type : Tr(info->ru, type.toUtf8().constData());
}

class MaterialGraphWidget::NodeItem final : public QGraphicsItem
{
public:
    NodeItem(const GraphNodeData& data, MaterialGraphWidget* owner, const QString& textureFile, const QString& problem)
        : data_(data), owner_(owner), problem_(problem)
    {
        if (IsTextureType(data_.type) && !textureFile.isEmpty())
            thumbnail_ = MaterialTextures::Thumbnail(textureFile, Thumbnail);
        textureFile_ = textureFile;
        // Своё имя (параметр, текстура, подпись из готового графа) или название типа на языке редактора.
        const bool ownTitle = data_.type != MasterType && !data_.title.isEmpty() && (!NodeName(data_).isEmpty() || data_.title != data_.type);
        title_ = ownTitle ? data_.title : MaterialGraphWidget::NodeDisplayName(data_.type);
        const QString hint = NodeHint(data_.type);
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
        painter->fillPath(header, CategoryColor(data_.type));

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
            const QColor color = PinColor(pin.type);
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
        else if (data_.type == QStringLiteral("ScalarParam") || data_.type == QStringLiteral("Constant"))
            painter->drawText(content, Qt::AlignCenter, QString::number(data_.properties.value(QStringLiteral("value"), 0.5).toDouble(), 'f', 3));
        else if (VectorSize(data_.type))
            painter->drawText(content, Qt::AlignCenter, VectorText(data_.properties.value(QStringLiteral("value")), VectorSize(data_.type)));
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
                            : IsParameterType(data_.type) || data_.type == QStringLiteral("Constant") ||
                              data_.type == QStringLiteral("UVCoord") || data_.type == QStringLiteral("Panner") ? 22 : 0;
        height_ = contentTop_ + content + 6;
    }

    GraphNodeData data_;
    MaterialGraphWidget* owner_ = nullptr;
    QString problem_, title_;
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
    add->setToolTip(Tr("Добавить ноду: список с поиском (также ПКМ по холсту или Tab)",
                       "Add a node: a searchable list (also RMB on the canvas or Tab)"));
    connect(add, &QToolButton::clicked, this, [this, add] {
        ShowNodeSearch(add->mapToGlobal(QPoint(0, add->height())), view_->mapToScene(view_->viewport()->rect().center()));
    });
    bar->addWidget(add);

    auto history = [this, bar](const char* name, const char* glyph, const QString& tip, void (MaterialGraphWidget::*slot)()) {
        auto* button = new QToolButton(this);
        button->setObjectName(QString::fromLatin1(name));
        button->setText(QString::fromUtf8(glyph));
        QFont font = button->font();
        // Стрелки в обычном размере шрифта почти не видны.
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * 1.4);
        else if (font.pixelSize() > 0) font.setPixelSize(font.pixelSize() * 7 / 5);
        button->setFont(font);
        button->setToolTip(tip);
        button->setEnabled(false);
        connect(button, &QToolButton::clicked, this, slot);
        bar->addWidget(button);
        return button;
    };
    undoButton_ = history("MaterialGraphUndo", "↶", Tr("Отменить (Ctrl+Z)", "Undo (Ctrl+Z)"), &MaterialGraphWidget::Undo);
    redoButton_ = history("MaterialGraphRedo", "↷", Tr("Повторить (Ctrl+Y)", "Redo (Ctrl+Y)"), &MaterialGraphWidget::Redo);
    auto* frame = new QToolButton(this);
    frame->setText(Tr("Показать всё", "Frame all"));
    frame->setToolTip(Tr("Уместить граф в окно, а при выделении — выделенные ноды (F)", "Fit the graph, or the selection, into view (F)"));
    connect(frame, &QToolButton::clicked, this, [this] { FrameNodes(); });
    bar->addWidget(frame);

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
    undo_.clear();
    redo_.clear();
    undoKey_.clear();
    problems_.clear();
    ReplaceModel(model);
    UpdateHistoryButtons();
}

void MaterialGraphWidget::ReplaceModel(const EditorCore::MaterialGraphModel& model)
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
    bool changed = false;
    for (const auto& node : model_.Nodes())
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
        model_.FindNode(node.id)->properties[key] = stored;
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
    for (const auto& node : model_.Nodes())
    {
        if (!IsTextureType(node.type) || NodeName(node) != textureName) continue;
        GraphNodeData* data = model_.FindNode(node.id);
        if (data->properties.value(QStringLiteral("texture")).toString() == gamePath) continue;
        if (!changed) Checkpoint();
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
    // Выделение переживает пересборку: после правки свойства или вставки ноды остаются выделенными.
    const QList<int> selected = SelectedNodes();
    // Сначала связи: scene_->clear() удаляет и их.
    links_.clear();
    dragLink_ = nullptr;
    scene_->clear();
    for (const auto& node : model_.Nodes())
        scene_->addItem(new NodeItem(node, this, textures_.value(NodeName(node)), problems_.value(node.id)));
    UpdateConnections();
    SelectNodes(selected);
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
    else if (const int size = VectorSize(type))
    {
        named("name", QStringLiteral("Vector"));
        if (!properties.contains(QStringLiteral("value"))) properties[QStringLiteral("value")] = QVariantList(size, 0.0);
    }
    else if (type == QStringLiteral("Constant"))
    {
        if (!properties.contains(QStringLiteral("value"))) properties[QStringLiteral("value")] = 1.0;
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
    Checkpoint();
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
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin) return true; // уже есть
    Checkpoint(linkKey_);

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
    // Выход материала не удаляется.
    const bool any = std::any_of(ids.begin(), ids.end(), [this](int id) {
        const GraphNodeData* node = model_.FindNode(id);
        return node && node->type != MasterType;
    });
    if (!any) return;
    Checkpoint();
    for (const int id : ids)
    {
        const GraphNodeData* node = model_.FindNode(id);
        if (node && node->type != MasterType) model_.RemoveNode(id);
    }
    RebuildScene();
    NotifyStructure();
}

bool MaterialGraphWidget::RenameNode(int nodeId, const QString& name)
{
    GraphNodeData* node = model_.FindNode(nodeId);
    if (!node || NodeName(*node).isEmpty() || !EditorCore::HlslGraphCompiler::NameProblem(name).isEmpty()) return false;
    if (name == NodeName(*node)) return true;
    if (UniqueName(name) != name) return false;
    Checkpoint();
    node = model_.FindNode(nodeId);
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
    // Снятие связи и её новое подключение — один шаг отмены.
    linkKey_ = QStringLiteral("link:%1").arg(++linkSerial_);
    if (input)
    {
        // Подключённый вход: связь снимается и тянется дальше от своего выхода, как в Unreal.
        for (const auto& c : model_.Connections())
            if (c.toNode == nodeId && c.toPin == pin)
            {
                Checkpoint(linkKey_);
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
    if (PinAt(scenePos, nodeId, pin, input) && input != linkFromInput_)
    {
        if (linkFromInput_) Connect(nodeId, pin, linkNode_, linkPin_);
        else Connect(linkNode_, linkPin_, nodeId, pin);
    }
    // Связь сняли и вернули на тот же вход: граф прежний, пустой шаг отмены не нужен.
    if (undoKey_ == linkKey_ && !undo_.isEmpty() && undo_.last() == model_.ExportToJson())
    {
        undo_.removeLast();
        undoKey_.clear();
        UpdateHistoryButtons();
    }
    linkKey_.clear();
}

bool MaterialGraphWidget::eventFilter(QObject* watched, QEvent* event)
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
        undoKey_.clear(); // новая правка мышью не сливается с правкой из настроек
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
            pressSnapshot_ = model_.ExportToJson();
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
            // Перетаскивание нод — один шаг отмены: граф до нажатия.
            if (!pressSnapshot_.isEmpty()) PushSnapshot(pressSnapshot_, {});
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

void MaterialGraphWidget::ShowCanvasMenu(const QPoint& screenPos, const QPointF& scenePos)
{
    ShowNodeSearch(screenPos, scenePos);
}

void MaterialGraphWidget::ShowNodeSearch(const QPoint& screenPos, const QPointF& scenePos)
{
    auto* popup = new NodeSearchPopup(this, CanPaste(), [this, scenePos](const QString& type) {
        if (type == QLatin1String(NodeSearchPopup::Paste)) PasteNodes(scenePos);
        else SelectNodes({AddNode(type, scenePos)});
    });
    popup->Open(screenPos);
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
    // Команды правки — для выделения, если нода в нём, иначе для одной ноды.
    if (!SelectedNodes().contains(nodeId)) SelectNodes({nodeId});
    const QList<int> chosen = SelectedNodes();
    const bool onlyOutput = std::all_of(chosen.begin(), chosen.end(), [this](int id) {
        const GraphNodeData* n = model_.FindNode(id);
        return !n || n->type == MasterType;
    });
    menu.addSeparator();
    menu.addAction(Tr("Копировать\tCtrl+C", "Copy\tCtrl+C"), this, [this, chosen] { CopyNodes(chosen); })->setEnabled(!onlyOutput);
    menu.addAction(Tr("Дублировать\tCtrl+D", "Duplicate\tCtrl+D"), this, [this, chosen] { DuplicateNodes(chosen); })->setEnabled(!onlyOutput);
    menu.addAction(Tr("Удалить\tDel", "Delete\tDel"), this, [this, chosen] { RemoveNodes(chosen); })->setEnabled(!onlyOutput);
    menu.exec(screenPos);
}

void MaterialGraphWidget::RequestRename(int nodeId)
{
    const GraphNodeData* node = model_.FindNode(nodeId);
    if (!node) return;
    bool accepted = false;
    const QString name = QInputDialog::getText(this, Tr("Имя в HLSL", "HLSL name"), Tr("Имя:", "Name:"),
                                               QLineEdit::Normal, NodeName(*node), &accepted).trimmed();
    if (!accepted || RenameNode(nodeId, name)) return;
    QString problem = EditorCore::HlslGraphCompiler::NameProblem(name);
    if (problem.isEmpty()) problem = Tr("Имя «%1» уже есть в графе.", "The name \"%1\" is already in the graph.").arg(name);
    QMessageBox::warning(this, Tr("Имя не подходит", "Invalid name"), problem);
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
        Checkpoint();
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
        Checkpoint();
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
        QVector<double> values{node->properties.value(keyX, uv ? 1.0 : 0.5).toDouble(), node->properties.value(keyY, uv ? 1.0 : 0.0).toDouble()};
        if (!EditValues(this, uv ? Tr("Тайлинг UV", "UV tiling") : Tr("Скорость сдвига UV", "UV pan speed"),
                        {QStringLiteral("U"), QStringLiteral("V")}, values) || !(node = model_.FindNode(nodeId)))
            return;
        Checkpoint();
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
        if (!accepted || !(node = model_.FindNode(nodeId))) return;
        Checkpoint();
        node->properties[QStringLiteral("value")] = value;
        RebuildScene();
        NotifyStructure();
    }
    else if (const int size = VectorSize(node->type))
    {
        const QVariantList stored = node->properties.value(QStringLiteral("value")).toList();
        QVector<double> values;
        for (int i = 0; i < size; ++i) values << (i < stored.size() ? stored[i].toDouble() : 0.0);
        if (!EditValues(this, name, QStringList{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")}.mid(0, size), values) ||
            !(node = model_.FindNode(nodeId)))
            return;
        QVariantList list;
        std::array<float, 4> value{};
        for (int i = 0; i < size; ++i)
        {
            list << values[i];
            value[i] = static_cast<float>(values[i]);
        }
        Checkpoint();
        node->properties[QStringLiteral("value")] = list;
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
    ReplaceModel(model);
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
    Checkpoint(); // готовый граф и ответ ИИ отменяются, как любая правка
    ReplaceModel(model);
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
    for (const auto& group : NodeGroups())
        for (const QString& type : group.types)
        {
            QStringList inputs, outputs;
            for (const auto& pin : NodePins(type)) (pin.isInput ? inputs : outputs) << pin.name + QLatin1Char(':') + pin.type;
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

bool MaterialGraphWidget::PasteFromAi(const QString& text)
{
    QString json = text.trimmed();
    static const QRegularExpression block(QStringLiteral(R"(```(?:json)?\s*([\s\S]*?)\s*```)"));
    if (const auto match = block.match(json); match.hasMatch()) json = match.captured(1);
    EditorCore::MaterialGraphModel model;
    if (!model.ImportFromJson(json)) return false;
    const auto& nodes = model.Nodes();
    if (std::count_if(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return n.type == MasterType; }) != 1) return false;
    // Выдуманная нейросетью нода молча дала бы 0 в HLSL: такой ответ не принимается.
    if (std::any_of(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return !EditorCore::HlslGraphCompiler::IsKnownNode(n.type); }))
        return false;
    Checkpoint(); // готовый граф и ответ ИИ отменяются, как любая правка
    ReplaceModel(model);
    NotifyStructure();
    return true;
}

void MaterialGraphWidget::Checkpoint(const QString& mergeKey)
{
    if (!mergeKey.isEmpty() && mergeKey == undoKey_) return;
    PushSnapshot(model_.ExportToJson(), mergeKey);
}

void MaterialGraphWidget::PushSnapshot(const QString& snapshot, const QString& mergeKey)
{
    constexpr int Limit = 200;
    undoKey_ = mergeKey;
    redo_.clear();
    if (undo_.isEmpty() || undo_.last() != snapshot) undo_ << snapshot;
    while (undo_.size() > Limit) undo_.removeFirst();
    UpdateHistoryButtons();
}

void MaterialGraphWidget::UpdateHistoryButtons()
{
    if (undoButton_) undoButton_->setEnabled(CanUndo());
    if (redoButton_) redoButton_->setEnabled(CanRedo());
}

void MaterialGraphWidget::Undo()
{
    if (undo_.isEmpty() || dragLink_) return;
    redo_ << model_.ExportToJson();
    RestoreSnapshot(undo_.takeLast());
}

void MaterialGraphWidget::Redo()
{
    if (redo_.isEmpty() || dragLink_) return;
    undo_ << model_.ExportToJson();
    RestoreSnapshot(redo_.takeLast());
}

void MaterialGraphWidget::RestoreSnapshot(const QString& snapshot)
{
    EditorCore::MaterialGraphModel restored;
    if (!restored.ImportFromJson(snapshot)) return;
    const EditorCore::MaterialGraphModel before = model_;
    model_ = restored;
    undoKey_.clear();
    RebuildScene();
    UpdateHistoryButtons();
    // Значения параметров и текстуры живут и в материале: отмена возвращает их и туда.
    for (const auto& node : model_.Nodes())
    {
        const GraphNodeData* old = before.FindNode(node.id);
        if (!old || old->type != node.type || NodeName(*old) != NodeName(node) || NodeName(node).isEmpty()) continue;
        if (IsParameterType(node.type) && old->properties != node.properties && parameterEdited)
            parameterEdited(NodeName(node), ParameterValue(node));
        const QString was = old->properties.value(QStringLiteral("texture")).toString();
        const QString now = node.properties.value(QStringLiteral("texture")).toString();
        if (IsTextureType(node.type) && was != now && textureRestored) textureRestored(NodeName(node), now);
    }
    NotifyStructure();
}

QList<int> MaterialGraphWidget::SelectedNodes() const
{
    QList<int> ids;
    for (auto* item : scene_->selectedItems())
        if (auto* node = dynamic_cast<NodeItem*>(item)) ids << node->Data().id;
    std::sort(ids.begin(), ids.end());
    return ids;
}

void MaterialGraphWidget::SelectNodes(const QList<int>& ids)
{
    scene_->clearSelection();
    for (auto* item : scene_->items())
        if (auto* node = dynamic_cast<NodeItem*>(item); node && ids.contains(node->Data().id)) node->setSelected(true);
}

QString MaterialGraphWidget::ClipboardJson(const QList<int>& ids) const
{
    // Тот же формат, что у файла графа, с меткой: вставка узнаёт его и в обычном тексте.
    const QJsonObject graph = QJsonDocument::fromJson(model_.ExportToJson().toUtf8()).object();
    QJsonArray nodes, connections;
    QSet<int> chosen;
    for (const auto& value : graph.value(QStringLiteral("nodes")).toArray())
    {
        const QJsonObject node = value.toObject();
        const int id = node.value(QStringLiteral("id")).toInt();
        if (!ids.contains(id) || node.value(QStringLiteral("type")).toString() == MasterType) continue;
        nodes.append(node);
        chosen.insert(id);
    }
    if (nodes.isEmpty()) return {};
    for (const auto& value : graph.value(QStringLiteral("connections")).toArray())
    {
        const QJsonObject link = value.toObject();
        if (chosen.contains(link.value(QStringLiteral("fromNode")).toInt()) && chosen.contains(link.value(QStringLiteral("toNode")).toInt()))
            connections.append(link);
    }
    QJsonObject clip;
    clip[QStringLiteral("materialGraphNodes")] = 1;
    clip[QStringLiteral("nodes")] = nodes;
    clip[QStringLiteral("connections")] = connections;
    return QString::fromUtf8(QJsonDocument(clip).toJson(QJsonDocument::Indented));
}

QString MaterialGraphWidget::ClipboardGraph() const
{
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    if (!mime) return {};
    const QByteArray data = mime->hasFormat(ClipboardFormat) ? mime->data(ClipboardFormat) : mime->text().toUtf8();
    const QJsonDocument document = QJsonDocument::fromJson(data);
    return document.isObject() && document.object().contains(QStringLiteral("materialGraphNodes")) ? QString::fromUtf8(data) : QString();
}

bool MaterialGraphWidget::CopyNodes(const QList<int>& ids) const
{
    const QString json = ClipboardJson(ids);
    if (json.isEmpty()) return false;
    auto* mime = new QMimeData;
    mime->setData(ClipboardFormat, json.toUtf8());
    mime->setText(json);
    QGuiApplication::clipboard()->setMimeData(mime);
    return true;
}

bool MaterialGraphWidget::CanPaste() const
{
    return !ClipboardGraph().isEmpty();
}

QList<int> MaterialGraphWidget::PasteNodes(const QPointF& topLeft)
{
    return InsertNodes(ClipboardGraph(), topLeft);
}

QList<int> MaterialGraphWidget::DuplicateNodes(const QList<int>& ids)
{
    QRectF bounds;
    for (const int id : ids)
        if (const GraphNodeData* node = model_.FindNode(id); node && node->type != MasterType)
            bounds = bounds.isNull() ? QRectF(node->pos, QSizeF(1, 1)) : bounds.united(QRectF(node->pos, QSizeF(1, 1)));
    return bounds.isNull() ? QList<int>() : InsertNodes(ClipboardJson(ids), bounds.topLeft() + QPointF(2 * GridStep, 2 * GridStep));
}

QList<int> MaterialGraphWidget::InsertNodes(const QString& json, const QPointF& topLeft)
{
    EditorCore::MaterialGraphModel clip;
    if (json.isEmpty() || !clip.ImportFromJson(json)) return {};
    QVector<GraphNodeData> nodes;
    QRectF bounds;
    for (const auto& node : clip.Nodes())
    {
        if (node.type == MasterType || !EditorCore::HlslGraphCompiler::IsKnownNode(node.type)) continue;
        nodes << node;
        bounds = bounds.isNull() ? QRectF(node.pos, QSizeF(1, 1)) : bounds.united(QRectF(node.pos, QSizeF(1, 1)));
    }
    if (nodes.isEmpty()) return {};

    Checkpoint();
    const QPointF offset = Snap(topLeft) - Snap(bounds.topLeft());
    QHash<int, int> ids; // id в буфере -> новый id
    for (const auto& node : nodes)
    {
        QVariantMap properties = node.properties;
        QString title = node.title;
        const QString name = NodeName(node);
        if (!name.isEmpty())
        {
            const QString key = IsTextureType(node.type) ? QStringLiteral("textureName") : QStringLiteral("name");
            // Копия ноды текстуры читает ту же текстуру, если имя свободно или занято текстурой;
            // параметр с тем же именем был бы тем же параметром, поэтому получает новое имя.
            QList<const GraphNodeData*> holders;
            for (const auto& existing : model_.Nodes())
                if (NodeName(existing) == name) holders << &existing;
            const bool share = IsTextureType(node.type) && !holders.isEmpty() &&
                               std::all_of(holders.begin(), holders.end(), [](const GraphNodeData* n) { return IsTextureType(n->type); });
            if (share)
            {
                const QString texture = holders.first()->properties.value(QStringLiteral("texture")).toString();
                if (texture.isEmpty()) properties.remove(QStringLiteral("texture"));
                else properties[QStringLiteral("texture")] = texture;
            }
            else if (!holders.isEmpty())
            {
                const QString unique = UniqueName(name);
                properties[key] = unique;
                if (title == name) title = unique;
            }
        }
        ids.insert(node.id, model_.AddNode(node.type, title, Snap(node.pos + offset), properties));
    }
    for (const auto& c : clip.Connections())
        if (ids.contains(c.fromNode) && ids.contains(c.toNode))
            model_.AddConnection(ids.value(c.fromNode), c.fromPin, ids.value(c.toNode), c.toPin);
    RebuildScene();
    QList<int> created = ids.values();
    std::sort(created.begin(), created.end());
    SelectNodes(created);
    NotifyStructure();
    return created;
}

QPointF MaterialGraphWidget::CursorScenePos() const
{
    const QPoint local = view_->viewport()->mapFromGlobal(QCursor::pos());
    return view_->mapToScene(view_->viewport()->rect().contains(local) ? local : view_->viewport()->rect().center());
}

void MaterialGraphWidget::FrameNodes()
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

void MaterialGraphWidget::SetNodeProblems(const QHash<int, QString>& problems)
{
    if (problems == problems_) return;
    problems_ = problems;
    RebuildScene();
}

void MaterialGraphWidget::FocusNode(int nodeId)
{
    NodeItem* item = FindItem(nodeId);
    if (!item) return;
    SelectNodes({nodeId});
    view_->centerOn(item);
    view_->setFocus();
}
