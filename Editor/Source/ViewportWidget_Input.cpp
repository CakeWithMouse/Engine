#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "GizmoMath.h"
#include "GizmoGeometry.h"
#include "CommandRegistry.h"
#include "EditorSettings.h"
#include "ViewportOverlayHelper.h"

#include <cmath>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <Engine/InputDevice/Keys.h>

namespace {
uint32_t GameKey(QKeyEvent* key)
{
    int logical = CameraNavigationKey(key->nativeScanCode(), key->key());
    if (logical == 'W' || logical == 'A' || logical == 'S' || logical == 'D' || logical == 'Q' || logical == 'E') return logical;
    if (key->nativeVirtualKey() && key->nativeVirtualKey() < 256) return key->nativeVirtualKey();
    if (logical >= 0x20 && logical <= 0x7e) return logical;
    switch (logical) {
    case Qt::Key_Control: return 17; case Qt::Key_Shift: return 16; case Qt::Key_Alt: return 18;
    case Qt::Key_Escape: return 27; case Qt::Key_Tab: return 9; case Qt::Key_Return: return 13;
    case Qt::Key_Left: return 37; case Qt::Key_Up: return 38; case Qt::Key_Right: return 39; case Qt::Key_Down: return 40;
    default: return logical >= Qt::Key_F1 && logical <= Qt::Key_F24 ? 112 + logical - Qt::Key_F1 : 0;
    }
    return 0;
}
} // namespace

bool ViewportWidget::event(QEvent* e)
{
    if (HandleFocusEvent(e))
        return true;

    if (attached_ && host_->HasScene() && host_->PlayState() != EditorQtHost::Edit)
    {
        if (HandleGameInput(e))
            return true;
    }

    if (e->type() == QEvent::ContextMenu)
    {
        e->accept();
        return true;
    }

    switch (e->type())
    {
    case QEvent::MouseButtonPress:
        return HandleMousePress(static_cast<QMouseEvent*>(e));
    case QEvent::MouseMove:
        return HandleMouseMove(static_cast<QMouseEvent*>(e));
    case QEvent::MouseButtonRelease:
        return HandleMouseRelease(static_cast<QMouseEvent*>(e));
    case QEvent::Wheel:
        return HandleWheel(static_cast<QWheelEvent*>(e));
    case QEvent::KeyPress:
        return HandleKeyPress(static_cast<QKeyEvent*>(e));
    case QEvent::KeyRelease:
        return HandleKeyRelease(static_cast<QKeyEvent*>(e));
    default:
        break;
    }

    return QWidget::event(e);
}

bool ViewportWidget::HandleFocusEvent(QEvent* e)
{
    if (e->type() == QEvent::FocusIn || (e->type() == QEvent::WindowActivate && hasFocus()))
    {
        EditorCommand c; c.type = EditorCommand::InputFocus; c.w = 1; host_->PostCommand(c);
        return false;
    }
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate || e->type() == QEvent::Hide)
    {
        EditorCommand c; c.type = EditorCommand::InputFocus; c.w = 0; host_->PostCommand(c);
        looking_ = false;
        rmbPressed_ = false;
        rmbMoved_ = false;
        EndObjectDrag();
        keys_.clear();
        return false;
    }
    return false;
}

bool ViewportWidget::HandleGameInput(QEvent* e)
{
    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && hasFocus())
    {
        auto* key = static_cast<QKeyEvent*>(e);
        if (key->isAutoRepeat()) return true;
        const int nav = CameraNavigationKey(key->nativeScanCode(), key->key());
        if ((nav == Qt::Key_Space || nav == Qt::Key_P) && key->modifiers() == Qt::NoModifier)
        {
            if (e->type() == QEvent::KeyPress)
            {
                EditorCommand c;
                c.type = (nav == Qt::Key_Space) ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
                host_->PostCommand(c);
            }
            return true;
        }
        EditorCommand c;
        c.type = EditorCommand::InputKey;
        c.w = GameKey(key);
        c.h = (e->type() == QEvent::KeyPress);
        host_->PostCommand(c);
        return true;
    }
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);
        if (e->type() == QEvent::MouseButtonPress) { activateWindow(); setFocus(Qt::MouseFocusReason); lastMouse_ = mouse->position(); }
        EditorCommand c;
        c.type = EditorCommand::InputKey;
        c.w = static_cast<uint32_t>(mouse->button() == Qt::RightButton ? Keys::RightButton : mouse->button() == Qt::MiddleButton ? Keys::MiddleButton : Keys::LeftButton);
        c.h = (e->type() == QEvent::MouseButtonPress);
        host_->PostCommand(c);
        return true;
    }
    if (e->type() == QEvent::MouseMove && hasFocus())
    {
        auto* mouse = static_cast<QMouseEvent*>(e);
        auto delta = mouse->position() - lastMouse_;
        lastMouse_ = mouse->position();
        EditorCommand c; c.type = EditorCommand::InputPointer; c.yaw = static_cast<float>(delta.x()); c.pitch = static_cast<float>(delta.y());
        host_->PostCommand(c);
        return true;
    }
    if (e->type() == QEvent::Wheel && hasFocus())
    {
        EditorCommand c; c.type = EditorCommand::InputPointer; c.zoom = static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y()) / 120.f;
        host_->PostCommand(c);
        return true;
    }
    return false;
}

