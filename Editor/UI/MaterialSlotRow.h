#pragma once

#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QApplication>
#include <algorithm>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <functional>

// Слот материала части модели: индекс материала в файле модели, его имя и назначенный .mat (пусто — стандартный).
struct MaterialSlotInfo
{
    int index = 0;
    QString name;
    QString path;
    bool operator==(const MaterialSlotInfo& o) const { return index == o.index && name == o.name && path == o.path; }
};

namespace MaterialSlotDrag
{
// Перетаскивание слота на слот передаёт /Game путь в собственном формате (и текстом, для полей ввода).
inline const QString& MimeType()
{
    static const QString type = QStringLiteral("application/x-engine-material-path");
    return type;
}

inline bool IsMaterialPath(const QString& path)
{
    return path.startsWith(QStringLiteral("/Game/")) && path.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive);
}

// .mat из Контент-браузера (файл на диске внутри Content), из другого слота или текстом "/Game/….mat".
inline QString DroppedMaterial(const QMimeData* mime, const QString& contentRoot)
{
    if (!mime) return {};
    if (mime->hasFormat(MimeType()))
    {
        const QString path = QString::fromUtf8(mime->data(MimeType()));
        return IsMaterialPath(path) ? path : QString();
    }
    if (mime->hasUrls())
    {
        if (mime->urls().size() != 1 || contentRoot.isEmpty()) return {};
        const QString file = mime->urls().first().toLocalFile();
        if (!file.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive)) return {};
        const QString relative = QDir(contentRoot).relativeFilePath(file);
        if (relative.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(relative)) return {};
        return QStringLiteral("/Game/") + QDir::fromNativeSeparators(relative);
    }
    const QString text = mime->text().trimmed();
    return IsMaterialPath(text) ? text : QString();
}

inline QString DiskPath(const QString& contentRoot, const QString& path)
{
    if (contentRoot.isEmpty() || !IsMaterialPath(path)) return {};
    return QDir(contentRoot).filePath(path.mid(6));
}
}

// Подпись, которая сокращает длинный текст многоточием вместо того, чтобы раздвигать инспектор.
class MaterialElidedLabel : public QLabel
{
public:
    explicit MaterialElidedLabel(QWidget* parent = nullptr, Qt::TextElideMode mode = Qt::ElideRight)
        : QLabel(parent), mode_(mode)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(24);
    }

    void SetFullText(const QString& text)
    {
        if (full_ == text) return;
        full_ = text;
        setText(text);
        update();
    }
    const QString& FullText() const { return full_; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, foregroundRole()));
        const QRect area = contentsRect();
        painter.drawText(area, Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(full_, mode_, area.width()));
    }

private:
    QString full_;
    Qt::TextElideMode mode_;
};

// Одна строка: «индекс · имя части · материал · […] [✎] [✕]». Принимает .mat перетаскиванием,
// сама перетаскивается на другой слот (копирует материал), двойной щелчок открывает материал.
class MaterialSlotRow : public QFrame
{
public:
    std::function<void(const QString&)> dropped;
    std::function<void()> choose;
    std::function<void()> edit;
    std::function<void()> clear;
    std::function<void()> activate; // кнопка-глаз: показать во вьюпорте только части с этим слотом
    std::function<QString()> contentRoot;

    MaterialSlotRow(const MaterialSlotInfo& info, QWidget* parent)
        : QFrame(parent), info_(info)
    {
        const QString id = QString::number(info.index);
        setObjectName(QStringLiteral("MaterialSlot_") + id);
        setProperty("materialSlotRow", true);
        setAcceptDrops(true);
        setFixedHeight(26);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 1, 2, 1);
        layout->setSpacing(4);

        indexLabel_ = new QLabel(id, this);
        indexLabel_->setObjectName(QStringLiteral("MaterialSlotIndex_") + id);
        indexLabel_->setFixedWidth(fontMetrics().horizontalAdvance(QStringLiteral("000")));
        indexLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(indexLabel_);

