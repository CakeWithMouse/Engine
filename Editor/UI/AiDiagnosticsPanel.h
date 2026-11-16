#pragma once

#include "CollapsibleCard.h"
#include "ColorSwatch.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "GizmoGeometry.h"
#include "InfoButton.h"
#include "InspectorCardStyle.h"
#include "NavOverlayColors.h"
#include "NoWheelDoubleSpinBox.h"
#include "QtHost.h"
#include "SettingsService.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

// UI owns configuration only. All scene access runs through RunTask on the engine thread.
class AiDiagnosticsPanel : public QWidget
{
public:
    static constexpr int kMinDetailsCardHeight = 120;

    AiDiagnosticsPanel(EditorQtHost* host, QWidget* viewport, QWidget* parent = nullptr)
        : QWidget(parent), host_(host), viewport_(viewport)
    {
        setObjectName(QStringLiteral("AiDiagnosticsPanel"));
        setMinimumSize(40, 30);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("AiDiagnosticsScrollArea"));
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setFrameShape(QFrame::NoFrame);
        outer->addWidget(scroll);

        auto* body = new QWidget(scroll);
        body->setObjectName(QStringLiteral("AiDiagnosticsScrollContent"));
        body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        scroll->setWidget(body);

        auto* layout = new QVBoxLayout(body);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        enabled_ = new QCheckBox(Tr("Показывать диагностику AI", "Show AI diagnostics"), body);
        enabled_->setObjectName(QStringLiteral("AiEnabled"));
        layout->addWidget(enabled_);

        source_ = new QLabel(Tr("Слои выключены", "Layers off"), body);
        source_->setObjectName(QStringLiteral("AiSourceStatus"));
        source_->setWordWrap(true);
        source_->setStyleSheet(QStringLiteral("font-size: 11px; padding: 6px 10px; border-radius: 4px; background: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1);"));
        source_->setVisible(false);
        layout->addWidget(source_);

        // Card 1: Layers
        auto* cardLayers = new CollapsibleCard(Tr("Слои во вьюпорте", "Viewport layers"), body, true);
        cardLayers->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        cardLayers->SetInfo(
            Tr("Слои во вьюпорте", "Viewport layers"),
            Tr("Легенда слоев:\n• Зеленый — проход\n• Красный — препятствие\n• Желтый — стоимость / тесно\n• A / B — старт и цель\n• Голубой — направление",
               "Layer legend:\n• Green — walkable\n• Red — obstacle\n• Yellow — cost / clearance\n• A / B — start & goal\n• Cyan — direction")
        );
        if (auto* btn = cardLayers->InfoBtn())
            btn->setObjectName(QStringLiteral("AiLegendInfoButton"));

        auto* layerLayout = new QGridLayout;
        layerLayout->setSpacing(InspectorCardStyle::kRowSpacing);
        layerLayout->setContentsMargins(0, 0, 0, 0);

        layers_[0] = new QCheckBox(Tr("Навигационная сетка", "Navigation grid"), cardLayers->Content());
        layers_[1] = new QCheckBox(Tr("Стоимость прохода", "Traversal cost"), cardLayers->Content());
        layers_[2] = new QCheckBox(Tr("Проходимость по радиусу", "Agent clearance"), cardLayers->Content());
        layers_[3] = new QCheckBox(Tr("Пути и цели агентов", "Agent paths and goals"), cardLayers->Content());
        layers_[4] = new QCheckBox(Tr("Поле направлений", "Direction field"), cardLayers->Content());
        layers_[5] = new QCheckBox(Tr("Восприятие и цели", "Perception and targets"), cardLayers->Content());
        layers_[6] = new QCheckBox(Tr("Строй отряда", "Squad formation"), cardLayers->Content());
        layers_[7] = new QCheckBox(Tr("Пробный маршрут A → B", "Test route A → B"), cardLayers->Content());

        for (int i = 0; i < 8; ++i)
        {
            layers_[i]->setObjectName(QStringLiteral("AiLayer%1").arg(i));
            layerLayout->addWidget(layers_[i], i / 2, i % 2);
            connect(layers_[i], &QCheckBox::toggled, this, [this] { SaveSettings(); Poll(); });
        }
        cardLayers->ContentLayout()->addLayout(layerLayout);

