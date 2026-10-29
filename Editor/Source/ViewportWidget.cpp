#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "EditorDialogs.h"
#include "EditorLocale.h"

#include <type_traits>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <cmath>
#include <memory>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCursor>
#include <QApplication>
#include <Engine/InputDevice/Keys.h>

namespace {
uint32_t GameKey(QKeyEvent* key)
{
    // Native virtual keys for arbitrary bindings; physical WASD stays layout independent.
    int logical=CameraNavigationKey(key->nativeScanCode(),key->key());
    if (logical=='W'||logical=='A'||logical=='S'||logical=='D'||logical=='Q'||logical=='E')return logical;
    if (key->nativeVirtualKey() && key->nativeVirtualKey()<256) return key->nativeVirtualKey();
    if (logical>=0x20&&logical<=0x7e)return logical;
    switch(logical){case Qt::Key_Control:return 17;case Qt::Key_Shift:return 16;case Qt::Key_Alt:return 18;
    case Qt::Key_Escape:return 27;case Qt::Key_Tab:return 9;case Qt::Key_Return:return 13;
    case Qt::Key_Left:return 37;case Qt::Key_Up:return 38;case Qt::Key_Right:return 39;case Qt::Key_Down:return 40;
    default:return logical>=Qt::Key_F1&&logical<=Qt::Key_F24?112+logical-Qt::Key_F1:0;}
}
}

namespace
{
    QString GizmoModeName(EditorGizmoMode mode)
    {
        switch (mode)
        {
        case EditorGizmoTranslate:
            return QStringLiteral("Move");
        case EditorGizmoRotate:
            return QStringLiteral("Rotate");
        case EditorGizmoScale:
            return QStringLiteral("Scale");
        default:
            return QStringLiteral("Unknown");
        }
    }

    void NormalizeQuat(double q[4])
    {
        const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

        if (len < 1e-6)
        {
            q[0] = 0.0;
            q[1] = 0.0;
            q[2] = 0.0;
            q[3] = 1.0;
            return;
        }

        const double inv = 1.0 / len;
        q[0] *= inv;
        q[1] *= inv;
        q[2] *= inv;
        q[3] *= inv;
    }

    void MulQuat(const double a[4], const double b[4], double out[4])
    {
        const double x =
            a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];

        const double y =
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];

        const double z =
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];

        const double w =
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];

        out[0] = x;
        out[1] = y;
        out[2] = z;
        out[3] = w;
    }

    void AxisAngleQuat(double x, double y, double z, double angle, double out[4])
    {
        const double len = std::sqrt(x * x + y * y + z * z);

        if (len < 1e-6)
        {
            out[0] = 0.0;
            out[1] = 0.0;
            out[2] = 0.0;
            out[3] = 1.0;
            return;
        }

        const double inv = 1.0 / len;
        const double half = angle * 0.5;
        const double s = std::sin(half);

        out[0] = x * inv * s;
        out[1] = y * inv * s;
        out[2] = z * inv * s;
        out[3] = std::cos(half);
    }
}

