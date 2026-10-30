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
        setStyleSheet(QStringLiteral(
            "QFrame#CollapsibleCard {"
            "  background-color: #171A21;"
            "  border: 1px solid #282D37;"
            "  border-radius: 6px;"
            "  margin-bottom: 6px;"
            "}"
        ));

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        headerBtn_ = new QPushButton(this);
        headerBtn_->setObjectName(QStringLiteral("CardHeaderBtn"));
        headerBtn_->setCursor(Qt::PointingHandCursor);
        headerBtn_->setStyleSheet(QStringLiteral(
            "QPushButton#CardHeaderBtn {"
            "  background-color: #1E222B;"
            "  color: #E2E8F0;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "  text-align: left;"
            "  padding: 6px 10px;"
            "  border: none;"
            "  border-top-left-radius: 5px;"
            "  border-top-right-radius: 5px;"
            "}"
            "QPushButton#CardHeaderBtn:hover {"
            "  background-color: #262C38;"
            "  color: #FFFFFF;"
            "}"
        ));
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
