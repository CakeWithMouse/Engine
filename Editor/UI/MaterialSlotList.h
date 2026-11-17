#pragma once

#include "EditorLocale.h"
#include "EditorTheme.h"

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
        chooseButton_ = tool(QStringLiteral("MaterialSlotChoose_"), QStringLiteral("…"));
        editButton_ = tool(QStringLiteral("MaterialSlotEdit_"), QStringLiteral("✎"));
        clearButton_ = tool(QStringLiteral("MaterialSlotClear_"), QStringLiteral("✕"));
        QObject::connect(chooseButton_, &QToolButton::clicked, this, [this] { if (choose) choose(); });
        QObject::connect(editButton_, &QToolButton::clicked, this, [this] { if (edit && !info_.path.isEmpty()) edit(); });
        QObject::connect(clearButton_, &QToolButton::clicked, this, [this] { if (clear && !info_.path.isEmpty()) clear(); });

        Retranslate();
        SetInfo(info, true, false);
    }

    const MaterialSlotInfo& Info() const { return info_; }
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

    void Retranslate()
    {
        chooseButton_->setToolTip(Tr("Выбрать материал для этой части…", "Choose a material for this part…"));
        editButton_->setToolTip(Tr("Открыть материал в редакторе (или двойной щелчок по строке)",
                                   "Open the material in the editor (or double-click the row)"));
        clearButton_->setToolTip(Tr("Сбросить на стандартный материал", "Reset to the default material"));
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
    QToolButton* chooseButton_ = nullptr;
    QToolButton* editButton_ = nullptr;
    QToolButton* clearButton_ = nullptr;
    QPoint pressPos_;
};

// Список слотов материалов модели внутри карточки «Материал».
class MaterialSlotList : public QWidget
{
public:
    // Фильтр по имени показывается, только когда слотов больше этого числа.
    static constexpr int kFilterThreshold = 8;

    std::function<void(int, const QString&)> onAssign; // индекс слота, /Game путь или пусто (стандартный)
    std::function<void(int)> onChoose;
    std::function<void(const QString&)> onEdit;

    explicit MaterialSlotList(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("MaterialSlotList"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 4, 0, 0);
        layout->setSpacing(2);

        header_ = new QLabel(this);
        header_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(header_);

        overrideHint_ = new QLabel(this);
        overrideHint_->setObjectName(QStringLiteral("MaterialSlotOverrideHint"));
        overrideHint_->setWordWrap(true);
        layout->addWidget(overrideHint_);

        filter_ = new QLineEdit(this);
        filter_->setObjectName(QStringLiteral("MaterialSlotFilter"));
        filter_->setClearButtonEnabled(true);
        filter_->setFixedHeight(22);
        QObject::connect(filter_, &QLineEdit::textChanged, this, [this] { ApplyFilter(); });
        layout->addWidget(filter_);

        rows_ = new QWidget(this);
        rowsLayout_ = new QVBoxLayout(rows_);
        rowsLayout_->setContentsMargins(0, 0, 0, 0);
        rowsLayout_->setSpacing(1);
        layout->addWidget(rows_);

        hint_ = new QLabel(this);
        hint_->setObjectName(QStringLiteral("MaterialSlotHint"));
        hint_->setWordWrap(true);
        layout->addWidget(hint_);

        ApplyStyle();
        Retranslate();
        hide();
    }

    void SetSlots(const QVector<MaterialSlotInfo>& items, const QString& contentRoot, bool overridden, bool forceCheck)
    {
        if (forceCheck || contentRoot != contentRoot_) exists_.clear();
        contentRoot_ = contentRoot;
        const bool layoutChanged = items.size() != slots_.size()
            || !std::equal(items.begin(), items.end(), slots_.begin(), [](const auto& a, const auto& b) { return a.index == b.index && a.name == b.name; });
        if (!layoutChanged && !forceCheck && items == slots_ && overridden == overridden_) return;
        if (layoutChanged) Rebuild(items);
        slots_ = items;
        overridden_ = overridden;
        for (int i = 0; i < rowWidgets_.size(); ++i)
            rowWidgets_[i]->SetInfo(items[i], Exists(items[i].path), overridden);
        header_->setText(Tr("Материалы частей модели: %1", "Model part materials: %1").arg(items.size()));
        overrideHint_->setVisible(overridden);
        filter_->setVisible(items.size() > kFilterThreshold);
        setVisible(!items.isEmpty());
    }