class AdjustLastOperationPanel : public QFrame
{
public:
    explicit AdjustLastOperationPanel(EditorQtHost* host, QWidget* parent = nullptr)
        : QFrame(parent), host_(host)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(QStringLiteral(
            "AdjustLastOperationPanel {"
            "  background-color: rgba(28, 30, 36, 235);"
            "  border: 1px solid rgba(80, 85, 95, 200);"
            "  border-radius: 6px;"
            "}"
            "QLabel {"
            "  color: #bbb;"
            "  font-size: 11px;"
            "}"
            "QLineEdit, QDoubleSpinBox {"
            "  background: #202228;"
            "  color: #eee;"
            "  border: 1px solid #444;"
            "  border-radius: 3px;"
            "  font-size: 11px;"
            "  padding: 1px 3px;"
            "}"
            "QPushButton#CloseBtn {"
            "  background: transparent;"
            "  color: #888;"
            "  border: none;"
            "  font-weight: bold;"
            "}"
            "QPushButton#CloseBtn:hover {"
            "  color: #fff;"
            "}"
        ));

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 6, 8, 8);
        mainLayout->setSpacing(4);

        auto* headerLayout = new QHBoxLayout;
        auto* titleLabel = new QLabel(Tr("Параметры операции", "Adjust Last Operation"), this);
        titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #4fa3ff;"));
        auto* closeBtn = new QPushButton(QStringLiteral("✕"), this);
        closeBtn->setObjectName(QStringLiteral("CloseBtn"));
        closeBtn->setFixedSize(16, 16);
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        auto* nameLayout = new QHBoxLayout;
        nameLayout->addWidget(new QLabel(Tr("Имя", "Name") + QStringLiteral(":"), this));
        nameEdit_ = new QLineEdit(this);
        nameLayout->addWidget(nameEdit_, 1);
        mainLayout->addLayout(nameLayout);

        connect(nameEdit_, &QLineEdit::editingFinished, this, [this] {
            if (updating_ || !objectId_ || !host_) return;
            EditorCommand c;
            c.type = EditorCommand::RenameObject;
            c.objectId = objectId_;
            c.name = nameEdit_->text().trimmed();
            if (!c.name.isEmpty()) host_->PostCommand(c);
        });

        auto* posLayout = new QHBoxLayout;
        posLayout->addWidget(new QLabel(Tr("Позиция", "Pos") + QStringLiteral(":"), this));
        posX_ = new QDoubleSpinBox(this);
        posY_ = new QDoubleSpinBox(this);
        posZ_ = new QDoubleSpinBox(this);
        for (auto* s : {posX_, posY_, posZ_}) {
            s->setRange(-10000.0, 10000.0);
            s->setDecimals(2);
            s->setSingleStep(0.5);
            posLayout->addWidget(s);
            connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (updating_ || !objectId_ || !host_) return;
                transform_.position[0] = static_cast<float>(posX_->value());
                transform_.position[1] = static_cast<float>(posY_->value());
                transform_.position[2] = static_cast<float>(posZ_->value());
                EditorCommand c;
                c.type = EditorCommand::UpdateObjectTransform;
                c.objectId = objectId_;
                c.transform = transform_;
                c.path = QStringLiteral("adjust");
                host_->PostCommand(c);
            });
        }
        mainLayout->addLayout(posLayout);

        auto* scaleLayout = new QHBoxLayout;
        scaleLayout->addWidget(new QLabel(Tr("Масштаб", "Scale") + QStringLiteral(":"), this));
        scaleX_ = new QDoubleSpinBox(this);
        scaleY_ = new QDoubleSpinBox(this);
        scaleZ_ = new QDoubleSpinBox(this);
        for (auto* s : {scaleX_, scaleY_, scaleZ_}) {
            s->setRange(0.001, 1000.0);
            s->setDecimals(2);
            s->setSingleStep(0.2);
            scaleLayout->addWidget(s);
            connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (updating_ || !objectId_ || !host_) return;
                transform_.scale[0] = static_cast<float>(scaleX_->value());
                transform_.scale[1] = static_cast<float>(scaleY_->value());
                transform_.scale[2] = static_cast<float>(scaleZ_->value());
                EditorCommand c;
                c.type = EditorCommand::UpdateObjectTransform;
                c.objectId = objectId_;
                c.transform = transform_;
                c.path = QStringLiteral("adjust");
                host_->PostCommand(c);
            });
        }
        mainLayout->addLayout(scaleLayout);

        setMinimumWidth(230);
        hide();
    }

    void ShowForObject(uint64_t id, const QString& name, const EngineSceneTransform& transform)
    {
        objectId_ = id;
        transform_ = transform;
        updating_ = true;
        nameEdit_->setText(name);
        posX_->setValue(transform.position[0]);
        posY_->setValue(transform.position[1]);
        posZ_->setValue(transform.position[2]);
        scaleX_->setValue(transform.scale[0]);
        scaleY_->setValue(transform.scale[1]);
        scaleZ_->setValue(transform.scale[2]);
        updating_ = false;

        adjustSize();
        if (parentWidget()) {
            move(12, parentWidget()->height() - height() - 12);
        }
        show();
        raise();
    }

    void SetObjectId(uint64_t id) { objectId_ = id; }

private:
    EditorQtHost* host_ = nullptr;
    uint64_t objectId_ = 0;
    bool updating_ = false;
    EngineSceneTransform transform_{};

    QLineEdit* nameEdit_ = nullptr;
    QDoubleSpinBox* posX_ = nullptr;
    QDoubleSpinBox* posY_ = nullptr;
    QDoubleSpinBox* posZ_ = nullptr;
    QDoubleSpinBox* scaleX_ = nullptr;
    QDoubleSpinBox* scaleY_ = nullptr;
    QDoubleSpinBox* scaleZ_ = nullptr;
};

class PieMenuWidget : public QWidget
{
public:
    struct PieItem {
        QString label;
        int actionId;
    };