        auto* legendGrid = new QGridLayout;
        legendGrid->setObjectName(QStringLiteral("AiLegendGrid"));
        legendGrid->setContentsMargins(0, 4, 0, 4);
        legendGrid->setHorizontalSpacing(6);
        legendGrid->setVerticalSpacing(3);

        struct LegendRowInfo {
            QColor color;
            QString name;
            QString desc;
        };
        const std::array<LegendRowInfo, 5> legendRows = {{
            { NavOverlayColors::Walkable(), Tr("Проход", "Walkable"), Tr("агент может пройти", "agent can pass") },
            { NavOverlayColors::Obstacle(), Tr("Препятствие", "Obstacle"), Tr("проход закрыт", "path blocked") },
            { NavOverlayColors::Cost(), Tr("Стоимость", "Cost"), Tr("чем теплее цвет, тем дороже путь", "warmer color means higher cost") },
            { NavOverlayColors::RouteAB(), Tr("A/B", "A/B"), Tr("старт и цель пробного маршрута", "start and goal of test route") },
            { NavOverlayColors::Direction(), Tr("Направление", "Direction"), Tr("куда ведёт путь", "where path leads") }
        }};

        for (int i = 0; i < 5; ++i)
        {
            auto* swatch = new ColorSwatch(legendRows[i].color, cardLayers->Content());
            swatch->setObjectName(QStringLiteral("AiSwatch%1").arg(i));
            legendGrid->addWidget(swatch, i, 0, Qt::AlignCenter);

            auto* lblName = new QLabel(legendRows[i].name, cardLayers->Content());
            lblName->setObjectName(QStringLiteral("AiLegendName%1").arg(i));
            lblName->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold;"));
            legendGrid->addWidget(lblName, i, 1, Qt::AlignLeft | Qt::AlignVCenter);

            auto* lblDesc = new QLabel(legendRows[i].desc, cardLayers->Content());
            lblDesc->setObjectName(QStringLiteral("AiLegendDesc%1").arg(i));
            lblDesc->setStyleSheet(QStringLiteral("font-size: 11px; color: rgba(255, 255, 255, 0.7);"));
            lblDesc->setWordWrap(true);
            legendGrid->addWidget(lblDesc, i, 2, Qt::AlignLeft | Qt::AlignVCenter);

            if (i == 0)
            {
                legend_ = lblName;
                legend_->setObjectName(QStringLiteral("AiLegend"));
            }
        }
        cardLayers->ContentLayout()->addLayout(legendGrid);
        layout->addWidget(cardLayers);

        // Card 2: Subject
        auto* cardSubject = new CollapsibleCard(Tr("Объект диагностики", "Diagnostic subject"), body, true);
        cardSubject->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto* af = new QFormLayout;
        InspectorCardStyle::ApplyRowLayout(af);

        units_ = new QComboBox(cardSubject->Content());
        units_->setObjectName(QStringLiteral("AiSubject"));
        units_->setFixedHeight(InspectorCardStyle::kFieldHeight);
        units_->addItem(Tr("Выделенный объект сцены", "Scene selection"), 0);
        af->addRow(units_);

        selected_ = new QCheckBox(Tr("Только выбранный агент / отряд", "Selected agent / squad only"), cardSubject->Content());
        selected_->setObjectName(QStringLiteral("AiSelectedOnly"));
        af->addRow(selected_);

        agentRadius_ = new QCheckBox(Tr("Брать радиус выбранного агента", "Use selected agent radius"), cardSubject->Content());
        agentRadius_->setObjectName(QStringLiteral("AiAgentRadius"));
        af->addRow(agentRadius_);

        radius_ = Spin("AiRadius", 0.5, 0.01, 100.0, cardSubject->Content());
        auto* lblRadius = new QLabel(Tr("Радиус, м", "Radius, m"), cardSubject->Content());
        InspectorCardStyle::ApplyLabelStyle(lblRadius);
        af->addRow(lblRadius, radius_);
        cardSubject->ContentLayout()->addLayout(af);
        layout->addWidget(cardSubject);

        // Card 3: Probe
        auto* cardProbe = new CollapsibleCard(Tr("Проба навигации · XZ", "Navigation probe · XZ"), body, false);
        cardProbe->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto* pf = new QFormLayout;
        InspectorCardStyle::ApplyRowLayout(pf);

