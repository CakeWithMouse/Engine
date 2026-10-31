#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "GizmoMath.h"
#include "ViewportOverlayHelper.h"
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

    schemeSubId_ = EditorSettings::Instance().SubscribeControlSchemeChanged([this](ControlScheme scheme) {
        if (headerBar_) headerBar_->SetControlScheme(scheme);
    });

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
            if (c.right || c.up || c.forward)
            {
                host_->PostCommand(c);
            }
        }

        hasCameraInfo_ = host_->GetCamera(lastCameraInfo_);
        if (hasCameraInfo_)
        {
            camPos_[0] = lastCameraInfo_.position[0];
            camPos_[1] = lastCameraInfo_.position[1];
            camPos_[2] = lastCameraInfo_.position[2];
            float fx = lastCameraInfo_.viewMatrix[8];
            float fy = lastCameraInfo_.viewMatrix[9];
            float fz = lastCameraInfo_.viewMatrix[10];
            float len = std::sqrt(fx * fx + fy * fy + fz * fz);
            if (len > 1e-4f)
            {
                fx /= len; fy /= len; fz /= len;
                camPitch_ = std::asin(std::clamp(fy, -1.0f, 1.0f));
                camYaw_ = std::atan2(fz, fx);
            }
        }

        ViewportOverlayHelper::UpdateOverlays(
            host_,
            lastCameraInfo_,
            hasCameraInfo_,
            hoveredHandle_,
            static_cast<int>(gizmoMode_),
            localOrientation_,
            overlayHash_
        );
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
    if (attached_)
        SendResize();
}

void ViewportWidget::AttachHeaderBar(ViewportHeaderBar* bar)
{
    headerBar_ = bar;
    if (!headerBar_) return;
    connect(headerBar_->ComboSnapStep(), &QComboBox::currentTextChanged, this, [this](const QString& text) {
        moveSnapStep_ = text.toDouble();
    });
    headerBar_->SetGizmoMode(static_cast<int>(gizmoMode_));
    headerBar_->SetSnapActive(snapEnabled_);
    headerBar_->SetCoordinateSpaceLocal(localOrientation_);
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

void ViewportWidget::setGizmoMode(EditorGizmoMode mode)
{
    if (gizmoMode_ == mode)
        return;

    gizmoMode_ = mode;
    host_->SetGizmoMode(mode);
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

    EditorCommand c;
    c.type = EditorCommand::BeginGizmoDrag;
    c.objectId = dragObjectId_;
    host_->PostCommand(c);
    gizmoDragSession_ = true;

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
    activeAxis_ = GizmoMath::AxisLock::All;
    lmbPressed_ = false;
    lmbMoved_ = false;
}

void ViewportWidget::CancelObjectDrag()
{
    if (!draggingSelectedObject_) return;
    const uint64_t id = dragObjectId_;
    const EngineSceneTransform startT = dragStartTransform_;
    EndObjectDrag();

    if (id)
    {
        EditorCommand c;
        c.type = EditorCommand::UpdateObjectTransform;
        c.objectId = id;
        c.transform = startT;
        c.path = QStringLiteral("gizmo");
        host_->PostCommand(c);
    }
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

    const GizmoMath::GizmoMode mode = (gizmoMode_ == EditorGizmoRotate) ? GizmoMath::GizmoMode::Rotate : GizmoMath::GizmoMode::Scale;
    EngineSceneTransform t = GizmoMath::ApplyRotateScaleGizmo(
        mode,
        dragStartTransform_,
        total,
        activeAxis_,
        snapEnabled_,
        rotateSnapStep_,
        scaleSnapStep_,
        rotateSensitivity_,
        scaleSensitivity_,
        localOrientation_
    );

    PostSelectedTransform(t);
}