    explicit PieMenuWidget(std::function<void(int)> onSelect, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          onSelect_(std::move(onSelect))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFixedSize(260, 260);

        items_ = {
            {Tr("Перемещение (1)", "Move (1)"), 2},
            {Tr("Вращение (2)", "Rotate (2)"), 3},
            {Tr("Масштаб (3)", "Scale (3)"), 4},
            {Tr("Фокус (F)", "Focus (F)"), 5},
            {Tr("Удалить (Del)", "Delete (Del)"), 6},
            {Tr("Дубликат", "Duplicate"), 7},
            {Tr("+ Куб", "+ Add Cube"), 0},
            {Tr("+ Свет", "+ Add Light"), 1}
        };
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QPointF center(width() / 2.0, height() / 2.0);
        const double outerRadius = 120.0;
        const double innerRadius = 38.0;

        p.setPen(QPen(QColor(70, 75, 90, 220), 1.5));
        p.setBrush(QColor(22, 24, 30, 235));
        p.drawEllipse(center, outerRadius, outerRadius);

        for (int i = 0; i < 8; ++i)
        {
            const double midAngleDeg = i * 45.0;
            const double startAngleDeg = midAngleDeg - 22.5;

            if (hoveredSector_ == i)
            {
                QPainterPath path;
                path.moveTo(center);
                path.arcTo(QRectF(center.x() - outerRadius, center.y() - outerRadius, outerRadius * 2, outerRadius * 2), -startAngleDeg, -45.0);
                path.closeSubpath();

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(40, 110, 220, 170));
                p.drawPath(path);
            }

            const double rad = startAngleDeg * 3.1415926535 / 180.0;
            p.setPen(QPen(QColor(60, 65, 80, 150), 1.0));
            p.drawLine(center + QPointF(innerRadius * std::cos(rad), innerRadius * std::sin(rad)),
                       center + QPointF(outerRadius * std::cos(rad), outerRadius * std::sin(rad)));
        }

        p.setPen(QPen(QColor(80, 85, 105, 200), 1.5));
        p.setBrush(QColor(16, 18, 22, 250));
        p.drawEllipse(center, innerRadius, innerRadius);

        p.setPen(QColor(160, 165, 180));
        QFont hubFont = p.font();
        hubFont.setPointSize(8);
        hubFont.setBold(true);
        p.setFont(hubFont);
        p.drawText(QRectF(center.x() - innerRadius, center.y() - innerRadius, innerRadius * 2, innerRadius * 2),
                   Qt::AlignCenter, QStringLiteral("PIE"));

        QFont labelFont = p.font();
        labelFont.setPointSize(8);
        labelFont.setBold(false);
        p.setFont(labelFont);

        const double textRadius = 78.0;
        for (int i = 0; i < 8; ++i)
        {
            const double rad = (i * 45.0) * 3.1415926535 / 180.0;
            const QPointF textPos = center + QPointF(textRadius * std::cos(rad), textRadius * std::sin(rad));
            const QRectF textRect(textPos.x() - 45, textPos.y() - 14, 90, 28);

            p.setPen(hoveredSector_ == i ? QColor(255, 255, 255) : QColor(200, 205, 215));
            p.drawText(textRect, Qt::AlignCenter, items_[i].label);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPointF center(width() / 2.0, height() / 2.0);
        const QPointF diff = e->position() - center;
        const double dist = std::hypot(diff.x(), diff.y());

        int prevHover = hoveredSector_;
        if (dist > 30.0 && dist <= 125.0)
        {
            double angle = std::atan2(diff.y(), diff.x()) * 180.0 / 3.1415926535;
            if (angle < 0.0) angle += 360.0;
            hoveredSector_ = static_cast<int>(std::floor((angle + 22.5) / 45.0)) % 8;
        }
        else
        {
            hoveredSector_ = -1;
        }

        if (hoveredSector_ != prevHover)
            update();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
        {
            if (hoveredSector_ >= 0 && hoveredSector_ < 8 && onSelect_)
            {
                const int actId = items_[hoveredSector_].actionId;
                close();
                onSelect_(actId);
                return;
            }
            close();
        }
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape)
        {
            close();
        }
    }

private:
    std::function<void(int)> onSelect_;
    std::vector<PieItem> items_;
    int hoveredSector_ = -1;
};

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
    controlScheme_ = EditorSettings::GetControlScheme();
    controlSchemeSubId_ = EditorSettings::SubscribeControlSchemeChanged([this](ControlScheme scheme) {
        controlScheme_ = scheme;
        updateGizmoToolTip();
    });

    if (host_->HasViewport())
    {
        setAttribute(Qt::WA_NativeWindow, true);
        // Qt не закрашивает и не буферизует: сюда Present'ит движок
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
    }
    setFocusPolicy(Qt::StrongFocus);
    updateGizmoToolTip();

    adjustPanel_ = new AdjustLastOperationPanel(host_, this);

    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    connect(timer, &QTimer::timeout, this, [this, elapsed]
    {
        const float dt = std::min(0.05f, static_cast<float>(elapsed->restart()) / 1000.0f);
        const int state=host_->PlayState();
        if(inputState_!=state){inputState_=state;keys_.clear();looking_=rmbPressed_=false;EndObjectDrag();}
        if (!attached_ || !hasFocus() || !isActiveWindow()) return;
        if (state != EditorQtHost::Edit && host_->HasScene()) return;

        const float speed = dt * (keys_.contains(Qt::Key_Shift) ? 4.0f : 1.0f);

        // Если есть выделенный объект, управляем им, иначе камерой
        if (host_->HasSelection())
        {
            float right = static_cast<float>(keys_.contains(Qt::Key_Right) - keys_.contains(Qt::Key_Left)) * speed;
            float up = static_cast<float>(keys_.contains(Qt::Key_PageUp) - keys_.contains(Qt::Key_PageDown)) * speed;
            float forward = static_cast<float>(keys_.contains(Qt::Key_Up) - keys_.contains(Qt::Key_Down)) * speed;

            if (right != 0.0f || up != 0.0f || forward != 0.0f)
            {
                host_->MoveSelectedObject(right, up, forward);
            }
        }
        {
            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.right = static_cast<float>(keys_.contains(Qt::Key_D) - keys_.contains(Qt::Key_A)) * speed;
            c.up = static_cast<float>(keys_.contains(Qt::Key_E) - keys_.contains(Qt::Key_Q)) * speed;
            c.forward = static_cast<float>(keys_.contains(Qt::Key_W) - keys_.contains(Qt::Key_S)) * speed;
            if (c.right || c.up || c.forward)
            {
                host_->PostCommand(c);
                const float cy = std::cos(camYaw_);
                const float sy = std::sin(camYaw_);
                const float cp = std::cos(camPitch_);
                const float sp = std::sin(camPitch_);
                camPos_[0] += (sy * c.right + cy * cp * c.forward);
                camPos_[1] += (c.up + sp * c.forward);
                camPos_[2] += (-cy * c.right + sy * cp * c.forward);
            }
        }
    });
    timer->start(16);
    setMinimumSize(320, 200);
}