        fromX_ = Spin("AiFromX", -10.0, -100000.0, 100000.0, cardProbe->Content());
        fromZ_ = Spin("AiFromZ", 0.0, -100000.0, 100000.0, cardProbe->Content());
        toX_ = Spin("AiToX", 10.0, -100000.0, 100000.0, cardProbe->Content());
        toZ_ = Spin("AiToZ", 0.0, -100000.0, 100000.0, cardProbe->Content());

        auto* lblFrom = new QLabel(Tr("A · X / Z", "A · X / Z"), cardProbe->Content()); InspectorCardStyle::ApplyLabelStyle(lblFrom);
        pf->addRow(lblFrom, Pair(fromX_, fromZ_));
        auto* lblTo = new QLabel(Tr("B · X / Z", "B · X / Z"), cardProbe->Content()); InspectorCardStyle::ApplyLabelStyle(lblTo);
        pf->addRow(lblTo, Pair(toX_, toZ_));

        auto* picks = new QWidget(cardProbe->Content());
        auto* pl = new QHBoxLayout(picks);
        InspectorCardStyle::ApplyRowLayout(pl);
        for (int i = 1; i <= 2; ++i)
        {
            auto* b = new QPushButton(i == 1 ? Tr("Указать A", "Pick A") : Tr("Указать B", "Pick B"), picks);
            b->setObjectName(i == 1 ? QStringLiteral("AiPickA") : QStringLiteral("AiPickB"));
            b->setFixedHeight(InspectorCardStyle::kButtonHeight);
            pl->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, i] {
                pick_ = i;
                source_->setText(Tr("Щёлкните по плоскости во вьюпорте. Esc — отмена.", "Click the viewport plane. Esc cancels."));
            });
        }
        pf->addRow(picks);

        probeStatus_ = new QLabel(cardProbe->Content());
        probeStatus_->setObjectName(QStringLiteral("AiProbeStatus"));
        probeStatus_->setWordWrap(true);
        pf->addRow(probeStatus_);
        cardProbe->ContentLayout()->addLayout(pf);
        layout->addWidget(cardProbe);

        // Card 4: Display
        auto* cardDisplay = new CollapsibleCard(Tr("Отображение", "Display"), body, false);
        cardDisplay->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto* vf = new QFormLayout;
        InspectorCardStyle::ApplyRowLayout(vf);

        height_ = Spin("AiPlaneHeight", 0.15, -1000.0, 1000.0, cardDisplay->Content());
        auto* lblHeight = new QLabel(Tr("Высота плоскости, м", "Plane height, m"), cardDisplay->Content()); InspectorCardStyle::ApplyLabelStyle(lblHeight);
        vf->addRow(lblHeight, height_);

        extent_ = Spin("AiExtent", 60.0, 5.0, 500.0, cardDisplay->Content());
        auto* lblExtent = new QLabel(Tr("Область у камеры, м", "Camera area, m"), cardDisplay->Content()); InspectorCardStyle::ApplyLabelStyle(lblExtent);
        vf->addRow(lblExtent, extent_);

        xray_ = new QCheckBox(Tr("Поверх геометрии", "Show through geometry"), cardDisplay->Content());
        xray_->setObjectName(QStringLiteral("AiXray"));
        vf->addRow(xray_);
        cardDisplay->ContentLayout()->addLayout(vf);
        layout->addWidget(cardDisplay);

        // Card 5: Details
        auto* cardDetails = new CollapsibleCard(Tr("Состояние · задачи · память · история", "State · Tasks · Memory · History"), body, true);
        cardDetails->setObjectName(QStringLiteral("AiDetailsCard"));
        cardDetails->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        cardDetails->setMinimumHeight(kMinDetailsCardHeight);
        cardDetails->SetToggledCallback([cardDetails](bool exp) {
            cardDetails->setMinimumHeight(exp ? kMinDetailsCardHeight : 0);
            cardDetails->updateGeometry();
        });

        cell_ = new QLabel(Tr("Клетка: наведите курсор во вьюпорте", "Cell: hover viewport"), cardDetails->Content());
        cell_->setObjectName(QStringLiteral("AiCellInfo"));
        cell_->setWordWrap(true);
        cell_->setStyleSheet(QStringLiteral("font-size: 11px;"));
        cardDetails->ContentLayout()->addWidget(cell_);

        cellDetails_ = new QWidget(cardDetails->Content());
        cellDetails_->setObjectName(QStringLiteral("AiCellDetails"));
        auto* cdl = new QVBoxLayout(cellDetails_);
        cdl->setContentsMargins(0, 0, 0, 0);
        cellDetailsLabel_ = new QLabel(cellDetails_);
        cellDetailsLabel_->setObjectName(QStringLiteral("AiCellDetailsLabel"));
        cellDetailsLabel_->setWordWrap(true);
        cellDetailsLabel_->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold;"));
        cdl->addWidget(cellDetailsLabel_);
        cellDetails_->setVisible(false);
        cardDetails->ContentLayout()->addWidget(cellDetails_);

        details_ = new QPlainTextEdit(cardDetails->Content());
        details_->setObjectName(QStringLiteral("AiDetails"));
        details_->setReadOnly(true);
        details_->setMinimumHeight(150);
        details_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        details_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        details_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        details_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        cardDetails->ContentLayout()->addWidget(details_);
        layout->addWidget(cardDetails);
        UpdateDetailsGeometry();

        layout->addStretch();

        LoadSettings();

        connect(enabled_, &QCheckBox::toggled, this, [this](bool checked) {
            CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleAiDiagnostics"), {{QStringLiteral("enabled"), checked}});
            SaveSettings();
            Poll();
        });
        connect(&CommandRegistry::Instance(), &CommandRegistry::CommandExecuted, this, [this](const QString& id, bool success) {
            if (success && id == QStringLiteral("View.ToggleAiDiagnostics"))
            {
                const bool val = SettingsService::Instance().GetValue(QStringLiteral("ai/enabled"), false).toBool();
                if (enabled_->isChecked() != val)
                {
                    QSignalBlocker block(enabled_);
                    enabled_->setChecked(val);
                    Poll();
                }
            }
        });
        for (auto* cb : {selected_, agentRadius_, xray_})
            connect(cb, &QCheckBox::toggled, this, [this] { SaveSettings(); Poll(); });
        for (auto* sp : {radius_, height_, extent_})
            connect(sp, &QDoubleSpinBox::editingFinished, this, [this] { SaveSettings(); Poll(); });
        for (auto* sp : {fromX_, fromZ_, toX_, toZ_})
            connect(sp, &QDoubleSpinBox::editingFinished, this, [this] { Poll(); });

        if (viewport_)
        {
            viewport_->installEventFilter(this);
            viewport_->setMouseTracking(true);
        }
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, [this] { Poll(); });
        timer_->start(180);
    }

    QSize sizeHint() const override { return QSize(320, 600); }
    QSize minimumSizeHint() const override { return QSize(40, 30); }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        UpdateDetailsGeometry();
    }

    void UpdateDetailsGeometry()
    {
        if (!details_ || !details_->document()) return;
        const int viewportW = details_->viewport()->width();
        details_->document()->setTextWidth(viewportW > 0 ? viewportW : 280);
        const int docH = static_cast<int>(std::ceil(details_->document()->size().height()));
        const int neededH = (std::max)(150, docH + 16);
        if (details_->minimumHeight() != neededH)
        {
            details_->setMinimumHeight(neededH);
            details_->updateGeometry();
        }
    }

    void Poll()
    {
        if (pending_ || !host_ || !host_->HasScene())
            return;
        unsigned mask = 0;
        if (enabled_->isChecked())
        {
            for (int i = 0; i < 8; ++i)
                if (layers_[i]->isChecked())
                    mask |= 1u << i;
        }
        if (!mask && !wasEnabled_)
            return;
        wasEnabled_ = mask != 0;

        EngineCameraInfo cam{};
        host_->GetCamera(cam);
        float origin[3]{}, direction[3]{}, hit[3]{};
        float plane[3]{0, static_cast<float>(height_->value()), 0}, normal[3]{0, 1, 0};
        float cx = cam.position[0], cz = cam.position[2];
        if (GizmoGeometry::ScreenToWorldRay(cam.width * 0.5f, cam.height * 0.5f, cam, origin, direction) &&
            GizmoGeometry::RayPlaneIntersection(origin, direction, plane, normal, hit))
        {
            cx = hit[0];
            cz = hit[2];
        }

        QJsonObject q{
            {"layers", static_cast<int>(mask)},
            {"object", static_cast<double>(host_->ReadSnapshot().selected)},
            {"controller", units_->currentData().toInt()},
            {"radius", radius_->value()},
            {"agentRadius", agentRadius_->isChecked()},
            {"selectedOnly", selected_->isChecked()},
            {"height", height_->value()},
            {"extent", extent_->value()},
            {"centerX", cx},
            {"centerZ", cz},
            {"xray", xray_->isChecked()},
            {"fromX", fromX_->value()},
            {"fromZ", fromZ_->value()},
            {"toX", toX_->value()},
            {"toZ", toZ_->value()},
            {"hover", hover_},
            {"hoverX", hoverX_},
            {"hoverZ", hoverZ_}
        };

        const auto bytes = QJsonDocument(q).toJson(QJsonDocument::Compact);
        pending_ = true;
        QPointer<AiDiagnosticsPanel> self(this);
        auto* host = host_;
        EditorCommand c;
        c.type = EditorCommand::RunTask;
        c.task = [self, host, bytes](const EngineEditSessionApi&) {
            QByteArray reply;
            uint32_t status = EngineSceneNotFound;
            if (const auto* api = host->AiDiagnosticsApi())
                status = api->inspect(
                    api->context, bytes.constData(),
                    [](void* user, const char* text) { *static_cast<QByteArray*>(user) = text; }, &reply);
            host->RunOnUi([self, reply, status] {
                if (!self) return;
                self->pending_ = false;
                self->Display(reply, status);
            });
        };
        host_->PostCommand(c);
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (object != viewport_ || !enabled_->isChecked())
            return false;
        if (event->type() == QEvent::Leave)
        {
            hover_ = false;
            return false;
        }
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape && pick_)
        {
            pick_ = 0;
            return true;
        }
        if (event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonPress)
            return false;

        auto* mouse = static_cast<QMouseEvent*>(event);
        EngineCameraInfo cam{};
        if (!host_->GetCamera(cam))
            return false;

        float origin[3], dir[3], hit[3], point[3]{0, static_cast<float>(height_->value()), 0}, normal[3]{0, 1, 0};
        hover_ = GizmoGeometry::ScreenToWorldRay(
                     static_cast<float>(mouse->position().x() * cam.width / (std::max)(1, viewport_->width())),
                     static_cast<float>(mouse->position().y() * cam.height / (std::max)(1, viewport_->height())), cam, origin, dir) &&
                 GizmoGeometry::RayPlaneIntersection(origin, dir, point, normal, hit);
        if (hover_)
        {
            hoverX_ = hit[0];
            hoverZ_ = hit[2];
        }
        if (pick_ && event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton)
        {
            if (hover_)
            {
                (pick_ == 1 ? fromX_ : toX_)->setValue(hit[0]);
                (pick_ == 1 ? fromZ_ : toZ_)->setValue(hit[2]);
                layers_[7]->setChecked(true);
                pick_ = 0;
                Poll();
            }
            return true;
        }
        return false;
    }

