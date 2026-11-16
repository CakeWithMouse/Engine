#pragma once

#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QFrame>
#include <QLabel>
#include <QPoint>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class InfoButton : public QToolButton
{
public:
    explicit InfoButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setObjectName(QStringLiteral("InfoButton"));
        setFixedSize(18, 18);
        setIconSize(QSize(14, 14));
        setAutoRaise(true);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);

        UpdateIcon();
        QObject::connect(this, &QToolButton::clicked, this, &InfoButton::ShowPopup);

        langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
            RetranslateUi();
        });
        themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
            UpdateIcon();
            UpdatePopupStyle();
        });
    }

    explicit InfoButton(const QString& ruText, const QString& enText, QWidget* parent = nullptr)
        : InfoButton(parent)
    {
        SetInfoText(ruText, enText);
    }

    ~InfoButton() override
    {
        if (langSubId_) EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        if (themeSubId_) EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
        if (popup_) popup_->deleteLater();
    }

    void SetInfoText(const QString& ruText, const QString& enText = QString())
    {
        ruText_ = ruText;
        enText_ = enText.isEmpty() ? ruText : enText;
        RetranslateUi();
    }

    QString CurrentText() const
    {
        return EditorLocale::Instance().Language() == EditorLanguage::Ru ? ruText_ : enText_;
    }

    void ShowPopup()
    {
        if (!popup_)
        {
            popup_ = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
            popup_->setObjectName(QStringLiteral("InfoPopupWindow"));
            auto* layout = new QVBoxLayout(popup_);
            layout->setContentsMargins(10, 8, 10, 8);
            popupLabel_ = new QLabel(popup_);
            popupLabel_->setObjectName(QStringLiteral("InfoPopupText"));
            popupLabel_->setWordWrap(true);
            popupLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(popupLabel_);
        }
        UpdatePopupStyle();
        popupLabel_->setText(CurrentText());
        popup_->adjustSize();

        QPoint pos = mapToGlobal(rect().bottomLeft());
        pos.setY(pos.y() + 2);
        popup_->move(pos);
        popup_->show();
    }

    void RetranslateUi()
    {
        const QString text = CurrentText();
        setToolTip(text);
        if (popupLabel_) popupLabel_->setText(text);
    }

    QFrame* Popup() const { return popup_; }

private:
    void UpdateIcon()
    {
        setIcon(IconProvider::HelpIcon(EditorTheme::Instance().Palette().textSecondary));
    }

    void UpdatePopupStyle()
    {
        if (!popup_) return;
        const auto& pal = EditorTheme::Instance().Palette();
        popup_->setStyleSheet(QStringLiteral(
            "QFrame#InfoPopupWindow {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "}"
            "QLabel#InfoPopupText {"
            "  background: transparent;"
            "  color: %2;"
            "  font-size: 11px;"
            "  line-height: 1.4;"
            "}"
        ).arg(pal.bgPanel.name(), pal.textPrimary.name(), pal.borderMid.name()));
    }

    QString ruText_;
    QString enText_;
    QFrame* popup_{nullptr};
    QLabel* popupLabel_{nullptr};
    int langSubId_{0};
    uint32_t themeSubId_{0};
};