ViewportWidget::~ViewportWidget()
{
    if (controlSchemeSubId_ != 0)
    {
        EditorSettings::UnsubscribeControlSchemeChanged(controlSchemeSubId_);
        controlSchemeSubId_ = 0;
    }
}

QPaintEngine* ViewportWidget::paintEngine() const
{
    // nullptr только когда контент нативный; в режиме заглушки нужен обычный paintEngine
    return host_->HasViewport() ? nullptr : QWidget::paintEngine();
}

void ViewportWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (host_->HasViewport() && !attached_)
        SendAttach();
}

void ViewportWidget::hideEvent(QHideEvent* e)
{
    if (attached_)
    {
        EditorCommand c;
        c.type = EditorCommand::ViewportDetach;
        host_->PostCommand(c);
        attached_ = false;
    }
    QWidget::hideEvent(e);
}

void ViewportWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (attached_)
        SendResize();
    if (adjustPanel_ && adjustPanel_->isVisible())
        adjustPanel_->move(12, height() - adjustPanel_->height() - 12);
}

void ViewportWidget::paintEvent(QPaintEvent*)
{
    if (host_->HasViewport())
        return; // нативный контент, не трогаем
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setPen(QColor(200, 200, 200));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("No scene preview is available for this project."));
}

void ViewportWidget::SendAttach()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportAttach;
    c.hwnd = (void*)winId(); // создаёт нативное окно виджета
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
    attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportResize;
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
uint32_t ViewportWidget::PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