    void InvalidateCache() { exists_.clear(); }

    void Retranslate()
    {
        header_->setText(Tr("Материалы частей модели: %1", "Model part materials: %1").arg(slots_.size()));
        overrideHint_->setText(Tr("Общий материал выше перекрывает все слоты. Уберите его, чтобы части модели получили свои материалы.",
                                  "The model-wide material above overrides every slot. Clear it to give each part its own material."));
        filter_->setPlaceholderText(Tr("Фильтр по имени или материалу…", "Filter by name or material…"));
        hint_->setText(Tr("Перетащите .mat из контент-браузера на строку, или строку на другую строку, чтобы скопировать материал.",
                          "Drop a .mat from the Content Browser onto a row, or drag a row onto another to copy its material."));
        for (auto* row : rowWidgets_) row->Retranslate();
        if (!slots_.isEmpty()) SetSlots(slots_, contentRoot_, overridden_, true);
    }

    void ApplyStyle()
    {
        const auto pal = EditorTheme::Instance().Palette();
        setStyleSheet(QStringLiteral(
            "QFrame[materialSlotRow=\"true\"] { background: %1; border: 1px solid transparent; border-radius: 3px; }"
            "QFrame[materialSlotRow=\"true\"]:hover { border-color: %2; }"
            "QFrame[materialSlotRow=\"true\"][dropTarget=\"true\"] { border-color: %3; }"
            "QLabel#MaterialSlotOverrideHint { color: %3; }"
            "QLabel#MaterialSlotHint { color: %4; }")
            .arg(pal.bgDarkest.name(), pal.borderMid.name(), EditorTheme::Instance().AccentColor().name(), pal.textDisabled.name()));
    }

private:
    bool Exists(const QString& path)
    {
        if (path.isEmpty() || contentRoot_.isEmpty()) return true;
        auto found = exists_.find(path);
        if (found == exists_.end()) found = exists_.insert(path, QFileInfo::exists(MaterialSlotDrag::DiskPath(contentRoot_, path)));
        return *found;
    }

    void Rebuild(const QVector<MaterialSlotInfo>& items)
    {
        qDeleteAll(rowWidgets_);
        rowWidgets_.clear();
        for (const auto& slot : items)
        {
            auto* row = new MaterialSlotRow(slot, rows_);
            const int index = slot.index;
            row->contentRoot = [this] { return contentRoot_; };
            row->dropped = [this, index](const QString& path) { if (onAssign) onAssign(index, path); };
            row->clear = [this, index] { if (onAssign) onAssign(index, QString()); };
            row->choose = [this, index] { if (onChoose) onChoose(index); };
            row->edit = [this, row] { if (onEdit) onEdit(row->Info().path); };
            rowsLayout_->addWidget(row);
            rowWidgets_.push_back(row);
        }
        ApplyFilter();
    }

    void ApplyFilter()
    {
        const QString text = filter_->text().trimmed();
        for (auto* row : rowWidgets_)
            row->setVisible(text.isEmpty() || row->SearchText().contains(text, Qt::CaseInsensitive));
    }

    QLabel* header_ = nullptr;
    QLabel* overrideHint_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QWidget* rows_ = nullptr;
    QVBoxLayout* rowsLayout_ = nullptr;
    QLabel* hint_ = nullptr;
    QVector<MaterialSlotRow*> rowWidgets_;
    QVector<MaterialSlotInfo> slots_;
    QHash<QString, bool> exists_;
    QString contentRoot_;
    bool overridden_ = false;
};
