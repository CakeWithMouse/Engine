#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"

#include <type_traits>
#include <QPainter>
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

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
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
            if (c.right || c.up || c.forward) host_->PostCommand(c);
        }
    });
    timer->start(16);
    setMinimumSize(320, 200);
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
    }

    if (e->type() == QEvent::MouseMove)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (rmbPressed_ && !rmbMoved_)
        {
            const QPoint delta = mouse->position().toPoint() - rmbPressPos_;

            if (delta.manhattanLength() > 6)
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

        if (mouse->button() == Qt::RightButton)
        {
            looking_ = false;

            // Если не было движения мышью — показываем меню.
            if (rmbPressed_ && !rmbMoved_)
            {
                emit contextMenuRequested(mapToGlobal(mouse->position().toPoint()));
            }

            rmbPressed_ = false;
            rmbMoved_ = false;

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

    setToolTip(
        QStringLiteral("Gizmo: %1 | Keys: 1 Move, 2 Rotate, 3 Scale\n"
            "Space: Play/Stop | P: Pause | RMB: look | WASD: move | Tab: select object | Arrows/PgUp/PgDn: move object")
        .arg(modeName));
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