        nameLabel_ = new MaterialElidedLabel(this);
        nameLabel_->setObjectName(QStringLiteral("MaterialSlotName_") + id);
        layout->addWidget(nameLabel_, 2);

        pathLabel_ = new MaterialElidedLabel(this, Qt::ElideLeft);
        pathLabel_->setObjectName(QStringLiteral("MaterialSlotPath_") + id);
        layout->addWidget(pathLabel_, 3);

        const auto tool = [this, &id, layout](const QString& name, const QString& glyph) {
            auto* button = new QToolButton(this);
            button->setObjectName(name + id);
            button->setText(glyph);
            button->setFixedSize(22, 22);
            button->setAutoRaise(true);
            button->setFocusPolicy(Qt::TabFocus);
            layout->addWidget(button);
            return button;
        };
        isolateButton_ = tool(QStringLiteral("MaterialSlotIsolate_"), QString());
        isolateButton_->setCheckable(true);
        QObject::connect(isolateButton_, &QToolButton::clicked, this, [this] {
            isolateButton_->setChecked(active_); // состояние приходит из снимка сцены
            if (activate) activate();
        });
        chooseButton_ = tool(QStringLiteral("MaterialSlotChoose_"), QStringLiteral("…"));
        editButton_ = tool(QStringLiteral("MaterialSlotEdit_"), QStringLiteral("✎"));
        clearButton_ = tool(QStringLiteral("MaterialSlotClear_"), QStringLiteral("✕"));
        QObject::connect(chooseButton_, &QToolButton::clicked, this, [this] { if (choose) choose(); });
        QObject::connect(editButton_, &QToolButton::clicked, this, [this] { if (edit && !info_.path.isEmpty()) edit(); });
        QObject::connect(clearButton_, &QToolButton::clicked, this, [this] { if (clear && !info_.path.isEmpty()) clear(); });

