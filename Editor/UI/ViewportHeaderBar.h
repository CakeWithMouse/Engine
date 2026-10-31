#pragma once

#include "EditorLocale.h"
#include "CommandRegistry.h"
#include "EditorSettings.h"
#include "EditorTheme.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>

class ViewportHeaderBar : public QFrame
{
public:
    explicit ViewportHeaderBar(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("ViewportHeaderBar"));
        setFixedHeight(30);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(3);

        btnTranslate_ = new QPushButton(this);
        btnTranslate_->setIconSize(QSize(16, 16));
        btnTranslate_->setCheckable(true);
        btnTranslate_->setChecked(true);
        btnTranslate_->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnTranslate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate"));
        });
        layout->addWidget(btnTranslate_);

        btnRotate_ = new QPushButton(this);
        btnRotate_->setIconSize(QSize(16, 16));
        btnRotate_->setCheckable(true);
        btnRotate_->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnRotate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate"));
        });
        layout->addWidget(btnRotate_);

        btnScale_ = new QPushButton(this);
        btnScale_->setIconSize(QSize(16, 16));
        btnScale_->setCheckable(true);
        btnScale_->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnScale_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale"));
        });
        layout->addWidget(btnScale_);

        layout->addSpacing(4);

        btnCoordSpace_ = new QPushButton(this);
        btnCoordSpace_->setIconSize(QSize(16, 16));
        btnCoordSpace_->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnCoordSpace_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
        });
        layout->addWidget(btnCoordSpace_);

        layout->addSpacing(4);

        btnSnap_ = new QPushButton(this);
        btnSnap_->setIconSize(QSize(16, 16));
        btnSnap_->setCheckable(true);
        btnSnap_->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnSnap_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleSnap"));
        });
        layout->addWidget(btnSnap_);

        comboSnapStep_ = new QComboBox(this);
        comboSnapStep_->addItems({QStringLiteral("0.1"), QStringLiteral("0.25"), QStringLiteral("0.5"), QStringLiteral("1.0"), QStringLiteral("5.0")});
        comboSnapStep_->setCurrentIndex(2); // 0.5
        comboSnapStep_->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(comboSnapStep_);

        layout->addStretch(1);

        ApplyTheme();
        RetranslateUi();

        themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
            ApplyTheme();
        });
    }

    ~ViewportHeaderBar() override
    {
        if (themeSubId_)
        {
            EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
            themeSubId_ = 0;
        }
    }

    void ApplyTheme()
    {
        const auto p = EditorTheme::Instance().Palette();
        setStyleSheet(QStringLiteral(
            "QFrame#ViewportHeaderBar {"
            "  background-color: %1;"
            "  border: none;"
            "  border-bottom: 1px solid %2;"
            "  padding: 0px;"
            "}"
            "QPushButton {"
            "  background-color: transparent;"
            "  border: 1px solid transparent;"
            "  border-radius: 4px;"
            "  padding: 2px;"
            "  min-width: 24px;"
            "  max-width: 24px;"
            "  min-height: 24px;"
            "  max-height: 24px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %3;"
            "  border-color: %2;"
            "}"
            "QPushButton:checked {"
            "  background-color: %4;"
            "  border: 1px solid %5;"
            "}"
            "QComboBox {"
            "  background-color: %6;"
            "  color: %7;"
            "  border: 1px solid %2;"
            "  border-radius: 3px;"
            "  padding: 1px 4px;"
            "  font-size: 11px;"
            "  min-height: 20px;"
            "  max-height: 22px;"
            "}"
        ).arg(
            p.bgPanel.name(),
            p.borderLight.name(),
            p.bgButtonHover.name(),
            p.bgButton.name(),
            p.accent.name(),
            p.bgButton.name(),
            p.textPrimary.name()
        ));

        btnTranslate_->setIcon(EditorTheme::CreateTranslateIcon(p.textPrimary));
        btnRotate_->setIcon(EditorTheme::CreateRotateIcon(p.textPrimary));
        btnScale_->setIcon(EditorTheme::CreateScaleIcon(p.textPrimary));
        btnCoordSpace_->setIcon(isLocal_ ? EditorTheme::CreateLocalIcon() : EditorTheme::CreateWorldIcon(p.textPrimary));
        btnSnap_->setIcon(EditorTheme::CreateSnapIcon(p.textPrimary));
    }

    void SetGizmoMode(int mode)
    {
        btnTranslate_->setChecked(mode == 0);
        btnRotate_->setChecked(mode == 1);
        btnScale_->setChecked(mode == 2);
    }

    void SetCoordinateSpaceLocal(bool local)
    {
        isLocal_ = local;
        const auto p = EditorTheme::Instance().Palette();
        btnCoordSpace_->setIcon(local ? EditorTheme::CreateLocalIcon() : EditorTheme::CreateWorldIcon(p.textPrimary));
        btnCoordSpace_->setToolTip(local
            ? Tr("Пространство координат: Local", "Coordinate Space: Local")
            : Tr("Пространство координат: World", "Coordinate Space: World"));
    }

    void SetSnapActive(bool active)
    {
        btnSnap_->setChecked(active);
    }

    void SetControlScheme(ControlScheme)
    {
    }

    double CurrentSnapStep() const
    {
        return comboSnapStep_->currentText().toDouble();
    }

    void RetranslateUi()
    {
        btnTranslate_->setToolTip(Tr("Перемещение (W/G)", "Translate (W/G)"));
        btnRotate_->setToolTip(Tr("Вращение (E/R)", "Rotate (E/R)"));
        btnScale_->setToolTip(Tr("Масштаб (R/S)", "Scale (R/S)"));
        btnCoordSpace_->setToolTip(isLocal_
            ? Tr("Пространство координат: Local", "Coordinate Space: Local")
            : Tr("Пространство координат: World", "Coordinate Space: World"));
        btnSnap_->setToolTip(Tr("Привязка к сетке (Shift+S)", "Grid Snapping (Shift+S)"));
        comboSnapStep_->setToolTip(Tr("Шаг привязки перемещения", "Move Snap Step"));
    }

    QComboBox* ComboSnapStep() const { return comboSnapStep_; }

private:
    QPushButton* btnTranslate_ = nullptr;
    QPushButton* btnRotate_ = nullptr;
    QPushButton* btnScale_ = nullptr;
    QPushButton* btnCoordSpace_ = nullptr;
    QPushButton* btnSnap_ = nullptr;
    QComboBox* comboSnapStep_ = nullptr;
    int themeSubId_ = 0;
    bool isLocal_ = false;
};
