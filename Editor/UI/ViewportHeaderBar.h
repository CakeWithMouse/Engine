#pragma once

#include "EditorLocale.h"
#include "CommandRegistry.h"
#include "EditorSettings.h"
#include "EditorTheme.h"

#include <QAction>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>

class ViewportHeaderBar : public QFrame
{
public:
    explicit ViewportHeaderBar(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("ViewportHeaderBar"));
        setFixedHeight(31);

        layout_ = new QHBoxLayout(this);
        layout_->setContentsMargins(4, 3, 4, 3);
        layout_->setSpacing(2);

        btnTranslate_ = new QPushButton(this);
        btnTranslate_->setObjectName(QStringLiteral("btnGizmoTranslate"));
        btnTranslate_->setIconSize(QSize(16, 16));
        btnTranslate_->setFixedSize(24, 24);
        btnTranslate_->setCheckable(true);
        btnTranslate_->setChecked(true);
        btnTranslate_->setFocusPolicy(Qt::NoFocus);
        btnTranslate_->setFlat(true);
        QObject::connect(btnTranslate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate"));
        });
        layout_->addWidget(btnTranslate_);

        btnRotate_ = new QPushButton(this);
        btnRotate_->setObjectName(QStringLiteral("btnGizmoRotate"));
        btnRotate_->setIconSize(QSize(16, 16));
        btnRotate_->setFixedSize(24, 24);
        btnRotate_->setCheckable(true);
        btnRotate_->setFocusPolicy(Qt::NoFocus);
        btnRotate_->setFlat(true);
        QObject::connect(btnRotate_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate"));
        });
        layout_->addWidget(btnRotate_);

        btnScale_ = new QPushButton(this);
        btnScale_->setObjectName(QStringLiteral("btnGizmoScale"));
        btnScale_->setIconSize(QSize(16, 16));
        btnScale_->setFixedSize(24, 24);
        btnScale_->setCheckable(true);
        btnScale_->setFocusPolicy(Qt::NoFocus);
        btnScale_->setFlat(true);
        QObject::connect(btnScale_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale"));
        });
        layout_->addWidget(btnScale_);

        btnCoordSpace_ = new QPushButton(this);
        btnCoordSpace_->setObjectName(QStringLiteral("btnGizmoCoordSpace"));
        btnCoordSpace_->setIconSize(QSize(16, 16));
        btnCoordSpace_->setFixedSize(24, 24);
        btnCoordSpace_->setFocusPolicy(Qt::NoFocus);
        btnCoordSpace_->setFlat(true);
        QObject::connect(btnCoordSpace_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
        });
        layout_->addWidget(btnCoordSpace_);

        btnSnap_ = new QPushButton(this);
        btnSnap_->setObjectName(QStringLiteral("btnGizmoSnap"));
        btnSnap_->setIconSize(QSize(16, 16));
        btnSnap_->setFixedSize(24, 24);
        btnSnap_->setCheckable(true);
        btnSnap_->setFocusPolicy(Qt::NoFocus);
        btnSnap_->setFlat(true);
        QObject::connect(btnSnap_, &QPushButton::clicked, [] {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleSnap"));
        });
        layout_->addWidget(btnSnap_);

        comboSnapStep_ = new QComboBox(this);
        comboSnapStep_->setObjectName(QStringLiteral("comboGizmoSnapStep"));
        comboSnapStep_->addItems({QStringLiteral("0.1"), QStringLiteral("0.25"), QStringLiteral("0.5"), QStringLiteral("1.0"), QStringLiteral("5.0")});
        comboSnapStep_->setCurrentIndex(2); // 0.5
        comboSnapStep_->setFocusPolicy(Qt::NoFocus);
        comboSnapStep_->setFixedHeight(22);
        comboSnapStep_->setFixedWidth(52);
        layout_->addWidget(comboSnapStep_);

        btnOverflow_ = new QToolButton(this);
        btnOverflow_->setObjectName(QStringLiteral("btnViewportOverflow"));
        btnOverflow_->setIconSize(QSize(16, 16));
        btnOverflow_->setFixedSize(24, 24);
        btnOverflow_->setAutoRaise(true);
        btnOverflow_->setPopupMode(QToolButton::InstantPopup);
        btnOverflow_->setFocusPolicy(Qt::NoFocus);

        overflowMenu_ = new QMenu(btnOverflow_);
        overflowMenu_->setObjectName(QStringLiteral("menuViewportOverflow"));
        btnOverflow_->setMenu(overflowMenu_);
        QObject::connect(overflowMenu_, &QMenu::aboutToShow, [this] {
            BuildOverflowMenu();
        });
        layout_->addWidget(btnOverflow_);
        btnOverflow_->hide();

        layout_->addStretch(1);

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
            "QPushButton, QToolButton {"
            "  background-color: transparent;"
            "  border: 1px solid transparent;"
            "  border-radius: 3px;"
            "  padding: 0px;"
            "  min-width: 24px;"
            "  max-width: 24px;"
            "  min-height: 24px;"
            "  max-height: 24px;"
            "}"
            "QPushButton:hover, QToolButton:hover {"
            "  background-color: %3;"
            "  border-color: %2;"
            "}"
            "QPushButton:checked {"
            "  background-color: %4;"
            "  border: 1px solid %5;"
            "}"
            "QToolButton::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
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
        btnOverflow_->setIcon(EditorTheme::CreateMoreIcon(p.textPrimary));
    }

    void SetGizmoMode(int mode)
    {
        gizmoMode_ = mode;
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
        isSnapActive_ = active;
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
        btnOverflow_->setToolTip(Tr("Дополнительно", "More"));

        BuildOverflowMenu();
    }

    QComboBox* ComboSnapStep() const { return comboSnapStep_; }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QFrame::resizeEvent(event);
        UpdateOverflow(event->size().width());
    }

