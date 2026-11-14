#include "MaterialTextureTools.h"
#include "EditorTheme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
const QStringList TextureFilters{QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                 QStringLiteral("*.bmp"), QStringLiteral("*.tga")};
const char* const TextureDialogFilter = "Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)";

const QRegularExpression& SurfacePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\bvoid\s+Surface\s*\(\s*MaterialInput\s+(\w+)\s*,\s*inout\s+MaterialOutput\s+(\w+)\s*\))"));
    return pattern;
}

QRegularExpression Identifier(const QString& name)
{
    return QRegularExpression(QStringLiteral(R"(\b%1\b)").arg(QRegularExpression::escape(name)));
}

// Закрывающая скобка тела функции; скобки в комментариях не считаются.
int FunctionEnd(const QString& source, int from)
{
    int depth = 0;
    for (int i = from; i < source.size(); ++i)
    {
        if (source[i] == QLatin1Char('/') && i + 1 < source.size())
        {
            if (source[i + 1] == QLatin1Char('/'))
            {
                i = source.indexOf(QLatin1Char('\n'), i);
                if (i < 0) return -1;
                continue;
            }
            if (source[i + 1] == QLatin1Char('*'))
            {
                i = source.indexOf(QStringLiteral("*/"), i);
                if (i < 0) return -1;
                ++i;
                continue;
            }
        }
        if (source[i] == QLatin1Char('{')) ++depth;
        else if (source[i] == QLatin1Char('}') && --depth == 0) return i;
    }
    return -1;
}

int LineStart(const QString& source, int position)
{
    return position > 0 ? source.lastIndexOf(QLatin1Char('\n'), position - 1) + 1 : 0;
}

int NextTextureRegister(const QString& source)
{
    static const QRegularExpression used(QStringLiteral(R"(register\s*\(\s*t(\d+)\s*\))"));
    QSet<int> occupied{32}; // t0-t5 and point shadow t32 belong to the engine.
    int last = 5;
    for (auto match = used.globalMatch(source); match.hasNext();)
    {
        const int slot = match.next().captured(1).toInt();
        occupied.insert(slot);
        if (slot >= 6 && slot < 128) last = std::max(last, slot);
    }
    for (int offset = 0; offset < 122; ++offset)
    {
        const int slot = 6 + (last - 5 + offset) % 122;
        if (!occupied.contains(slot)) return slot;
    }
    return -1;
}

QPixmap SlotImage(const QString& path, const QString& file, int size)
{
    const auto& theme = EditorTheme::Instance();
    const auto& pal = theme.Palette();
    const QImage image = path.isEmpty() ? QImage() : MaterialTextures::Thumbnail(file, size);
    QPixmap pixmap(size, size);
    pixmap.fill(path.isEmpty() ? pal.bgPanel : pal.bgDarkest);
    QPainter painter(&pixmap);
    if (!image.isNull())
    {
        painter.drawImage((size - image.width()) / 2, (size - image.height()) / 2, image);
    }
    else if (!path.isEmpty())
    {
        painter.setPen(pal.textDisabled);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("?")); // файл пропал или не читается
    }
    return pixmap;
}

QString DroppedTexture(const QMimeData* mime)
{
    if (!mime->hasUrls() || mime->urls().size() != 1) return {};
    const QString file = mime->urls().first().toLocalFile();
    return MaterialTextures::IsTextureFile(file) ? file : QString();
}
}