bool ViewportWidget::event(QEvent* e)
{
    if(e->type()==QEvent::FocusIn || (e->type()==QEvent::WindowActivate && hasFocus())){
        EditorCommand c;c.type=EditorCommand::InputFocus;c.w=1;host_->PostCommand(c);
    }
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate || e->type() == QEvent::Hide)
    {
        EditorCommand c;c.type=EditorCommand::InputFocus;c.w=0;host_->PostCommand(c);
        looking_ = false;
        rmbPressed_ = false;
        rmbMoved_ = false;
        mmbPressed_ = false;

        EndObjectDrag();

        keys_.clear();
    }

    // Qt owns editor/UI shortcuts and text widgets. Only this focused widget feeds the game.
    if(attached_ && host_->HasScene() && host_->PlayState()!=EditorQtHost::Edit)
    {
        if((e->type()==QEvent::KeyPress||e->type()==QEvent::KeyRelease)&&hasFocus()){
            auto* key=static_cast<QKeyEvent*>(e);if(key->isAutoRepeat())return true;
            const int nav=CameraNavigationKey(key->nativeScanCode(),key->key());
            if((nav==Qt::Key_Space||nav==Qt::Key_P)&&key->modifiers()==Qt::NoModifier){
                if(e->type()==QEvent::KeyPress){EditorCommand c;c.type=nav==Qt::Key_Space?EditorCommand::TogglePlay:EditorCommand::TogglePause;host_->PostCommand(c);}return true;
            }
            EditorCommand c;c.type=EditorCommand::InputKey;c.w=GameKey(key);c.h=e->type()==QEvent::KeyPress;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::MouseButtonPress||e->type()==QEvent::MouseButtonRelease){
            auto* mouse=static_cast<QMouseEvent*>(e);if(e->type()==QEvent::MouseButtonPress){activateWindow();setFocus(Qt::MouseFocusReason);lastMouse_=mouse->position();}
            EditorCommand c;c.type=EditorCommand::InputKey;c.w=static_cast<uint32_t>(mouse->button()==Qt::RightButton?Keys::RightButton:mouse->button()==Qt::MiddleButton?Keys::MiddleButton:Keys::LeftButton);
            c.h=e->type()==QEvent::MouseButtonPress;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::MouseMove&&hasFocus()){
            auto* mouse=static_cast<QMouseEvent*>(e);auto delta=mouse->position()-lastMouse_;lastMouse_=mouse->position();
            EditorCommand c;c.type=EditorCommand::InputPointer;c.yaw=static_cast<float>(delta.x());c.pitch=static_cast<float>(delta.y());host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::Wheel&&hasFocus()){
            EditorCommand c;c.type=EditorCommand::InputPointer;c.zoom=static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y())/120.f;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::ContextMenu)return true;
    }

    if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE") && e->type() == QEvent::KeyPress)
        host_->Log(qPrintable(
            QString("InputKey key=%1 focus=%2").arg(static_cast<QKeyEvent*>(e)->key()).arg(hasFocus())));

    // Глушим стандартное контекстное меню, чтобы показать своё.
    if (e->type() == QEvent::ContextMenu)
    {
        e->accept();
        return true;
    }

    if (e->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        activateWindow();
        setFocus(Qt::MouseFocusReason);

        if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE"))
            host_->Log(qPrintable(
                QString("InputFocus focus=%1 active=%2 policy=%3").arg(hasFocus()).arg(isActiveWindow()).arg(focusPolicy
                    ())));

        if (mouse->button() == Qt::LeftButton && attached_ && host_->HasScene())
        {
            lmbPressed_ = true;
            lmbMoved_ = false;
            draggingSelectedObject_ = false;

            lmbPressPos_ = mouse->position().toPoint();
            lastDragPos_ = mouse->position();

            EditorCommand c;
            c.type = EditorCommand::PickObject;
            c.right = static_cast<float>(2.0 * mouse->position().x() / width() - 1.0);
            c.up = static_cast<float>(1.0 - 2.0 * mouse->position().y() / height());
            host_->PostCommand(c);

            return true;
        }
        if (mouse->button() == Qt::RightButton)
        {
            rmbPressed_ = true;
            rmbMoved_ = false;
            rmbPressPos_ = mouse->position().toPoint();
            lastMouse_ = mouse->position();
            return true;
        }
        if (mouse->button() == Qt::MiddleButton)
        {
            mmbPressed_ = true;
            mmbPressPos_ = mouse->position().toPoint();
            lastMouse_ = mouse->position();
            return true;
        }
    }

    if (e->type() == QEvent::MouseMove)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (mmbPressed_)
        {
            const auto delta = mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();
            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            if (mouse->modifiers() & Qt::ShiftModifier)
            {
                c.right = static_cast<float>(-delta.x()) * 0.01f;
                c.up = static_cast<float>(delta.y()) * 0.01f;
            }
            else
            {
                c.yaw = static_cast<float>(delta.x()) * 0.005f;
                c.pitch = static_cast<float>(delta.y()) * 0.005f;
            }
            host_->PostCommand(c);
            return true;
        }

        if (rmbPressed_ && !rmbMoved_)
        {
            const QPoint delta = mouse->position().toPoint() - rmbPressPos_;

            if (delta.manhattanLength() >= 4)
            {
                rmbMoved_ = true;
                looking_ = attached_;
                lastMouse_ = mouse->position();
            }
        }

        if (looking_)
        {
            const auto delta = mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();

            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.yaw = static_cast<float>(delta.x()) * 0.005f;
            c.pitch = static_cast<float>(delta.y()) * 0.005f;

            camYaw_ -= c.yaw;
            camPitch_ -= c.pitch;
            camPitch_ = std::max(-1.5f, std::min(1.5f, camPitch_));

            host_->PostCommand(c);
            return true;
        }

        if (lmbPressed_ && !draggingSelectedObject_)
        {
            const QPoint delta = mouse->position().toPoint() - lmbPressPos_;

            if (!lmbMoved_ && delta.manhattanLength() > 6)
                lmbMoved_ = true;

            if (lmbMoved_ && host_->HasSelection())
            {
                if (BeginObjectDrag(mouse->position()))
                    return true;
            }
        }

        if (draggingSelectedObject_)
        {
            if (gizmoMode_ == EditorGizmoTranslate)
            {
                const QPointF delta = mouse->position() - lastDragPos_;
                lastDragPos_ = mouse->position();

                float right = 0.0f;
                float up = 0.0f;
                float forward = 0.0f;

                if (mouse->modifiers() & Qt::ShiftModifier)
                {
                    forward = static_cast<float>(-delta.y()) * dragSensitivity_;
                }
                else
                {
                    right = static_cast<float>(delta.x()) * dragSensitivity_;
                    up = static_cast<float>(-delta.y()) * dragSensitivity_;
                }

                if (right != 0.0f || up != 0.0f || forward != 0.0f)
                    host_->MoveSelectedObject(right, up, forward);
            }
            else
            {
                ApplyRotateScaleGizmo(mouse->position());
                lastDragPos_ = mouse->position();
            }

            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonRelease)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (mouse->button() == Qt::LeftButton && (lmbPressed_ || draggingSelectedObject_))
        {
            EndObjectDrag();
            return true;
        }

        if (mouse->button() == Qt::MiddleButton)
        {
            mmbPressed_ = false;
            return true;
        }

        if (mouse->button() == Qt::RightButton)
        {
            looking_ = false;

            // Если не было движения мышью — показываем меню.
            if (rmbPressed_ && !rmbMoved_)
            {
                lastSpawnPos_ = ComputeSpawnPosition(mouse->position().toPoint());
                emit contextMenuRequested(mapToGlobal(mouse->position().toPoint()));
            }

            rmbPressed_ = false;
            rmbMoved_ = false;
            keys_.remove(Qt::Key_W);
            keys_.remove(Qt::Key_A);
            keys_.remove(Qt::Key_S);
            keys_.remove(Qt::Key_D);
            keys_.remove(Qt::Key_Q);
            keys_.remove(Qt::Key_E);

            return true;
        }
    }

    if (!attached_)
        return QWidget::event(e);

    if (e->type() == QEvent::Wheel && hasFocus())
    {
        EditorCommand c;
        c.type = EditorCommand::ViewportCamera;
        c.zoom = static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y()) / 120.0f;
        host_->PostCommand(c);
        return true;
    }

    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && hasFocus())
    {
        auto* key = static_cast<QKeyEvent*>(e);
        if ((key->modifiers() & Qt::ControlModifier) && e->type()==QEvent::KeyPress) return QWidget::event(e);

        if (!key->isAutoRepeat())
        {
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape) {
                EditorCommand c; c.type=EditorCommand::SelectObject; host_->PostCommand(c); return true;
            }
            // Обработка Tab для циклического выбора объектов
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_Tab)
            {
                host_->FetchSceneAndSelectNext();
                return true;
            }

            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_F && host_->HasScene())
            {
                EditorCommand c;
                c.type = EditorCommand::FocusObject;
                host_->PostCommand(c);
                return true;
            }

            if (e->type() == QEvent::KeyPress)
            {
                const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());

                // Shift+A: Quick Add Object menu under cursor
                if ((key->modifiers() & Qt::ShiftModifier) && (navigationKey == 'A' || key->key() == Qt::Key_A))
                {
                    lastSpawnPos_ = ComputeSpawnPosition(mapFromGlobal(QCursor::pos()));
                    emit addMenuRequested(QCursor::pos());
                    return true;
                }

                // Pie Menu on ~ / Russian Ё (scan code 0x29)
                if (navigationKey == '~' || key->key() == Qt::Key_AsciiTilde || key->key() == Qt::Key_QuoteLeft || key->nativeScanCode() == 0x29)
                {
                    lastSpawnPos_ = ComputeSpawnPosition(mapFromGlobal(QCursor::pos()));
                    auto* pie = new PieMenuWidget([this](int actionId) {
                        switch (actionId)
                        {
                        case 0:
                            emit addPrimitiveRequested(QStringLiteral("Primitive.Cube"), QStringLiteral("Cube"), lastSpawnPos_);
                            break;
                        case 1:
                            emit addPrimitiveRequested(QStringLiteral("Light.Point"), QStringLiteral("Point Light"), lastSpawnPos_);
                            break;
                        case 2:
                            setGizmoMode(EditorGizmoTranslate);
                            break;
                        case 3:
                            setGizmoMode(EditorGizmoRotate);
                            break;
                        case 4:
                            setGizmoMode(EditorGizmoScale);
                            break;
                        case 5:
                            {
                                EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
                            }
                            break;
                        case 6:
                            {
                                EditorCommand c; c.type = EditorCommand::RemoveObject; host_->PostCommand(c);
                            }
                            break;
                        case 7:
                            {
                                EditorCommand c; c.type = EditorCommand::DuplicateObject; host_->PostCommand(c);
                            }
                            break;
                        }
                    }, this);
                    const QPoint globalPos = QCursor::pos();
                    pie->move(globalPos.x() - pie->width() / 2, globalPos.y() - pie->height() / 2);
                    pie->show();
                    return true;
                }

                switch (key->key())
                {
                case Qt::Key_1:
                    setGizmoMode(EditorGizmoTranslate);
                    return true;

                case Qt::Key_2:
                    setGizmoMode(EditorGizmoRotate);
                    return true;

                case Qt::Key_3:
                    setGizmoMode(EditorGizmoScale);
                    return true;

                default:
                    break;
                }

                if (!rmbPressed_)
                {
                    const int navKey = CameraNavigationKey(key->nativeScanCode(), key->key());
                    if (controlScheme_ == ControlScheme::Blender)
                    {
                        if (navKey == 'G' || key->key() == Qt::Key_G) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                        if (navKey == 'R' || key->key() == Qt::Key_R) {
                            setGizmoMode(EditorGizmoRotate);
                            return true;
                        }
                        if (navKey == 'S' || key->key() == Qt::Key_S) {
                            setGizmoMode(EditorGizmoScale);
                            return true;
                        }
                    }
                    else
                    {
                        if (navKey == 'W' || key->key() == Qt::Key_W) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                        if (navKey == 'E' || key->key() == Qt::Key_E) {
                            setGizmoMode(EditorGizmoRotate);
                            return true;
                        }
                        if (navKey == 'R' || key->key() == Qt::Key_R) {
                            setGizmoMode(EditorGizmoScale);
                            return true;
                        }
                        if (navKey == 'Q' || key->key() == Qt::Key_Q) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                    }
                }
            }

            const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());

            if ((navigationKey == Qt::Key_Space || navigationKey == Qt::Key_P) && isActiveWindow())
            {
                if (e->type() == QEvent::KeyPress)
                {
                    EditorCommand c;
                    c.type = navigationKey == Qt::Key_Space ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
                    host_->PostCommand(c);
                }

                return true;
            }

            if (e->type() == QEvent::KeyPress)
                keys_.insert(navigationKey);
            else
                keys_.remove(navigationKey);
        }

        return true;
    }

    return QWidget::event(e);
}

