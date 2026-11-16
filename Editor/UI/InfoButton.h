#pragma once

#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QFrame>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace InfoButtonGeometry
{
    constexpr int kMaxPopupWidth = 320;

    inline QRect CalculatePopupGeometry(const QRect& buttonGlobalRect, const QSize& popupSize, const QRect& screenAvailableGeom)
    {
        const int w = std::min(std::min(popupSize.width(), kMaxPopupWidth), screenAvailableGeom.width());
        const int h = std::min(popupSize.height(), screenAvailableGeom.height());

        // Horizontal: default align right edge of popup with right edge of button (expanding left)
        int x = buttonGlobalRect.right() - w + 1;
        if (x < screenAvailableGeom.left())
        {
            // If it doesn't fit on the left, align left edge of popup with left edge of button (expand right)
            x = buttonGlobalRect.left();
        }
        if (x + w > screenAvailableGeom.right() + 1)
        {
            x = screenAvailableGeom.right() + 1 - w;
        }
        if (x < screenAvailableGeom.left())
        {
            x = screenAvailableGeom.left();
        }

        // Vertical: default open below button
        int y = buttonGlobalRect.bottom() + 2;
        if (y + h > screenAvailableGeom.bottom() + 1)
        {
            // If it doesn't fit below, open above button
            y = buttonGlobalRect.top() - h - 2;
        }
        if (y + h > screenAvailableGeom.bottom() + 1)
        {
            y = screenAvailableGeom.bottom() + 1 - h;
        }
        if (y < screenAvailableGeom.top())
        {
            y = screenAvailableGeom.top();
        }

        return QRect(x, y, w, h);
    }
}

class InfoPopupWindow : public QFrame
{
public:
    explicit InfoPopupWindow(QWidget* parent = nullptr)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    {
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            close();
            event->accept();
            return;
        }
        QFrame::keyPressEvent(event);
    }
};

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
        RetranslateUi();
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

    explicit InfoButton(const QString& title, const QString& ruText, const QString& enText, QWidget* parent = nullptr)
        : InfoButton(parent)
    {
        SetInfoText(title, ruText, enText);
    }

    ~InfoButton() override
    {
        if (langSubId_) EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        if (themeSubId_) EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
        if (popup_) popup_->deleteLater();
    }

    void SetInfoText(const QString& ruText, const QString& enText = QString())
    {
        title_.clear();
        ruText_ = ruText;
        enText_ = enText.isEmpty() ? ruText : enText;
        RetranslateUi();
    }

    void SetInfoText(const QString& title, const QString& ruText, const QString& enText)
    {
        title_ = title;
        ruText_ = ruText;
        enText_ = enText.isEmpty() ? ruText : enText;
        RetranslateUi();
    }

    QString CurrentText() const
    {
        return EditorLocale::Instance().Language() == EditorLanguage::Ru ? ruText_ : enText_;
    }

    const QString& Title() const { return title_; }

    void ShowPopup()
    {
        if (!popup_)
        {
            popup_ = new InfoPopupWindow(nullptr);
            popup_->setObjectName(QStringLiteral("InfoPopupWindow"));
            popup_->setMaximumWidth(InfoButtonGeometry::kMaxPopupWidth);
            auto* layout = new QVBoxLayout(popup_);
            layout->setContentsMargins(10, 8, 10, 8);
            layout->setSpacing(4);

            titleLabel_ = new QLabel(popup_);
            titleLabel_->setObjectName(QStringLiteral("InfoPopupTitle"));
            titleLabel_->setWordWrap(true);
            layout->addWidget(titleLabel_);

            popupLabel_ = new QLabel(popup_);
            popupLabel_->setObjectName(QStringLiteral("InfoPopupText"));
            popupLabel_->setWordWrap(true);
            popupLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(popupLabel_);
        }
        UpdatePopupStyle();
        if (!title_.isEmpty())
        {
            titleLabel_->setText(title_);
            titleLabel_->show();
        }
        else
        {
            titleLabel_->hide();
        }
        popupLabel_->setText(CurrentText());
        popup_->adjustSize();

        QRect btnRect(mapToGlobal(QPoint(0, 0)), size());
        QScreen* screen = this->screen();
        if (!screen)
        {
            screen = QGuiApplication::screenAt(btnRect.center());
        }
        if (!screen)
        {
            screen = QGuiApplication::primaryScreen();
        }
        QRect screenGeom = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        QRect targetRect = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popup_->sizeHint(), screenGeom);

        popup_->setGeometry(targetRect);
        popup_->show();
    }

    void RetranslateUi()
    {
        setToolTip(EditorLocale::Instance().Language() == EditorLanguage::Ru
            ? QStringLiteral("Справка")
            : QStringLiteral("Help"));
        if (popupLabel_) popupLabel_->setText(CurrentText());
        if (titleLabel_ && !title_.isEmpty()) titleLabel_->setText(title_);
    }

    QFrame* Popup() const { return popup_; }

    void paintEvent(QPaintEvent* event) override
    {
        if (icon().isNull())
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, true);
            if (isDown())
            {
                p.fillRect(rect(), EditorTheme::Instance().Palette().bgButtonPressed);
            }
            else if (underMouse())
            {
                p.fillRect(rect(), EditorTheme::Instance().Palette().bgButtonHover);
            }
            p.setPen(EditorTheme::Instance().Palette().textSecondary);
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("?"));
            return;
        }
        QToolButton::paintEvent(event);
    }

private:
    void UpdateIcon()
    {
        QIcon ic = IconProvider::HelpIcon(EditorTheme::Instance().Palette().textSecondary);
        if (ic.isNull())
        {
            qWarning("[Warning] InfoButton: help icon not found, using '?' text fallback");
        }
        setIcon(ic);
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
            "QLabel#InfoPopupTitle {"
            "  background: transparent;"
            "  color: %2;"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "}"
            "QLabel#InfoPopupText {"
            "  background: transparent;"
            "  color: %2;"
            "  font-size: 11px;"
            "  line-height: 1.4;"
            "}"
        ).arg(pal.bgPanel.name(), pal.textPrimary.name(), pal.borderMid.name()));
    }

    QString title_;
    QString ruText_;
    QString enText_;
    QFrame* popup_{nullptr};
    QLabel* titleLabel_{nullptr};
    QLabel* popupLabel_{nullptr};
    int langSubId_{0};
    uint32_t themeSubId_{0};
};
