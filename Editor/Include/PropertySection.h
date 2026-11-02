#pragma once
#include <QWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSettings>
#include "EditorTheme.h"
#include "IconProvider.h"

class PropertySection : public QWidget {
public:
    QWidget* body = nullptr;
    QToolButton* header = nullptr;

    PropertySection(const QString& title, const QString& key, QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("CollapsibleCard"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        header = new QToolButton(this);
        header->setObjectName("Section." + key);
        header->setText(title);
        header->setCheckable(true);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        header->setFixedHeight(22);
        header->setCursor(Qt::PointingHandCursor);

        body = new QWidget(this);
        layout->addWidget(header);
        layout->addWidget(body);

        const auto setting = "Inspector/Sections/" + key;
        const bool open = QSettings(QSettings::defaultFormat(), QSettings::UserScope, "PainEngine", "Editor").value(setting, true).toBool();
        header->setChecked(open);
        body->setVisible(open);

        auto updateIcon = [this](bool isOpen) {
            header->setArrowType(Qt::NoArrow);
            const auto& theme = EditorTheme::Instance();
            header->setIcon(IconProvider::GetIcon(isOpen ? QStringLiteral("chevron-down") : QStringLiteral("chevron-right"), theme.TextPrimary(), 12));
            header->setIconSize(QSize(12, 12));
        };
        updateIcon(open);

        connect(header, &QToolButton::toggled, this, [this, setting, updateIcon](bool value) {
            body->setVisible(value);
            updateIcon(value);
            QSettings(QSettings::defaultFormat(), QSettings::UserScope, "PainEngine", "Editor").setValue(setting, value);
        });

        subId_ = EditorTheme::Instance().SubscribeThemeChanged([this, updateIcon] {
            if (header) updateIcon(header->isChecked());
        });
    }

    ~PropertySection() override {
        if (subId_ > 0) {
            EditorTheme::Instance().UnsubscribeThemeChanged(subId_);
            subId_ = 0;
        }
    }

private:
    int subId_ = 0;
};