void ViewportWidget::setGizmoMode(EditorGizmoMode mode)
{
    if (gizmoMode_ == mode)
        return;

    if (draggingSelectedObject_)
        return;

    gizmoMode_ = mode;

    host_->SetGizmoMode(mode);

    updateGizmoToolTip();
    emit gizmoModeChanged(mode);
}

void ViewportWidget::updateGizmoToolTip()
{
    const QString modeName = GizmoModeName(gizmoMode_);

    if (controlScheme_ == ControlScheme::Blender)
    {
        setToolTip(
            QStringLiteral("Gizmo: %1 | Keys: G Move, R Rotate, S Scale, 1/2/3\n"
                "Space: Play/Stop | P: Pause | MMB: Orbit | Shift+MMB: Pan | RMB: Look (WASD) | Tab: select | Arrows/PgUp/PgDn: move")
            .arg(modeName));
    }
    else
    {
        setToolTip(
            QStringLiteral("Gizmo: %1 | Keys: W Move, E Rotate, R Scale, 1/2/3\n"
                "Space: Play/Stop | P: Pause | RMB: Look (WASD) | MMB/Shift+MMB: Pan | Tab: select | Arrows/PgUp/PgDn: move")
            .arg(modeName));
    }
}

bool ViewportWidget::SelectedTransform(uint64_t& id, EngineSceneTransform& transform)
{
    const auto view = host_->ReadSnapshot();

    if (!view.selected)
        return false;

    auto found = std::find_if(
        view.objects.begin(),
        view.objects.end(),
        [&](const auto& o)
        {
            return o.id == view.selected;
        });

    if (found == view.objects.end())
        return false;

    id = found->id;
    transform = found->transform;
    return true;
}

