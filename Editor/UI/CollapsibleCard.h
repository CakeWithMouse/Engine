#pragma once

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QString>
#include <functional>
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"
#include "InfoButton.h"
#include "InspectorCardStyle.h"

class CollapsibleCard : public QFrame
{
public:
    explicit CollapsibleCard(const QString& title, QWidget* parent = nullptr, bool expanded = true)
        : QFrame(parent), expanded_(expanded), titleText_(title)
    {
        setObjectName(QStringLiteral("CollapsibleCard"));
        InspectorCardStyle::ApplyCardStyle(this);

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        headerBtn_ = new QPushButton(this);
        headerBtn_->setObjectName(QStringLiteral("CardHeaderBtn"));
        headerBtn_->setCursor(Qt::PointingHandCursor);
        headerBtn_->setFixedHeight(InspectorCardStyle::kHeaderHeight);

        auto* headerLayout = new QHBoxLayout(headerBtn_);
        headerLayout->setContentsMargins(0, 0, 4, 0);
        headerLayout->setSpacing(0);
        headerLayout->addStretch(1);

        infoBtn_ = new InfoButton(headerBtn_);
        infoBtn_->setObjectName(QStringLiteral("CardInfoButton"));
        infoBtn_->hide();
        headerLayout->addWidget(infoBtn_);

        UpdateHeaderText();

        QObject::connect(headerBtn_, &QPushButton::clicked, [this] {
            SetExpanded(!expanded_);
        });
        mainLayout->addWidget(headerBtn_);

        content_ = new QWidget(this);
        content_->setObjectName(QStringLiteral("CardContent"));
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(InspectorCardStyle::CardMargins());
        contentLayout_->setSpacing(InspectorCardStyle::kCardSpacing);
        mainLayout->addWidget(content_);

        content_->setVisible(expanded_);

        subId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
            UpdateHeaderText();
        });
    }

    ~CollapsibleCard() override
    {
        if (subId_ > 0)
        {
            EditorTheme::Instance().UnsubscribeThemeChanged(subId_);
            subId_ = 0;
        }
    }

    void SetTitle(const QString& title)
    {
        titleText_ = title;
        UpdateHeaderText();
    }

    const QString& Title() const { return titleText_; }

    void SetExpanded(bool exp)
    {
        if (expanded_ == exp)
            return;
        expanded_ = exp;
        content_->setVisible(expanded_);
        UpdateHeaderText();
        if (toggledCallback_)
        {
            toggledCallback_(expanded_);
        }
    }

    void SetToggledCallback(std::function<void(bool)> callback)
    {
        toggledCallback_ = std::move(callback);
    }

    bool IsExpanded() const { return expanded_; }

    QWidget* Content() const { return content_; }
    QVBoxLayout* ContentLayout() const { return contentLayout_; }

    void SetInfo(const QString& title, const QString& text)
    {
        infoTitle_ = title;
        infoText_ = text;
        if (!infoBtn_) return;
        if (text.isEmpty() && title.isEmpty())
        {
            infoBtn_->hide();
        }
        else
        {
            infoBtn_->SetInfoText(title, text, text);
            infoBtn_->show();
        }
    }

    void SetInfo(const QString& text)
    {
        SetInfo(titleText_, text);
    }

    void SetInfo(const QString& ruTitle, const QString& enTitle, const QString& ruText, const QString& enText)
    {
        infoTitle_ = EditorLocale::Instance().Language() == EditorLanguage::Ru ? ruTitle : enTitle;
        infoText_ = EditorLocale::Instance().Language() == EditorLanguage::Ru ? ruText : enText;
        if (!infoBtn_) return;
        if (ruText.isEmpty() && enText.isEmpty())
        {
            infoBtn_->hide();
        }
        else
        {
            infoBtn_->SetInfoText(infoTitle_, ruText, enText);
            infoBtn_->show();
        }
    }

    InfoButton* InfoBtn() const { return infoBtn_; }

private:
    void UpdateHeaderText()
    {
        const auto& theme = EditorTheme::Instance();
        const QColor iconColor = theme.TextPrimary();
        headerBtn_->setIcon(IconProvider::GetIcon(expanded_ ? QStringLiteral("chevron-down") : QStringLiteral("chevron-right"), iconColor, 12));
        headerBtn_->setIconSize(QSize(12, 12));
        headerBtn_->setText(titleText_);
    }

    bool expanded_ = true;
    QString titleText_;
    QString infoTitle_;
    QString infoText_;
    QPushButton* headerBtn_ = nullptr;
    InfoButton* infoBtn_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
    int subId_ = 0;
    std::function<void(bool)> toggledCallback_;
};