private:
    void UpdateOverflow(int w)
    {
        // Total needed when all 6 controls are visible: ~180px.
        // Step 1: collapse Snap & SnapStep into overflow menu (needs ~130px).
        // Step 2: collapse CoordSpace into overflow menu (needs ~100px).
        // Step 3: collapse Rotate & Scale into overflow menu (<100px).
        if (w >= 185)
        {
            btnTranslate_->setVisible(true);
            btnRotate_->setVisible(true);
            btnScale_->setVisible(true);
            btnCoordSpace_->setVisible(true);
            btnSnap_->setVisible(true);
            comboSnapStep_->setVisible(true);
            btnOverflow_->setVisible(false);
        }
        else if (w >= 135)
        {
            btnTranslate_->setVisible(true);
            btnRotate_->setVisible(true);
            btnScale_->setVisible(true);
            btnCoordSpace_->setVisible(true);
            btnSnap_->setVisible(false);
            comboSnapStep_->setVisible(false);
            btnOverflow_->setVisible(true);
        }
        else if (w >= 105)
        {
            btnTranslate_->setVisible(true);
            btnRotate_->setVisible(true);
            btnScale_->setVisible(true);
            btnCoordSpace_->setVisible(false);
            btnSnap_->setVisible(false);
            comboSnapStep_->setVisible(false);
            btnOverflow_->setVisible(true);
        }
        else
        {
            btnTranslate_->setVisible(true);
            btnRotate_->setVisible(false);
            btnScale_->setVisible(false);
            btnCoordSpace_->setVisible(false);
            btnSnap_->setVisible(false);
            comboSnapStep_->setVisible(false);
            btnOverflow_->setVisible(true);
        }
    }

    void BuildOverflowMenu()
    {
        if (!overflowMenu_)
            return;

        qDeleteAll(overflowMenu_->children());
        overflowMenu_->clear();
        const auto p = EditorTheme::Instance().Palette();

        if (!btnRotate_->isVisible())
        {
            auto* actRotate = overflowMenu_->addAction(EditorTheme::CreateRotateIcon(p.textPrimary),
                Tr("Вращение (E/R)", "Rotate (E/R)"), [] {
                    CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate"));
                });
            actRotate->setCheckable(true);
            actRotate->setChecked(gizmoMode_ == 1);

            auto* actScale = overflowMenu_->addAction(EditorTheme::CreateScaleIcon(p.textPrimary),
                Tr("Масштаб (R/S)", "Scale (R/S)"), [] {
                    CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale"));
                });
            actScale->setCheckable(true);
            actScale->setChecked(gizmoMode_ == 2);

            overflowMenu_->addSeparator();
        }

        if (!btnCoordSpace_->isVisible())
        {
            auto* actCoord = overflowMenu_->addAction(
                isLocal_ ? EditorTheme::CreateLocalIcon() : EditorTheme::CreateWorldIcon(p.textPrimary),
                isLocal_ ? Tr("Координаты: Local", "Coordinates: Local") : Tr("Координаты: World", "Coordinates: World"),
                [] {
                    CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
                });
            Q_UNUSED(actCoord);
            overflowMenu_->addSeparator();
        }

        if (!btnSnap_->isVisible())
        {
            auto* actSnap = overflowMenu_->addAction(EditorTheme::CreateSnapIcon(p.textPrimary),
                Tr("Привязка к сетке (Shift+S)", "Grid Snapping (Shift+S)"), [] {
                    CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleSnap"));
                });
            actSnap->setCheckable(true);
            actSnap->setChecked(isSnapActive_);

            auto* subMenu = overflowMenu_->addMenu(Tr("Шаг привязки", "Snap Step"));
            const QString currentStep = comboSnapStep_ ? comboSnapStep_->currentText() : QStringLiteral("0.5");
            const QStringList steps = {QStringLiteral("0.1"), QStringLiteral("0.25"), QStringLiteral("0.5"), QStringLiteral("1.0"), QStringLiteral("5.0")};
            for (const auto& step : steps)
            {
                auto* actStep = subMenu->addAction(step, [this, step] {
                    if (comboSnapStep_)
                        comboSnapStep_->setCurrentText(step);
                });
                actStep->setCheckable(true);
                actStep->setChecked(step == currentStep);
            }
        }
    }

private:
    QHBoxLayout* layout_ = nullptr;
    QPushButton* btnTranslate_ = nullptr;
    QPushButton* btnRotate_ = nullptr;
    QPushButton* btnScale_ = nullptr;
    QPushButton* btnCoordSpace_ = nullptr;
    QPushButton* btnSnap_ = nullptr;
    QComboBox* comboSnapStep_ = nullptr;
    QToolButton* btnOverflow_ = nullptr;
    QMenu* overflowMenu_ = nullptr;
    int themeSubId_ = 0;
    bool isLocal_ = false;
    bool isSnapActive_ = false;
    int gizmoMode_ = 0;
};
