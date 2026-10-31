#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "GizmoMath.h"
#include "ViewportHeaderBar.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"

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
}
}

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
    if (host_->HasViewport())
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    setFocusPolicy(Qt::StrongFocus);
    updateGizmoToolTip();

    headerBar_ = new ViewportHeaderBar(this);
    headerBar_->move(8, 8);
    connect(headerBar_->ComboSnapStep(), &QComboBox::currentTextChanged, this, [this](const QString& text) {
        moveSnapStep_ = text.toDouble();
    });

    schemeSubId_ = EditorSettings::Instance().SubscribeControlSchemeChanged([this](ControlScheme scheme) {
        if (headerBar_) headerBar_->SetControlScheme(scheme);
    });
    headerBar_->SetControlScheme(EditorSettings::Instance().GetControlScheme());

    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });

    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    connect(timer, &QTimer::timeout, this, [this, elapsed]
    {
        const float dt = std::min(0.05f, static_cast<float>(elapsed->restart()) / 1000.0f);
        const int state = host_->PlayState();
        if (inputState_ != state) { inputState_ = state; keys_.clear(); looking_ = rmbPressed_ = false; EndObjectDrag(); }
        if (!attached_ || !hasFocus() || !isActiveWindow()) return;
        if (state != EditorQtHost::Edit && host_->HasScene()) return;

        const float speed = dt * (keys_.contains(Qt::Key_Shift) ? 4.0f : 1.0f);

        if (host_->HasSelection() && !looking_)
        {
            float right = static_cast<float>(keys_.contains(Qt::Key_Right) - keys_.contains(Qt::Key_Left)) * speed;
            float up = static_cast<float>(keys_.contains(Qt::Key_PageUp) - keys_.contains(Qt::Key_PageDown)) * speed;
            float forward = static_cast<float>(keys_.contains(Qt::Key_Up) - keys_.contains(Qt::Key_Down)) * speed;

            if (right != 0.0f || up != 0.0f || forward != 0.0f)
                host_->MoveSelectedObject(right, up, forward);
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
    setMinimumSize(80, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

ViewportWidget::~ViewportWidget()
{
    if (schemeSubId_)
    {
        EditorSettings::Instance().UnsubscribeControlSchemeChanged(schemeSubId_);
        schemeSubId_ = 0;
    }
    if (langSubId_)
    {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

QPaintEngine* ViewportWidget::paintEngine() const
{
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
    if (headerBar_)
        headerBar_->move(8, 8);
    if (attached_)
        SendResize();
}

void ViewportWidget::paintEvent(QPaintEvent*)
{
    if (host_->HasViewport()) return;
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setPen(QColor(200, 200, 200));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No scene preview is available for this project."));
}

void ViewportWidget::SendAttach()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportAttach;
    c.hwnd = (void*)winId();
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

            double rawDelta[3] = {0.0, 0.0, 0.0};
            if (mouse->modifiers() & Qt::ShiftModifier)
                rawDelta[2] = static_cast<double>(-delta.y()) * dragSensitivity_;
            else
            {
                rawDelta[0] = static_cast<double>(delta.x()) * dragSensitivity_;
                rawDelta[1] = static_cast<double>(-delta.y()) * dragSensitivity_;
            }

            if (snapEnabled_)
                GizmoMath::SnapVector(rawDelta, moveSnapStep_, rawDelta);

            double finalDelta[3] = {rawDelta[0], rawDelta[1], rawDelta[2]};
            if (localOrientation_)
            {
                const double objRot[4] = {
                    dragStartTransform_.rotation[0], dragStartTransform_.rotation[1],
                    dragStartTransform_.rotation[2], dragStartTransform_.rotation[3]
                };
                GizmoMath::TransformDelta(rawDelta, objRot, true, finalDelta);
            }

            if (finalDelta[0] != 0.0 || finalDelta[1] != 0.0 || finalDelta[2] != 0.0)
                host_->MoveSelectedObject(static_cast<float>(finalDelta[0]), static_cast<float>(finalDelta[1]), static_cast<float>(finalDelta[2]));
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
    EditorCommand c;
    c.type = EditorCommand::ViewportCamera;
    c.zoom = static_cast<float>(wheel->angleDelta().y()) / 120.0f;
    host_->PostCommand(c);
    return true;
}

bool ViewportWidget::HandleKeyPress(QKeyEvent* key)
{
    if ((key->modifiers() & Qt::ControlModifier)) return false;
    if (key->isAutoRepeat()) return true;

    if (key->key() == Qt::Key_Escape)
    {
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

void ViewportWidget::setGizmoMode(EditorGizmoMode mode)
{
    if (gizmoMode_ == mode)
        return;

    gizmoMode_ = mode;
    updateGizmoToolTip();
    if (headerBar_)
        headerBar_->SetGizmoMode(static_cast<int>(mode));
    emit gizmoModeChanged(mode);
}

void ViewportWidget::SetSnapEnabled(bool enabled)
{
    snapEnabled_ = enabled;
    if (headerBar_)
        headerBar_->SetSnapActive(enabled);
}

void ViewportWidget::ToggleSnap()
{
    SetSnapEnabled(!snapEnabled_);
}

void ViewportWidget::SetCoordinateSpaceLocal(bool local)
{
    localOrientation_ = local;
    if (headerBar_)
        headerBar_->SetCoordinateSpaceLocal(local);
}

void ViewportWidget::ToggleCoordinateSpace()
{
    SetCoordinateSpaceLocal(!localOrientation_);
}

void ViewportWidget::RetranslateUi()
{
    updateGizmoToolTip();
    if (headerBar_)
        headerBar_->RetranslateUi();
}

void ViewportWidget::updateGizmoToolTip()
{
    const QString modeStr = (gizmoMode_ == EditorGizmoTranslate ? QStringLiteral("Move (W/G)") :
                            (gizmoMode_ == EditorGizmoRotate ? QStringLiteral("Rotate (E/R)") : QStringLiteral("Scale (R/S)")));
    setToolTip(Tr("Режим трансформации: %1\nLMB — тянуть по осям, Shift — по Z\nЛКМ: выбор, ПКМ: вращение камеры, Колёсико: зум",
                  "Gizmo mode: %1\nLMB — drag axes, Shift — along Z\nLMB: pick, RMB: camera look, Wheel: zoom").arg(modeStr));
}

bool ViewportWidget::SelectedTransform(uint64_t& id, EngineSceneTransform& transform)
{
    id = 0;
    const auto view = host_->ReadSnapshot();
    if (!view.selected)
        return false;

    auto found = std::find_if(
        view.objects.begin(),
        view.objects.end(),
        [&](const auto& o) { return o.id == view.selected; }
    );

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

    if (!SelectedTransform(dragObjectId_, dragStartTransform_))
        return false;

    draggingSelectedObject_ = true;
    dragStartPos_ = pos;
    lastDragPos_ = pos;

    if (gizmoMode_ != EditorGizmoTranslate)
    {
        EditorCommand c;
        c.type = EditorCommand::BeginGizmoDrag;
        c.objectId = dragObjectId_;
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
    if (!dragObjectId_) return;
    EditorCommand c;
    c.type = EditorCommand::UpdateObjectTransform;
    c.objectId = dragObjectId_;
    c.transform = transform;
    c.path = QStringLiteral("gizmo");
    host_->PostCommand(c);
}

void ViewportWidget::ApplyRotateScaleGizmo(const QPointF& pos)
{
    if (!draggingSelectedObject_ || gizmoMode_ == EditorGizmoTranslate)
        return;

    const QPointF total = pos - dragStartPos_;
    if (std::fabs(total.x()) < 1e-3 && std::fabs(total.y()) < 1e-3)
        return;

    EngineSceneTransform t = dragStartTransform_;

    if (gizmoMode_ == EditorGizmoRotate)
    {
        const double startRot[4] = {
            dragStartTransform_.rotation[0], dragStartTransform_.rotation[1],
            dragStartTransform_.rotation[2], dragStartTransform_.rotation[3]
        };
        double newRot[4]{};
        GizmoMath::ApplyRotation(startRot, total.x(), total.y(), rotateSensitivity_, snapEnabled_, rotateSnapStep_, newRot);
        using RotT = std::remove_reference_t<decltype(t.rotation[0])>;
        for (int i = 0; i < 4; ++i)
            t.rotation[i] = static_cast<RotT>(newRot[i]);
    }
    else if (gizmoMode_ == EditorGizmoScale)
    {
        const double startScale[3] = {
            dragStartTransform_.scale[0], dragStartTransform_.scale[1], dragStartTransform_.scale[2]
        };
        double newScale[3]{};
        GizmoMath::ApplyScale(startScale, total.x(), total.y(), scaleSensitivity_, snapEnabled_, scaleSnapStep_, newScale);
        using ScaleT = std::remove_reference_t<decltype(t.scale[0])>;
        for (int i = 0; i < 3; ++i)
            t.scale[i] = static_cast<ScaleT>(newScale[i]);
    }

    PostSelectedTransform(t);
}