        isolateButton_->setEnabled(false);
        Retranslate();
        SetInfo(info, true, false);
    }

    const MaterialSlotInfo& Info() const { return info_; }
    bool IsActive() const { return active_; }

    void SetActive(bool on)
    {
        if (active_ == on) return;
        active_ = on;
        isolateButton_->setChecked(on);
        UpdateIsolateIcon();
        setProperty("activeSlot", on);
        style()->unpolish(this);
        style()->polish(this);
    }
    QString SearchText() const { return info_.name + QLatin1Char(' ') + info_.path + QLatin1Char(' ') + QString::number(info_.index); }

    void SetInfo(const MaterialSlotInfo& info, bool exists, bool overridden)
    {
        info_ = info;
        nameLabel_->SetFullText(info.name.isEmpty() ? Tr("Материал %1", "Material %1").arg(info.index) : info.name);
        nameLabel_->setToolTip(Tr("Часть модели: %1 (материал №%2 в файле модели)", "Model part: %1 (material #%2 in the model file)")
                                   .arg(nameLabel_->FullText()).arg(info.index));

        const auto pal = EditorTheme::Instance().Palette();
        QString shown = info.path.isEmpty() ? Tr("по умолчанию", "default") : QFileInfo(info.path).completeBaseName();
        QColor color = pal.textPrimary;
        QString tip = info.path.isEmpty()
            ? Tr("Стандартный материал. Перетащите сюда .mat из контент-браузера или нажмите «…».",
                 "Default material. Drop a .mat from the Content Browser here or press \"…\".")
            : info.path;
        if (info.path.isEmpty()) color = pal.textDisabled;
        else if (!exists)
        {
            color = EditorTheme::Instance().AccentColor();
            tip += QLatin1Char('\n') + Tr("Файл материала не найден на диске", "Material file not found on disk");
        }
        else if (overridden) color = pal.textSecondary;
        if (overridden)
            tip += QLatin1Char('\n') + Tr("Сейчас не действует: общий материал модели перекрывает все слоты.",
                                           "Not in effect: the model-wide material overrides all slots.");
        pathLabel_->SetFullText(shown);
        pathLabel_->setToolTip(tip);
        QPalette p = pathLabel_->palette();
        p.setColor(QPalette::WindowText, color);
        pathLabel_->setPalette(p);
        QFont f = pathLabel_->font();
        f.setItalic(info.path.isEmpty() || overridden);
        pathLabel_->setFont(f);

        QPalette ip = indexLabel_->palette();
        ip.setColor(QPalette::WindowText, pal.textSecondary);
        indexLabel_->setPalette(ip);

        editButton_->setEnabled(!info.path.isEmpty() && exists);
        clearButton_->setEnabled(!info.path.isEmpty());
    }

    void SetIsolationAvailable(bool available)
    {
        isolateButton_->setEnabled(available);
        isolateButton_->setToolTip(available
            ? Tr("Показать во вьюпорте только части с этим материалом (ещё раз — всю модель)",
                 "Show only the parts with this material in the viewport (again: the whole model)")
            : Tr("Изоляция частей требует пересборки игрового модуля с текущим SDK.",
                 "Isolating parts requires rebuilding the game module with the current SDK."));
        UpdateIsolateIcon();
    }

    void Retranslate()
    {
        chooseButton_->setToolTip(Tr("Выбрать материал для этой части…", "Choose a material for this part…"));
        editButton_->setToolTip(Tr("Открыть материал в редакторе (или двойной щелчок по строке)",
                                   "Open the material in the editor (or double-click the row)"));
        clearButton_->setToolTip(Tr("Сбросить на стандартный материал", "Reset to the default material"));
        SetIsolationAvailable(isolateButton_->isEnabled()); // подсказка и цвет глаза под язык и тему
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const QString path = MaterialSlotDrag::DroppedMaterial(event->mimeData(), contentRoot ? contentRoot() : QString());
        if (path.isEmpty() || event->source() == this) return;
        event->setDropAction(Qt::CopyAction);
        event->accept();
        SetDropTarget(true);
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        SetDropTarget(false);
        QFrame::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent* event) override
    {
        SetDropTarget(false);
        const QString path = MaterialSlotDrag::DroppedMaterial(event->mimeData(), contentRoot ? contentRoot() : QString());
        if (path.isEmpty()) return;
        event->setDropAction(Qt::CopyAction);
        event->accept();
        if (path != info_.path && dropped) dropped(path);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) pressPos_ = event->position().toPoint();
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!(event->buttons() & Qt::LeftButton) || info_.path.isEmpty()
            || (event->position().toPoint() - pressPos_).manhattanLength() < QApplication::startDragDistance())
        {
            QFrame::mouseMoveEvent(event);
            return;
        }
        auto* mime = new QMimeData;
        mime->setData(MaterialSlotDrag::MimeType(), info_.path.toUtf8());
        mime->setText(info_.path);
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->setHotSpot(pressPos_);
        drag->exec(Qt::CopyAction);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && edit && editButton_->isEnabled()) edit();
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    void UpdateIsolateIcon()
    {
        const auto pal = EditorTheme::Instance().Palette();
        const QColor color = active_ ? EditorTheme::Instance().AccentColor() : isolateButton_->isEnabled() ? pal.textSecondary : pal.textDisabled;
        isolateButton_->setIcon(IconProvider::EyeIcon(true, color));
    }

    void SetDropTarget(bool on)
    {
        setProperty("dropTarget", on);
        style()->unpolish(this);
        style()->polish(this);
    }

    MaterialSlotInfo info_;
    QLabel* indexLabel_ = nullptr;
    MaterialElidedLabel* nameLabel_ = nullptr;
    MaterialElidedLabel* pathLabel_ = nullptr;
    QToolButton* isolateButton_ = nullptr;
    QToolButton* chooseButton_ = nullptr;
    QToolButton* editButton_ = nullptr;
    QToolButton* clearButton_ = nullptr;
    QPoint pressPos_;
    bool active_ = false;
};