bool ViewportWidget::HandleMousePress(QMouseEvent* mouse)
{
    activateWindow();
    setFocus(Qt::MouseFocusReason);

    if (mouse->button() == Qt::LeftButton && attached_ && host_->HasScene())
    {
        lmbPressed_ = true;
        lmbMoved_ = false;
        draggingSelectedObject_ = false;
        lmbPressPos_ = mouse->position().toPoint();
        lastDragPos_ = mouse->position();

        if (hoveredHandle_ != GizmoGeometry::Handle::None && host_->HasSelection())
        {
            switch (hoveredHandle_)
            {
            case GizmoGeometry::Handle::AxisX: activeAxis_ = GizmoMath::AxisLock::X; break;
            case GizmoGeometry::Handle::AxisY: activeAxis_ = GizmoMath::AxisLock::Y; break;
            case GizmoGeometry::Handle::AxisZ: activeAxis_ = GizmoMath::AxisLock::Z; break;
            case GizmoGeometry::Handle::PlaneXY: activeAxis_ = GizmoMath::AxisLock::Z; break;
            case GizmoGeometry::Handle::PlaneXZ: activeAxis_ = GizmoMath::AxisLock::Y; break;
            case GizmoGeometry::Handle::PlaneYZ: activeAxis_ = GizmoMath::AxisLock::X; break;
            case GizmoGeometry::Handle::Center:
            default:
                activeAxis_ = GizmoMath::AxisLock::All;
                break;
            }
            if (BeginObjectDrag(mouse->position()))
                return true;
        }

        if (hasCameraInfo_)
        {
            uint64_t bbObj = ViewportOverlayHelper::HitTestBillboard(
                host_,
                lastCameraInfo_,
                static_cast<float>(mouse->position().x()),
                static_cast<float>(mouse->position().y()),
                16.0f
            );
            if (bbObj != 0)
            {
                EditorCommand c;
                c.type = EditorCommand::SelectObject;
                c.objectId = bbObj;
                if (mouse->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) c.name = "toggle";
                host_->PostCommand(c);
                return true;
            }
        }

        EditorCommand c;
        c.type = EditorCommand::PickObject;
        if (mouse->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) c.name = "toggle";
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
    return false;
}

bool ViewportWidget::HandleMouseMove(QMouseEvent* mouse)
{
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
        const float yawDelta = static_cast<float>(delta.x()) * 0.005f;
        const float pitchDelta = static_cast<float>(delta.y()) * 0.005f;

        EditorCommand c;
        c.type = EditorCommand::ViewportCamera;
        c.yaw = yawDelta;
        c.pitch = pitchDelta;
        host_->PostCommand(c);
        return true;
    }

    if (!looking_ && !draggingSelectedObject_ && hasCameraInfo_)
    {
        uint64_t selId = 0;
        EngineSceneTransform selTrans{};
        if (SelectedTransform(selId, selTrans))
        {
            GizmoMath::GizmoMode mode = (gizmoMode_ == EditorGizmoTranslate) ? GizmoMath::GizmoMode::Translate :
                                        (gizmoMode_ == EditorGizmoRotate) ? GizmoMath::GizmoMode::Rotate :
                                                                            GizmoMath::GizmoMode::Scale;
            auto handle = GizmoGeometry::PickHandle(
                mode,
                selTrans.position,
                selTrans.rotation,
                localOrientation_,
                static_cast<float>(mouse->position().x()),
                static_cast<float>(mouse->position().y()),
                lastCameraInfo_,
                10.0f
            );

            if (handle != hoveredHandle_)
            {
                hoveredHandle_ = handle;
                setCursor(hoveredHandle_ != GizmoGeometry::Handle::None ? Qt::SizeAllCursor : Qt::ArrowCursor);
                ViewportOverlayHelper::UpdateOverlays(
                    host_,
                    lastCameraInfo_,
                    hasCameraInfo_,
                    hoveredHandle_,
                    static_cast<int>(gizmoMode_),
                    localOrientation_,
                    overlayHash_
                );
            }
        }
        else if (hoveredHandle_ != GizmoGeometry::Handle::None)
        {
            hoveredHandle_ = GizmoGeometry::Handle::None;
            unsetCursor();
        }
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
            const QPointF total = mouse->position() - dragStartPos_;
            lastDragPos_ = mouse->position();

            const auto finalDelta = GizmoMath::ComputeTranslateDelta(
                total,
                height(),
                camYaw_,
                camPitch_,
                camPos_,
                dragStartTransform_.position,
                activeAxis_,
                localOrientation_,
                dragStartTransform_.rotation,
                snapEnabled_,
                moveSnapStep_
            );

            EngineSceneTransform t = dragStartTransform_;
            using PosT = std::remove_reference_t<decltype(t.position[0])>;
            for (int i = 0; i < 3; ++i)
            {
                t.position[i] = static_cast<PosT>(static_cast<double>(dragStartTransform_.position[i]) + finalDelta[i]);
            }
            PostSelectedTransform(t);
        }
        else
        {
            ApplyRotateScaleGizmo(mouse->position());
            lastDragPos_ = mouse->position();
        }
        return true;
    }

    return false;
}