namespace MaterialTextures
{
QString RoleLabel(Role role)
{
    switch (role)
    {
    case Role::Opacity: return QStringLiteral("Прозрачность (красный канал)");
    case Role::Specular: return QStringLiteral("Сила блика (красный канал)");
    case Role::Emissive: return QStringLiteral("Свечение");
    default: return QStringLiteral("Цвет");
    }
}

QString DefaultName(const QString& source, Role role)
{
    const QString base = role == Role::Opacity ? QStringLiteral("OpacityMap")
                       : role == Role::Specular ? QStringLiteral("SpecularMap")
                       : role == Role::Emissive ? QStringLiteral("EmissiveMap")
                                                : QStringLiteral("BaseMap");
    QString name = base;
    for (int i = 2; source.contains(Identifier(name)); ++i) name = base + QString::number(i);
    return name;
}

QString NameProblem(const QString& source, const QString& name)
{
    static const QRegularExpression valid(QStringLiteral(R"(^[A-Za-z_]\w*$)"));
    if (!valid.match(name).hasMatch())
        return QStringLiteral("Имя слота: латинские буквы, цифры и «_», первой не может быть цифра.");
    if (source.contains(Identifier(name)))
        return QStringLiteral("Имя %1 уже встречается в HLSL.").arg(name);
    return {};
}

bool IsTextureFile(const QString& path)
{
    return QDir::match(TextureFilters, QFileInfo(path).fileName());
}

QImage Thumbnail(const QString& file, int size)
{
    QImageReader reader(file);
    reader.setAutoTransform(true);
    const QSize original = reader.size();
    if (original.isValid()) reader.setScaledSize(original.scaled(size, size, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    return image.isNull() ? image : image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

bool AddSlot(QString& source, const QString& name, Role role, QString& error)
{
    error = NameProblem(source, name);
    if (!error.isEmpty()) return false;
    const int slot = NextTextureRegister(source);
    if (slot < 0)
    {
        error = QStringLiteral("Нет свободных слотов текстур материала (максимум 121).");
        return false;
    }

    const auto surface = SurfacePattern().match(source);
    const int end = surface.hasMatch() ? FunctionEnd(source, static_cast<int>(surface.capturedEnd())) : -1;
    if (end < 0)
    {
        error = QStringLiteral("В HLSL не найдена функция void Surface(MaterialInput, inout MaterialOutput).");
        return false;
    }

    const QString input = surface.captured(1), output = surface.captured(2);
    const QString sample = QStringLiteral("%1.Sample(MaterialSampler, %2.UV)").arg(name, input);
    QStringList lines;
    if (role == Role::Color)
    {
        const QString color = name.left(1).toLower() + name.mid(1) + QStringLiteral("Color");
        lines << QStringLiteral("float4 %1 = %2;").arg(color, sample)
              << QStringLiteral("%1.Albedo *= %2.rgb;").arg(output, color)
              << QStringLiteral("%1.Alpha *= %2.a;").arg(output, color);
    }
    else if (role == Role::Opacity) lines << QStringLiteral("%1.Alpha *= %2.r;").arg(output, sample);
    else if (role == Role::Specular) lines << QStringLiteral("%1.SpecularWeight *= %2.r;").arg(output, sample);
    else lines << QStringLiteral("%1.Emissive += %2.rgb;").arg(output, sample);

    // Правки снизу вверх: позиции выше остаются верными.
    int bodyEnd = end;
    while (bodyEnd > 0 && (source[bodyEnd - 1] == QLatin1Char(' ') || source[bodyEnd - 1] == QLatin1Char('\t'))) --bodyEnd;
    QString body = bodyEnd > 0 && source[bodyEnd - 1] != QLatin1Char('\n') ? QStringLiteral("\n") : QString();
    for (const QString& line : lines) body += QStringLiteral("    ") + line + QLatin1Char('\n');
    source.replace(bodyEnd, end - bodyEnd, body);

    const QString declaration = QStringLiteral("Texture2D %1 : register(t%2);\n").arg(name).arg(slot);
    static const QRegularExpression texture(QStringLiteral(R"(^[ \t]*Texture2D\b[^\n]*\n)"), QRegularExpression::MultilineOption);
    int afterTextures = -1;
    for (auto match = texture.globalMatch(source); match.hasNext();)
    {
        const auto next = match.next();
        if (next.capturedStart() < surface.capturedStart()) afterTextures = static_cast<int>(next.capturedEnd());
    }
    if (afterTextures >= 0) source.insert(afterTextures, declaration);
    else source.insert(LineStart(source, static_cast<int>(surface.capturedStart())), declaration + QLatin1Char('\n'));

    static const QRegularExpression emissive(QStringLiteral(R"(^[ \t]*#[ \t]*define[ \t]+MATERIAL_EMISSIVE\b)"), QRegularExpression::MultilineOption);
    if (role == Role::Emissive && !source.contains(emissive))
    {
        const int position = LineStart(source, static_cast<int>(SurfacePattern().match(source).capturedStart()));
        source.insert(position, QStringLiteral("#define MATERIAL_EMISSIVE\n\n"));
    }
    return true;
}

bool RemoveSlot(QString& source, const QString& name, QString& error)
{
    // Убираются только простые формы, какие пишет AddSlot: выражение с чем-то ещё вырезать нельзя,
    // иначе вместе с текстурой молча пропадёт остальная логика (например, умножение на Tint).
    const QString id = QRegularExpression::escape(name);
    const QString sample = QStringLiteral(R"(%1\.Sample\([^()]*\)(?:\.\w+)?)").arg(id);
    const QRegularExpression declaration(QStringLiteral(R"(^\s*Texture2D\s+%1\b[^;]*;\s*$)").arg(id));
    const QRegularExpression local(QStringLiteral(R"(^\s*(?:const\s+)?float[234]?\s+(\w+)\s*=\s*%1\s*;\s*$)").arg(sample));

    const QStringList lines = source.split(QLatin1Char('\n'));
    QStringList names{id}, values{sample};
    for (const QString& line : lines)
        if (const auto match = local.match(line); match.hasMatch())
        {
            names.push_back(match.captured(1));
            values.push_back(match.captured(1) + QStringLiteral(R"((?:\.\w+)?)"));
        }
    const QRegularExpression used(QStringLiteral(R"(\b(?:%1)\b)").arg(names.join(QLatin1Char('|'))));
    const QRegularExpression assignment(QStringLiteral(R"(^\s*\w+\.\w+\s*[*+]?=\s*(?:%1)\s*;\s*$)").arg(values.join(QLatin1Char('|'))));

    QStringList kept;
    for (const QString& line : lines)
    {
        if (!line.contains(used)) kept.push_back(line);
        else if (!line.contains(declaration) && !line.contains(local) && !line.contains(assignment))
        {
            error = QStringLiteral("Текстуру %1 нельзя убрать автоматически, отредактируйте HLSL вручную:\n%2").arg(name, line.trimmed());
            return false;
        }
    }

    // Без записи в Emissive настройка только уводила бы материал из deferred в forward.
    static const QRegularExpression emissive(QStringLiteral(R"(\.Emissive\b)"));
    static const QRegularExpression define(QStringLiteral(R"(^[ \t]*#[ \t]*define[ \t]+MATERIAL_EMISSIVE\b[^\n]*\n\n?)"), QRegularExpression::MultilineOption);
    source = kept.join(QLatin1Char('\n'));
    if (!source.contains(emissive)) source.remove(define);
    return true;
}
}

MaterialTextureSlot::MaterialTextureSlot(const QString& name, const QString& path, const QString& file, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("MaterialSlot_%1").arg(name));
    setAcceptDrops(true);
    constexpr int Size = 56;

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    auto* thumbnail = new QToolButton(this);
    thumbnail->setObjectName(QStringLiteral("MaterialTextureThumb_%1").arg(name));
    thumbnail->setIconSize({Size, Size});
    thumbnail->setIcon(SlotImage(path, file, Size));
    thumbnail->setToolTip(QStringLiteral("Выбрать текстуру. Можно перетащить файл на эту строку."));
    layout->addWidget(thumbnail);

    auto* column = new QVBoxLayout;
    auto* text = new QLineEdit(path, this);
    text->setObjectName(QStringLiteral("MaterialTexture_%1").arg(name));
    text->setReadOnly(true);
    text->setPlaceholderText(QStringLiteral("нет текстуры (белая)"));
    text->setToolTip(path);
    column->addWidget(text);

    auto* buttons = new QHBoxLayout;
    auto* chooseButton = new QPushButton(QStringLiteral("Выбрать…"), this);
    chooseButton->setObjectName(QStringLiteral("MaterialTextureChoose_%1").arg(name));
    auto* clearButton = new QPushButton(QStringLiteral("Очистить"), this);
    clearButton->setObjectName(QStringLiteral("MaterialTextureClear_%1").arg(name));
    clearButton->setEnabled(!path.isEmpty());
    auto* removeButton = new QPushButton(QStringLiteral("Удалить слот"), this);
    removeButton->setObjectName(QStringLiteral("MaterialTextureRemove_%1").arg(name));
    removeButton->setToolTip(QStringLiteral("Убрать текстуру из HLSL"));
    buttons->addWidget(chooseButton);
    buttons->addWidget(clearButton);
    buttons->addWidget(removeButton);
    buttons->addStretch(1);
    column->addLayout(buttons);
    layout->addLayout(column, 1);

    const auto pick = [this] { if (choose) choose(); };
    connect(thumbnail, &QToolButton::clicked, this, pick);
    connect(chooseButton, &QPushButton::clicked, this, pick);
    connect(clearButton, &QPushButton::clicked, this, [this] { if (clear) clear(); });
    connect(removeButton, &QPushButton::clicked, this, [this] { if (remove) remove(); });
}

void MaterialTextureSlot::Highlight(bool on)
{
    setStyleSheet(on ? QStringLiteral("#%1 { border: 1px solid #4a9eff; }").arg(objectName()) : QString());
}

void MaterialTextureSlot::dragEnterEvent(QDragEnterEvent* event)
{
    if (DroppedTexture(event->mimeData()).isEmpty()) return;
    event->setDropAction(Qt::CopyAction);
    event->accept();
    Highlight(true);
}

void MaterialTextureSlot::dragLeaveEvent(QDragLeaveEvent* event)
{
    Highlight(false);
    QFrame::dragLeaveEvent(event);
}

void MaterialTextureSlot::dropEvent(QDropEvent* event)
{
    Highlight(false);
    const QString file = DroppedTexture(event->mimeData());
    if (file.isEmpty()) return;
    event->setDropAction(Qt::CopyAction);
    event->accept();
    if (dropped) dropped(file);
}

MaterialAddTextureDialog::MaterialAddTextureDialog(const QString& source, QWidget* parent)
    : QDialog(parent), source_(source)
{
    setObjectName(QStringLiteral("MaterialAddTexture"));
    setWindowTitle(QStringLiteral("Новая текстура материала"));

    auto* layout = new QFormLayout(this);
    role_ = new QComboBox(this);
    role_->setObjectName(QStringLiteral("MaterialAddTextureRole"));
    for (const auto role : {MaterialTextures::Role::Color, MaterialTextures::Role::Opacity,
                            MaterialTextures::Role::Specular, MaterialTextures::Role::Emissive})
        role_->addItem(MaterialTextures::RoleLabel(role));
    name_ = new QLineEdit(MaterialTextures::DefaultName(source_, MaterialTextures::Role::Color), this);
    name_->setObjectName(QStringLiteral("MaterialAddTextureName"));
    problem_ = new QLabel(this);
    problem_->setWordWrap(true);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(QStringLiteral("Назначение"), role_);
    layout->addRow(QStringLiteral("Имя в HLSL"), name_);
    layout->addRow(problem_);
    layout->addRow(buttons_);

    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(role_, &QComboBox::currentIndexChanged, this, [this] {
        if (!nameEdited_) name_->setText(MaterialTextures::DefaultName(source_, Role()));
    });
    connect(name_, &QLineEdit::textEdited, this, [this] { nameEdited_ = true; });
    connect(name_, &QLineEdit::textChanged, this, [this] { Validate(); });
    Validate();
}

MaterialTextures::Role MaterialAddTextureDialog::Role() const
{
    return static_cast<MaterialTextures::Role>(role_->currentIndex());
}

QString MaterialAddTextureDialog::Name() const { return name_->text().trimmed(); }

void MaterialAddTextureDialog::Validate()
{
    const QString problem = MaterialTextures::NameProblem(source_, Name());
    problem_->setText(problem);
    problem_->setVisible(!problem.isEmpty());
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty());
}
