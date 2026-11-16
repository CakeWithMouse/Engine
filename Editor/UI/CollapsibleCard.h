#pragma once

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
#include <functional>
#include "EditorTheme.h"
#include "IconProvider.h"

class CollapsibleCard : public QFrame
{
public:
    explicit CollapsibleCard(const QString& title, QWidget* parent = nullptr, bool expanded = true)
        : QFrame(parent), expanded_(expanded), titleText_(title)
    {
        setObjectName(QStringLiteral("CollapsibleCard"));

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        headerBtn_ = new QPushButton(this);
        headerBtn_->setObjectName(QStringLiteral("CardHeaderBtn"));
        headerBtn_->setCursor(Qt::PointingHandCursor);
        headerBtn_->setFixedHeight(22);
        UpdateHeaderText();

        QObject::connect(headerBtn_, &QPushButton::clicked, [this] {
            SetExpanded(!expanded_);
        });
        mainLayout->addWidget(headerBtn_);

        content_ = new QWidget(this);
        content_->setObjectName(QStringLiteral("CardContent"));
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(6, 4, 6, 4);
        contentLayout_->setSpacing(4);
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
    QPushButton* headerBtn_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
    int subId_ = 0;
    std::function<void(bool)> toggledCallback_;
};
