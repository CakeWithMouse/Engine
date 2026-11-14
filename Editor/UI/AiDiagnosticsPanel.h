#pragma once
#include "EditorLocale.h"
#include "GizmoGeometry.h"
#include "QtHost.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <array>

// UI owns configuration only. All scene access runs through RunTask on the engine thread.
class AiDiagnosticsPanel : public QWidget
{
  public:
    AiDiagnosticsPanel(EditorQtHost *host, QWidget *viewport, QWidget *parent = nullptr)
        : QWidget(parent), host_(host), viewport_(viewport)
    {
        setObjectName("AiDiagnosticsPanel");
        setMinimumWidth(390);
        setMaximumWidth(460);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(8, 8, 8, 8);
        enabled_ = new QCheckBox(Tr("Показывать диагностику AI", "Show AI diagnostics"), this);
        enabled_->setObjectName("AiEnabled");
        outer->addWidget(enabled_);
        source_ = new QLabel(Tr("Слои выключены", "Layers off"), this);
        source_->setWordWrap(true);
        outer->addWidget(source_);
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setFrameShape(QFrame::NoFrame);
        outer->addWidget(scroll, 1);
        auto *body = new QWidget;
        body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        scroll->setWidget(body);
        auto *layout = new QVBoxLayout(body);
        layout->setContentsMargins(0, 0, 5, 0);
        layout->setSpacing(9);
        auto *layers = new QGroupBox(Tr("Слои во вьюпорте", "Viewport layers"));
        auto *layerLayout = new QGridLayout(layers);
        layerLayout->setSpacing(3);
        const char *names[] = {"Навигационная сетка", "Стоимость прохода",    "Проходимость по радиусу",
                               "Пути и цели агентов", "Поле направлений",     "Восприятие и цели",
                               "Строй отряда",        "Пробный маршрут A → B"};
        const char *en[] = {"Navigation grid", "Traversal cost",         "Agent clearance", "Agent paths and goals",
                            "Direction field", "Perception and targets", "Squad formation", "Test route A → B"};
        for (int i = 0; i < 8; ++i)
        {
            layers_[i] = new QCheckBox(Tr(names[i], en[i]));
            layers_[i]->setObjectName(QString("AiLayer%1").arg(i));
            layerLayout->addWidget(layers_[i], i / 2, i % 2);
            connect(layers_[i], &QCheckBox::toggled, this, [this] { Poll(); });
        }
        layers_[0]->setChecked(true);
        layout->addWidget(layers);
        legend_ = new QLabel(Tr("● Зелёный — проход   ● Красный — препятствие\n● Жёлтый — стоимость / тесно для "
                                "агента\nA — старт, B — цель; голубой — направление",
                                "Green: walkable · Red: blocked\nYellow: cost / insufficient clearance\nA: start · B: "
                                "goal · Cyan: direction"));
        legend_->setWordWrap(true);
        layout->addWidget(legend_);
        auto *agent = new QGroupBox(Tr("Объект диагностики", "Diagnostic subject"));
        auto *af = new QFormLayout(agent);
        units_ = new QComboBox;
        units_->setObjectName("AiSubject");
        units_->addItem(Tr("Выделенный объект сцены", "Scene selection"), 0);
        af->addRow(units_);
        selected_ = new QCheckBox(Tr("Только выбранный агент / отряд", "Selected agent / squad only"));
        selected_->setChecked(true);
        af->addRow(selected_);
        agentRadius_ = new QCheckBox(Tr("Брать радиус выбранного агента", "Use selected agent radius"));
        agentRadius_->setChecked(true);
        af->addRow(agentRadius_);
        radius_ = Spin("AiRadius", .5, 0, 100);
        af->addRow(Tr("Радиус, м", "Radius, m"), radius_);
        layout->addWidget(agent);
        auto *probe = new QGroupBox(Tr("Проба навигации · плоскость XZ", "Navigation probe · XZ plane"));
        auto *pf = new QFormLayout(probe);
        fromX_ = Spin("AiFromX", -10);
        fromZ_ = Spin("AiFromZ", 0);
        toX_ = Spin("AiToX", 10);
        toZ_ = Spin("AiToZ", 0);
        pf->addRow("A · X / Z", Pair(fromX_, fromZ_));
        pf->addRow("B · X / Z", Pair(toX_, toZ_));
        auto *picks = new QWidget;
        auto *pl = new QHBoxLayout(picks);
        pl->setContentsMargins(0, 0, 0, 0);
        for (int i = 1; i <= 2; ++i)
        {
            auto *b = new QPushButton(i == 1 ? Tr("Указать A", "Pick A") : Tr("Указать B", "Pick B"));
            b->setObjectName(i == 1 ? "AiPickA" : "AiPickB");
            pl->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, i] {
                pick_ = i;
                source_->setText(
                    Tr("Щёлкните по плоскости во вьюпорте. Esc — отмена.", "Click the viewport plane. Esc cancels."));
            });
        }
        pf->addRow(picks);
        probeStatus_ = new QLabel;
        probeStatus_->setWordWrap(true);
        pf->addRow(probeStatus_);
        layout->addWidget(probe);
        auto *view = new QGroupBox(Tr("Отображение", "Display"));
        auto *vf = new QFormLayout(view);
        height_ = Spin("AiPlaneHeight", .15, -10000, 10000);
        vf->addRow(Tr("Высота плоскости, м", "Plane height, m"), height_);
        extent_ = Spin("AiExtent", 60, 5, 500);
        vf->addRow(Tr("Область у камеры, м", "Camera area, m"), extent_);
        xray_ = new QCheckBox(Tr("Поверх геометрии", "Show through geometry"));
        vf->addRow(xray_);
        layout->addWidget(view);
        cell_ = new QLabel(Tr("Наведите мышь на клетку во вьюпорте", "Hover a viewport cell"));
        cell_->setWordWrap(true);
        cell_->setObjectName("AiCellInfo");
        layout->addWidget(cell_);
        layout->addWidget(new QLabel(Tr("Состояние · задачи · память · история", "State · tasks · memory · history")));
        details_ = new QPlainTextEdit;
        details_->setObjectName("AiDetails");
        details_->setReadOnly(true);
        details_->setMinimumHeight(165);
        layout->addWidget(details_);
        layout->addStretch();
        viewport_->installEventFilter(this);
        viewport_->setMouseTracking(true);
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, [this] { Poll(); });
        timer_->start(180);
        connect(enabled_, &QCheckBox::toggled, this, [this] { Poll(); });
    }
    QSize sizeHint() const override
    {
        return QSize(410, 720);
    }
    QSize minimumSizeHint() const override
    {
        return QSize(390, 180);
    }
    void Poll()
    {
        if (pending_ || !host_->HasScene())
            return;
        unsigned mask = 0;
        if (enabled_->isChecked())
            for (int i = 0; i < 8; ++i)
                if (layers_[i]->isChecked())
                    mask |= 1u << i;
        if (!mask && !wasEnabled_)
            return;
        wasEnabled_ = mask != 0;
        EngineCameraInfo cam{};
        host_->GetCamera(cam);
        float origin[3]{}, direction[3]{}, hit[3]{};
        float plane[3]{0, float(height_->value()), 0}, normal[3]{0, 1, 0};
        float cx = cam.position[0], cz = cam.position[2];
        if (GizmoGeometry::ScreenToWorldRay(cam.width * .5f, cam.height * .5f, cam, origin, direction) &&
            GizmoGeometry::RayPlaneIntersection(origin, direction, plane, normal, hit))
        {
            cx = hit[0];
            cz = hit[2];
        }
        QJsonObject q{{"layers", int(mask)},
                      {"object", double(host_->ReadSnapshot().selected)},
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
                      {"hoverZ", hoverZ_}};
        const auto bytes = QJsonDocument(q).toJson(QJsonDocument::Compact);
        pending_ = true;
        QPointer<AiDiagnosticsPanel> self(this);
        auto *host = host_;
        EditorCommand c;
        c.type = EditorCommand::RunTask;
        c.task = [self, host, bytes](const EngineEditSessionApi &) {
            QByteArray reply;
            uint32_t status = EngineSceneNotFound;
            if (const auto *api = host->AiDiagnosticsApi())
                status = api->inspect(
                    api->context, bytes.constData(),
                    [](void *user, const char *text) { *static_cast<QByteArray *>(user) = text; }, &reply);
            host->RunOnUi([self, reply, status] {
                if (!self)
                    return;
                self->pending_ = false;
                self->Display(reply, status);
            });
        };
        host_->PostCommand(c);
    }

  protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object != viewport_ || !enabled_->isChecked())
            return false;
        if (event->type() == QEvent::Leave)
        {
            hover_ = false;
            return false;
        }
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape && pick_)
        {
            pick_ = 0;
            return true;
        }
        if (event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonPress)
            return false;
        auto *mouse = static_cast<QMouseEvent *>(event);
        EngineCameraInfo cam{};
        if (!host_->GetCamera(cam))
            return false;
        float origin[3], dir[3], hit[3], point[3]{0, float(height_->value()), 0}, normal[3]{0, 1, 0};
        hover_ = GizmoGeometry::ScreenToWorldRay(
                     float(mouse->position().x() * cam.width / std::max(1, viewport_->width())),
                     float(mouse->position().y() * cam.height / std::max(1, viewport_->height())), cam, origin, dir) &&
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
    QDoubleSpinBox *Spin(const char *name, double value, double lo = -100000, double hi = 100000)
    {
        auto *s = new QDoubleSpinBox;
        s->setObjectName(name);
        s->setRange(lo, hi);
        s->setDecimals(2);
        s->setSingleStep(.25);
        s->setValue(value);
        return s;
    }
    QWidget *Pair(QWidget *a, QWidget *b)
    {
        auto *w = new QWidget;
        auto *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(a);
        l->addWidget(b);
        return w;
    }
    void Display(const QByteArray &data, uint32_t status)
    {
        if (status != EngineSceneOk)
        {
            source_->setText(Tr("Диагностика недоступна. Пересоберите игровой модуль с текущим SDK.",
                                "Diagnostics unavailable. Rebuild the game module with the current SDK."));
            return;
        }
        auto r = QJsonDocument::fromJson(data).object();
        const auto src = r["source"].toString();
        if (src == "off")
        {
            source_->setText(Tr("Слои выключены", "Layers off"));
            details_->clear();
            return;
        }
        QString label = src == "preview"  ? Tr("EDIT · Предпросмотр NavVolume", "EDIT · NavVolume preview")
                        : src == "paused" ? Tr("ПАУЗА · Данные AI", "PAUSED · AI data")
                                          : Tr("PLAY · Данные AI", "PLAY · AI data");
        if (!r["configured"].toBool())
            label += Tr("\nНет границ навигации: добавьте NavVolume в режиме «Границы».",
                        "\nNo navigation bounds: add a Bounds NavVolume.");
        else
            label += QString("\n%1 × %2 · %3 m · r = %4 m")
                         .arg(r["width"].toInt())
                         .arg(r["height"].toInt())
                         .arg(r["cellSize"].toDouble())
                         .arg(r["radius"].toDouble());
        if (r["stride"].toInt() > 1)
            label += Tr("\nПрореживание: каждая %1-я клетка. Приблизьте область.",
                        "\nSampling every %1 cells. Reduce the area.")
                         .arg(r["stride"].toInt());
        if (!pick_)
            source_->setText(label);
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
            units_->setCurrentIndex(std::max(0, units_->findData(chosen)));
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
            cell_->setText(
                Tr("Клетка [%1, %2] · стоимость %3\nЗазор %4 м · %5", "Cell [%1, %2] · cost %3\nClearance %4 m · %5")
                    .arg(c["x"].toInt())
                    .arg(c["z"].toInt())
                    .arg(c["cost"].toInt())
                    .arg(c["clearance"].toDouble(), 0, 'f', 2)
                    .arg(c["walkable"].toBool() ? Tr("проходима", "walkable") : Tr("непроходима", "blocked")));
        }
        else
            cell_->setText(Tr("Наведите мышь на клетку во вьюпорте", "Hover a viewport cell"));
        const QString details = r["details"].toString(
            Tr("Выберите AI-персонажа в сцене или отряд в списке. Состояния, задачи и память доступны в Play.",
               "Select an AI character or squad. States, tasks and memory are available in Play."));
        if (details_->toPlainText() != details)
            details_->setPlainText(details);
    }
    EditorQtHost *host_;
    QWidget *viewport_;
    QTimer *timer_;
    bool pending_ = false, wasEnabled_ = false, hover_ = false;
    int pick_ = 0;
    float hoverX_ = 0, hoverZ_ = 0;
    QCheckBox *enabled_, *selected_, *agentRadius_, *xray_;
    std::array<QCheckBox *, 8> layers_{};
    QComboBox *units_;
    QLabel *source_, *legend_, *cell_, *probeStatus_;
    QPlainTextEdit *details_;
    QDoubleSpinBox *radius_, *height_, *extent_, *fromX_, *fromZ_, *toX_, *toZ_;
};