private:
    void LoadSettings()
    {
        auto& s = SettingsService::Instance();
        enabled_->setChecked(s.GetValue(QStringLiteral("ai/enabled"), false).toBool());
        for (int i = 0; i < 8; ++i)
            layers_[i]->setChecked(s.GetValue(QStringLiteral("ai/layer%1").arg(i), i == 0).toBool());
        selected_->setChecked(s.GetValue(QStringLiteral("ai/selectedOnly"), true).toBool());
        agentRadius_->setChecked(s.GetValue(QStringLiteral("ai/agentRadius"), true).toBool());
        radius_->setValue(std::clamp(s.GetValue(QStringLiteral("ai/radius"), 0.5).toDouble(), 0.01, 100.0));
        height_->setValue(std::clamp(s.GetValue(QStringLiteral("ai/planeHeight"), 0.15).toDouble(), -1000.0, 1000.0));
        extent_->setValue(std::clamp(s.GetValue(QStringLiteral("ai/extent"), 60.0).toDouble(), 5.0, 500.0));
        xray_->setChecked(s.GetValue(QStringLiteral("ai/xray"), false).toBool());
    }

    void SaveSettings()
    {
        auto& s = SettingsService::Instance();
        s.SetValue(QStringLiteral("ai/enabled"), enabled_->isChecked());
        for (int i = 0; i < 8; ++i) s.SetValue(QStringLiteral("ai/layer%1").arg(i), layers_[i]->isChecked());
        s.SetValue(QStringLiteral("ai/selectedOnly"), selected_->isChecked());
        s.SetValue(QStringLiteral("ai/agentRadius"), agentRadius_->isChecked());
        s.SetValue(QStringLiteral("ai/radius"), radius_->value());
        s.SetValue(QStringLiteral("ai/planeHeight"), height_->value());
        s.SetValue(QStringLiteral("ai/extent"), extent_->value());
        s.SetValue(QStringLiteral("ai/xray"), xray_->isChecked());
    }

    NoWheelDoubleSpinBox* Spin(const char* name, double value, double lo = -100000.0, double hi = 100000.0, QWidget* parent = nullptr)
    {
        auto* s = new NoWheelDoubleSpinBox(parent);
        s->setObjectName(QString::fromLatin1(name));
        s->setFixedHeight(InspectorCardStyle::kFieldHeight);
        s->setRange(lo, hi);
        s->setDecimals(2);
        s->setSingleStep(0.25);
        s->setValue(value);
        return s;
    }

    QWidget* Pair(QWidget* a, QWidget* b)
    {
        auto* w = new QWidget;
        auto* l = new QHBoxLayout(w);
        InspectorCardStyle::ApplyRowLayout(l);
        l->addWidget(a);
        l->addWidget(b);
        return w;
    }

    void Display(const QByteArray& data, uint32_t status)
    {
        if (status != EngineSceneOk)
        {
            source_->setVisible(true);
            source_->setText(Tr("Диагностика недоступна: модуль игры собран без поддержки AI. Соберите проект через run-editor.ps1 (без флага -Fast).",
                                "Diagnostics unavailable: game module was built without AI support. Build project via run-editor.ps1 (without -Fast flag)."));
            source_->setStyleSheet(QStringLiteral("font-size: 11px; padding: 6px 10px; border-radius: 4px; background: rgba(230, 160, 40, 0.15); border: 1px solid rgba(230, 160, 40, 0.35); color: #e6a028;"));
            details_->clear();
            UpdateDetailsGeometry();
            return;
        }
        auto r = QJsonDocument::fromJson(data).object();
        const auto src = r["source"].toString();
        if (src == "off")
        {
            source_->setVisible(false);
            source_->setText(Tr("Слои выключены", "Layers off"));
            details_->clear();
            UpdateDetailsGeometry();
            return;
        }

        if (src == "preview")
        {
            source_->setVisible(true);
            source_->setStyleSheet(QStringLiteral("font-size: 11px; padding: 8px 10px; border-radius: 4px; background: rgba(80, 140, 220, 0.12); border: 1px solid rgba(80, 140, 220, 0.3);"));
            QString banner = QStringLiteral("<b style='font-size: 11px;'>") +
                             Tr("EDIT · Режим границ NavVolume", "EDIT · NavVolume Bounds Mode") +
                             QStringLiteral("</b><br>");
            if (!r["configured"].toBool())
            {
                banner += Tr("Нет границ навигации: добавьте NavVolume в режиме «Границы».",
                             "No navigation bounds: add a Bounds NavVolume.");
            }
            else
            {
                banner += Tr("Сетка навигации: %1 × %2 · %3 м · r = %4 м",
                             "Navigation grid: %1 × %2 · cell %3 m · r = %4 m")
                              .arg(r["width"].toInt()).arg(r["height"].toInt())
                              .arg(r["cellSize"].toDouble()).arg(r["radius"].toDouble());
            }
            if (r["stride"].toInt() > 1)
            {
                banner += QStringLiteral("<br>") +
                          Tr("Прореживание: каждая %1-я клетка. Приблизьте область.",
                             "Sampling every %1 cells. Reduce the area.").arg(r["stride"].toInt());
            }
            if (!pick_) source_->setText(banner);
        }
        else
        {
            source_->setVisible(false);
        }

        const auto newUnits = r["units"].toArray();
        bool unitsChanged = units_->count() != newUnits.size() + 1;
        for (int i = 0; !unitsChanged && i < newUnits.size(); ++i)
        {
            const auto unit = newUnits[i].toObject();
            unitsChanged = units_->itemData(i + 1).toInt() != unit["id"].toInt() ||
                           units_->itemText(i + 1) != unit["name"].toString();
        }
        if (unitsChanged && !units_->view()->isVisible())
        {
            QSignalBlocker blocker(units_);
            const int chosen = units_->currentData().toInt();
            units_->clear();
            units_->addItem(Tr("Выделенный объект сцены", "Scene selection"), 0);
            for (auto v : r["units"].toArray())
            {
                auto u = v.toObject();
                units_->addItem(u["name"].toString(), u["id"].toInt());
            }
            units_->setCurrentIndex((std::max)(0, units_->findData(chosen)));
        }

        const auto path = r["path"].toString();
        QString probe;
        if (!path.isEmpty())
            probe = path == "found" ? Tr("A → B: путь найден", "A → B: path found")
                    : path == "partial"
                        ? Tr("A → B: частичный путь / лимит поиска", "A → B: partial path / search budget")
                        : Tr("A → B: путь не найден", "A → B: no path");
        if (layers_[4]->isChecked())
            probe += r["field"] == "none"       ? Tr("\nВыберите отряд с активным полем в Play.",
                                                     "\nSelect a squad with an active field in Play.")
                     : r["field"] == "building" ? Tr("\nПоле рассчитывается…", "\nBuilding field…")
                                                : Tr("\nПоле готово", "\nField ready");
        if (r["field"] == "stale")
            probe = Tr("Поле устарело: навигация изменилась.", "Field is stale: navigation changed.");
        if (src == "preview" && layers_[4]->isChecked())
            probe += Tr("\nПоле предпросмотра к точке B.", "\nPreview field towards B.");
        probeStatus_->setText(probe);

        if (r.contains("cell"))
        {
            auto c = r["cell"].toObject();
            cell_->setText(Tr("Клетка [%1, %2] · стоимость %3", "Cell [%1, %2] · cost %3")
                               .arg(c["x"].toInt()).arg(c["z"].toInt()).arg(c["cost"].toInt()));
            QString details = Tr("Зазор: %1 м · Статус: %2", "Clearance: %1 m · Status: %2")
                                  .arg(c["clearance"].toDouble(), 0, 'f', 2)
                                  .arg(c["walkable"].toBool() ? Tr("проходима", "walkable") : Tr("непроходима", "blocked"));
            if (c.contains("modifier"))
                details += Tr("\nВлияние: %1", "\nModifier: %1").arg(c["modifier"].toString());
            cellDetailsLabel_->setText(details);
            cellDetails_->setVisible(true);
        }
        else
        {
            cell_->setText(Tr("Клетка: наведите курсор во вьюпорте", "Cell: hover viewport"));
            cellDetails_->setVisible(false);
        }

        const QString details = r["details"].toString(
            Tr("Выберите AI-персонажа в сцене или отряд в списке. Состояния, задачи и память доступны в Play.",
               "Select an AI character or squad. States, tasks and memory are available in Play."));
        if (details_->toPlainText() != details)
        {
            details_->setPlainText(details);
            UpdateDetailsGeometry();
        }
    }

    EditorQtHost* host_{nullptr};
    QWidget* viewport_{nullptr};
    QTimer* timer_{nullptr};
    bool pending_{false}, wasEnabled_{false}, hover_{false};
    int pick_{0};
    float hoverX_{0.0f}, hoverZ_{0.0f};
    QCheckBox *enabled_{nullptr}, *selected_{nullptr}, *agentRadius_{nullptr}, *xray_{nullptr};
    std::array<QCheckBox*, 8> layers_{};
    QComboBox* units_{nullptr};
    QLabel *source_{nullptr}, *legend_{nullptr}, *cell_{nullptr}, *cellDetailsLabel_{nullptr}, *probeStatus_{nullptr};
    QWidget* cellDetails_{nullptr};
    QPlainTextEdit* details_{nullptr};
    NoWheelDoubleSpinBox *radius_{nullptr}, *height_{nullptr}, *extent_{nullptr}, *fromX_{nullptr}, *fromZ_{nullptr}, *toX_{nullptr}, *toZ_{nullptr};
};
