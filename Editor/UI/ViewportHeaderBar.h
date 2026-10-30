#pragma once

#include "EditorLocale.h"
#include "CommandRegistry.h"
#include "EditorSettings.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class ViewportHeaderBar : public QFrame
{
public:
    explicit ViewportHeaderBar(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("ViewportHeaderBar"));
        setStyleSheet(QStringLiteral(
            "QFrame#ViewportHeaderBar {"
            "  background-color: rgba(23, 26, 33, 210);"
            "  border: 1px solid #2D333F;"
            "  border-radius: 6px;"
            "  padding: 2px;"
            "}"
            "QPushButton {"
            "  background-color: #1E222B;"
            "  color: #CBD5E1;"
            "  border: 1px solid #333842;"
            "  border-radius: 4px;"
            "  padding: 3px 8px;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "  background-color: #2D333F;"
            "  color: #FFFFFF;"
            "}"
            "QPushButton:checked {"
            "  background-color: #DC2626;"
            "  color: #FFFFFF;"
            "  border: 1px solid #EF4444;"
            "}"
            "QComboBox {"
            "  background-color: #1E222B;"
            "  color: #CBD5E1;"
            "  border: 1px solid #333842;"
            "  border-radius: 4px;"
            "  padding: 2px 6px;"
            "  font-size: 11px;"
            "}"
        ));

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(4);

        btnTranslate_ = new QPushButton(Tr("Перемещение (W/G)", "Translate (W/G)"), this);
        btnTranslate_->setCheckable(true);
        btnTranslate_->setChecked(true);
        QObject::connect(btnTranslate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate"));
        });
        layout->addWidget(btnTranslate_);

        btnRotate_ = new QPushButton(Tr("Вращение (E/R)", "Rotate (E/R)"), this);
        btnRotate_->setCheckable(true);
        QObject::connect(btnRotate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate"));
        });
        layout->addWidget(btnRotate_);

        btnScale_ = new QPushButton(Tr("Масштаб (R/S)", "Scale (R/S)"), this);
        btnScale_->setCheckable(true);
        QObject::connect(btnScale_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale"));
        });
        layout->addWidget(btnScale_);

        layout->addSpacing(8);

        btnCoordSpace_ = new QPushButton(QStringLiteral("World"), this);
        btnCoordSpace_->setToolTip(Tr("Пространство координат (World / Local)", "Coordinate Space (World / Local)"));
        QObject::connect(btnCoordSpace_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
        });
        layout->addWidget(btnCoordSpace_);

        layout->addSpacing(8);

        btnSnap_ = new QPushButton(Tr("Снап", "Snap"), this);
        btnSnap_->setCheckable(true);
        btnSnap_->setToolTip(Tr("Привязка к сетке (Shift+S)", "Grid/Angle Snapping (Shift+S)"));
        QObject::connect(btnSnap_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleSnap"));
        });
        layout->addWidget(btnSnap_);

        comboSnapStep_ = new QComboBox(this);
        comboSnapStep_->addItems({QStringLiteral("0.1"), QStringLiteral("0.25"), QStringLiteral("0.5"), QStringLiteral("1.0"), QStringLiteral("5.0")});
        comboSnapStep_->setCurrentIndex(2); // 0.5
        layout->addWidget(comboSnapStep_);

        layout->addStretch(1);

        btnScheme_ = new QPushButton(QStringLiteral("Scheme: Unreal"), this);
        btnScheme_->setToolTip(Tr("Схема управления (Unreal / Blender)", "Control Scheme (Unreal / Blender)"));
        QObject::connect(btnScheme_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Editor.ToggleControlScheme"));
        });
        layout->addWidget(btnScheme_);
    }

    void SetGizmoMode(int mode)
    {
        btnTranslate_->setChecked(mode == 0); // Translate
        btnRotate_->setChecked(mode == 1);    // Rotate
        btnScale_->setChecked(mode == 2);     // Scale
    }

    void SetCoordinateSpaceLocal(bool local)
    {
        btnCoordSpace_->setText(local ? QStringLiteral("Local") : QStringLiteral("World"));
        btnCoordSpace_->setStyleSheet(local
            ? QStringLiteral("background-color: #2563EB; color: white; border: 1px solid #3B82F6;")
            : QString());
    }

    void SetSnapActive(bool active)
    {
        btnSnap_->setChecked(active);
    }

    void SetControlScheme(ControlScheme scheme)
    {
        btnScheme_->setText(scheme == ControlScheme::Blender ? QStringLiteral("Scheme: Blender") : QStringLiteral("Scheme: Unreal"));
    }

    double CurrentSnapStep() const
    {
        return comboSnapStep_->currentText().toDouble();
    }

    void RetranslateUi()
    {
        btnTranslate_->setText(Tr("Перемещение (W/G)", "Translate (W/G)"));
        btnRotate_->setText(Tr("Вращение (E/R)", "Rotate (E/R)"));
        btnScale_->setText(Tr("Масштаб (R/S)", "Scale (R/S)"));
        btnSnap_->setText(Tr("Снап", "Snap"));
    }

    QComboBox* ComboSnapStep() const { return comboSnapStep_; }

private:
    QPushButton* btnTranslate_ = nullptr;
    QPushButton* btnRotate_ = nullptr;
    QPushButton* btnScale_ = nullptr;
    QPushButton* btnCoordSpace_ = nullptr;
    QPushButton* btnSnap_ = nullptr;
    QComboBox* comboSnapStep_ = nullptr;
    QPushButton* btnScheme_ = nullptr;
};