bool ViewportWidget::BeginObjectDrag(const QPointF& pos)
{
    if (draggingSelectedObject_)
        return true;

    if (!attached_ || !host_->HasScene())
        return false;

    if (host_->PlayState() != EditorQtHost::Edit)
        return false;

    uint64_t id = 0;
    EngineSceneTransform transform{};

    if (!SelectedTransform(id, transform))
        return false;

    draggingSelectedObject_ = true;
    dragObjectId_ = id;
    dragStartTransform_ = transform;
    dragStartPos_ = pos;
    lastDragPos_ = pos;

    if (gizmoMode_ != EditorGizmoTranslate)
    {
        EditorCommand c;
        c.type = EditorCommand::BeginGizmoDrag;
        c.objectId = id;
        host_->PostCommand(c);

        gizmoDragSession_ = true;
    }

    return true;
}

void ViewportWidget::EndObjectDrag()
{
    if (gizmoDragSession_)
    {
        EditorCommand c;
        c.type = EditorCommand::EndGizmoDrag;
        c.objectId = dragObjectId_;
        host_->PostCommand(c);

        gizmoDragSession_ = false;
    }

    draggingSelectedObject_ = false;
    dragObjectId_ = 0;
    lmbPressed_ = false;
    lmbMoved_ = false;
}

void ViewportWidget::PostSelectedTransform(const EngineSceneTransform& transform)
{
    if (!dragObjectId_)
        return;

    EditorCommand c;
    c.type = EditorCommand::UpdateObjectTransform;
    c.objectId = dragObjectId_;
    c.transform = transform;
    c.path = QStringLiteral("gizmo");

    host_->PostCommand(c);
}