bool ViewportWidget::HandleMouseRelease(QMouseEvent* mouse)
{
    if (mouse->button() == Qt::LeftButton && (lmbPressed_ || draggingSelectedObject_))
    {
        EndObjectDrag();
        hoveredHandle_ = GizmoGeometry::Handle::None;
        unsetCursor();
        return true;
    }
    if (mouse->button() == Qt::RightButton)
    {
        looking_ = false;
        if (rmbPressed_ && !rmbMoved_)
            emit contextMenuRequested(mapToGlobal(mouse->position().toPoint()));
        rmbPressed_ = false;
        rmbMoved_ = false;
        return true;
    }
    return false;
}

bool ViewportWidget::HandleWheel(QWheelEvent* wheel)
{
    if (!attached_ || !hasFocus()) return false;
    const float zoom = static_cast<float>(wheel->angleDelta().y()) / 120.0f;
    EditorCommand c;
    c.type = EditorCommand::ViewportCamera;
    c.zoom = zoom;
    host_->PostCommand(c);
    return true;
}

bool ViewportWidget::HandleKeyPress(QKeyEvent* key)
{
    if ((key->modifiers() & Qt::ControlModifier)) return false;
    if (key->isAutoRepeat()) return true;

    if (key->key() == Qt::Key_Escape)
    {
        if (draggingSelectedObject_)
        {
            CancelObjectDrag();
            return true;
        }
        EditorCommand c; c.type = EditorCommand::SelectObject; host_->PostCommand(c);
        return true;
    }
    if (key->key() == Qt::Key_Tab)
    {
        host_->FetchSceneAndSelectNext();
        return true;
    }

    const int navKey = CameraNavigationKey(key->nativeScanCode(), key->key());
    const ControlScheme scheme = EditorSettings::Instance().GetControlScheme();

    // Hotkeys active when not flying camera with RMB
    if (!looking_)
    {
        if (key->key() == Qt::Key_X)
        {
            activeAxis_ = (activeAxis_ == GizmoMath::AxisLock::X) ? GizmoMath::AxisLock::All : GizmoMath::AxisLock::X;
            return true;
        }
        if (key->key() == Qt::Key_Y)
        {
            activeAxis_ = (activeAxis_ == GizmoMath::AxisLock::Y) ? GizmoMath::AxisLock::All : GizmoMath::AxisLock::Y;
            return true;
        }
        if (key->key() == Qt::Key_Z)
        {
            activeAxis_ = (activeAxis_ == GizmoMath::AxisLock::Z) ? GizmoMath::AxisLock::All : GizmoMath::AxisLock::Z;
            return true;
        }

        if (scheme == ControlScheme::UnrealUnity)
        {
            if (navKey == 'W') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate")); return true; }
            if (navKey == 'E') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate")); return true; }
            if (navKey == 'R') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale")); return true; }
            if (navKey == 'F') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.FocusSelected")); return true; }
        }
        else if (scheme == ControlScheme::Blender)
        {
            if (navKey == 'G') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate")); return true; }
            if (navKey == 'R') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate")); return true; }
            if (navKey == 'S' && key->modifiers() == Qt::NoModifier) { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale")); return true; }
            if (navKey == 'F') { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.FocusSelected")); return true; }
        }

        if (key->key() == Qt::Key_1) { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetTranslate")); return true; }
        if (key->key() == Qt::Key_2) { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetRotate")); return true; }
        if (key->key() == Qt::Key_3) { CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.SetScale")); return true; }

        if (key->key() == Qt::Key_QuoteLeft || key->key() == Qt::Key_AsciiTilde || navKey == 'Q')
        {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
            return true;
        }
        if ((key->modifiers() & Qt::ShiftModifier) && key->key() == Qt::Key_S)
        {
            CommandRegistry::Instance().Execute(QStringLiteral("Gizmo.ToggleSnap"));
            return true;
        }
    }

    if ((navKey == Qt::Key_Space || navKey == Qt::Key_P) && isActiveWindow())
    {
        EditorCommand c;
        c.type = (navKey == Qt::Key_Space) ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
        host_->PostCommand(c);
        return true;
    }

    keys_.insert(navKey);
    return true;
}

bool ViewportWidget::HandleKeyRelease(QKeyEvent* key)
{
    if (key->isAutoRepeat()) return true;
    const int navKey = CameraNavigationKey(key->nativeScanCode(), key->key());
    keys_.remove(navKey);
    return true;
}

