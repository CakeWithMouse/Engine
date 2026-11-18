#pragma once

#include "AssetExtensions.h"
#include "AssetPathDisplay.h"
#include "AssetDrag.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"
#include "InspectorCardStyle.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <functional>

class AssetSlotWidget : public QWidget
{
public:
    explicit AssetSlotWidget(EditorCore::AssetKind kind, QWidget* parent = nullptr)
        : QWidget(parent), kind_(kind)
    {
        setAcceptDrops(true);
        auto* l = new QHBoxLayout(this);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(2);

        display_ = new QLineEdit(this);
        display_->setReadOnly(true);
        display_->setFixedHeight(InspectorCardStyle::kFieldHeight);
        display_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        l->addWidget(display_, 1);

        RetranslateUi();
        langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
            RetranslateUi();
        });

        setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(this, &QWidget::customContextMenuRequested, [this](const QPoint& pt) {
            QMenu m(this);
            auto* copyAct = m.addAction(Tr("Копировать путь", "Copy Path"), [this] {
                if (!rawPath_.isEmpty())
                    QGuiApplication::clipboard()->setText(rawPath_);
            });
            copyAct->setEnabled(!rawPath_.isEmpty());
            auto* clrAct = m.addAction(Tr("Очистить слот", "Clear Slot"), [this] {
                if (onClearRequested) onClearRequested();
            });
            clrAct->setEnabled(!rawPath_.isEmpty());
            m.exec(mapToGlobal(pt));
        });
    }

    ~AssetSlotWidget() override
    {
        if (langSubId_)
            EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
    }

    void RetranslateUi()
    {
    }

    void SetContentRoot(const QString& root) { contentRoot_ = root; }

    void SetValue(const QString& rawPath, const QString& formatted)
    {
        rawPath_ = rawPath;
        display_->setText(formatted.isEmpty() ? rawPath : formatted);
        display_->setToolTip(rawPath);
    }

    void SetPlaceholderText(const QString& text) { display_->setPlaceholderText(text); }
    void SetReadOnly(bool ro) { display_->setReadOnly(ro); }
    void SetSlotEnabled(bool en) { display_->setEnabled(en); }
    void SetErrorStyle(bool error, const QString& tip = QString())
    {
        if (error)
        {
            display_->setStyleSheet(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
            display_->setToolTip(tip);
        }
        else
        {
            display_->setStyleSheet(QString());
            display_->setToolTip(rawPath_);
        }
    }

    QLineEdit* LineEdit() const { return display_; }
    QToolButton* ClearButton() const { return nullptr; }
    QString RawPath() const { return rawPath_; }

    std::function<void(const QString& path)> onAssetDropped;
    std::function<void()> onClearRequested;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (!isEnabled()) return;
        const auto payload = EditorCore::AssetDrag::FromMime(*event->mimeData(), contentRoot_);
        if (payload.kind == kind_ || (kind_ == EditorCore::AssetKind::Unknown && !payload.virtualPaths.isEmpty()))
        {
            event->acceptProposedAction();
            setProperty("dragHover", true);
            display_->setStyleSheet(QStringLiteral("border: 2px solid %1;").arg(EditorTheme::Instance().AccentColor().name()));
        }
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        Q_UNUSED(event);
        setProperty("dragHover", false);
        display_->setStyleSheet(QString());
    }

    void dropEvent(QDropEvent* event) override
    {
        setProperty("dragHover", false);
        display_->setStyleSheet(QString());
        const auto payload = EditorCore::AssetDrag::FromMime(*event->mimeData(), contentRoot_);
        if (!payload.virtualPaths.isEmpty())
        {
            event->acceptProposedAction();
            if (onAssetDropped)
                onAssetDropped(payload.virtualPaths.front());
        }
    }

private:
    EditorCore::AssetKind kind_;
    QString contentRoot_;
    QString rawPath_;
    QLineEdit* display_ = nullptr;
    int langSubId_ = 0;
};