void ViewportWidget::ApplyRotateScaleGizmo(const QPointF& pos)
{
    if (!draggingSelectedObject_)
        return;

    if (gizmoMode_ == EditorGizmoTranslate)
        return;

    const QPointF total = pos - dragStartPos_;

    if (std::fabs(total.x()) < 1e-3 && std::fabs(total.y()) < 1e-3)
        return;

    EngineSceneTransform t = dragStartTransform_;

    if (gizmoMode_ == EditorGizmoRotate)
    {
        const double yaw = total.x() * static_cast<double>(rotateSensitivity_);
        const double pitch = -total.y() * static_cast<double>(rotateSensitivity_);

        double startRot[4] = {
            static_cast<double>(dragStartTransform_.rotation[0]),
            static_cast<double>(dragStartTransform_.rotation[1]),
            static_cast<double>(dragStartTransform_.rotation[2]),
            static_cast<double>(dragStartTransform_.rotation[3])
        };

        NormalizeQuat(startRot);

        double qYaw[4];
        double qPitch[4];
        double qTmp[4];
        double qNew[4];

        AxisAngleQuat(0.0, 1.0, 0.0, yaw, qYaw);
        AxisAngleQuat(1.0, 0.0, 0.0, pitch, qPitch);

        MulQuat(qYaw, qPitch, qTmp);
        MulQuat(qTmp, startRot, qNew);
        NormalizeQuat(qNew);

        using RotT = std::remove_reference_t<decltype(t.rotation[0])>;

        for (int i = 0; i < 4; ++i)
            t.rotation[i] = static_cast<RotT>(qNew[i]);
    }
    else if (gizmoMode_ == EditorGizmoScale)
    {
        double factor = std::exp((total.x() - total.y()) * static_cast<double>(scaleSensitivity_));
        factor = std::max(0.01, std::min(100.0, factor));

        using ScaleT = std::remove_reference_t<decltype(t.scale[0])>;

        for (int i = 0; i < 3; ++i)
        {
            const double base = dragStartTransform_.scale[i];
            t.scale[i] = static_cast<ScaleT>(base * factor);
        }
    }

    PostSelectedTransform(t);
}

std::array<double, 3> ViewportWidget::ComputeSpawnPosition(const QPoint& viewportPos) const
{
    if (EditorSettings::GetSpawnLocationMode() == SpawnLocationMode::WorldOrigin)
    {
        return {0.0, 0.0, 0.0};
    }

    const float w = width() > 0 ? static_cast<float>(width()) : 800.0f;
    const float h = height() > 0 ? static_cast<float>(height()) : 600.0f;

    const float ndcX = 2.0f * static_cast<float>(viewportPos.x()) / w - 1.0f;
    const float ndcY = 1.0f - 2.0f * static_cast<float>(viewportPos.y()) / h;

    const float aspect = w / h;
    const float tanHalfFov = 0.577f;

    const float cy = std::cos(camYaw_);
    const float sy = std::sin(camYaw_);
    const float cp = std::cos(camPitch_);
    const float sp = std::sin(camPitch_);

    const float fwdX = cy * cp;
    const float fwdY = sp;
    const float fwdZ = sy * cp;

    const float rightX = sy;
    const float rightY = 0.0f;
    const float rightZ = -cy;

    const float upX = -fwdY * rightZ;
    const float upY = fwdZ * rightX - fwdX * rightZ;
    const float upZ = fwdY * rightX;

    float rx = fwdX + ndcX * aspect * tanHalfFov * rightX + ndcY * tanHalfFov * upX;
    float ry = fwdY + ndcX * aspect * tanHalfFov * rightY + ndcY * tanHalfFov * upY;
    float rz = fwdZ + ndcX * aspect * tanHalfFov * rightZ + ndcY * tanHalfFov * upZ;

    const float rLen = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rLen > 1e-6f)
    {
        rx /= rLen;
        ry /= rLen;
        rz /= rLen;
    }

    if (ry < -0.01f && camPos_[1] > 0.0f)
    {
        const float t = -camPos_[1] / ry;
        if (t > 0.1f && t < 200.0f)
        {
            return {
                static_cast<double>(camPos_[0] + rx * t),
                0.0,
                static_cast<double>(camPos_[2] + rz * t)
            };
        }
    }

    return {
        static_cast<double>(camPos_[0] + fwdX * 10.0f),
        0.0,
        static_cast<double>(camPos_[2] + fwdZ * 10.0f)
    };
}

void ViewportWidget::ShowAdjustLastOperation(uint64_t id, const QString& name, const EngineSceneTransform& transform)
{
    if (adjustPanel_) {
        adjustPanel_->ShowForObject(id, name, transform);
    }
}

void ViewportWidget::SetAdjustLastOperationObjectId(uint64_t id)
{
    if (adjustPanel_) {
        adjustPanel_->SetObjectId(id);
    }
}
