#pragma once

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>

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
        UpdateHeaderText();

        QObject::connect(headerBtn_, &QPushButton::clicked, [this] {
            SetExpanded(!expanded_);
        });
        mainLayout->addWidget(headerBtn_);

        content_ = new QWidget(this);
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(8, 8, 8, 8);
        contentLayout_->setSpacing(6);
        mainLayout->addWidget(content_);

        content_->setVisible(expanded_);
    }

    void SetTitle(const QString& title)
    {
        titleText_ = title;
        UpdateHeaderText();
    }

    void SetExpanded(bool exp)
    {
        expanded_ = exp;
        content_->setVisible(expanded_);
        UpdateHeaderText();
    }

    bool IsExpanded() const { return expanded_; }

    QWidget* Content() const { return content_; }
    QVBoxLayout* ContentLayout() const { return contentLayout_; }

private:
    void UpdateHeaderText()
    {
        const QString arrow = expanded_ ? QString::fromUtf8("\xE2\x96\xBC ") : QString::fromUtf8("\xE2\x96\xB6 ");
        headerBtn_->setText(arrow + titleText_);
    }

    bool expanded_ = true;
    QString titleText_;
    QPushButton* headerBtn_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
};
