#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "GizmoMath.h"
#include "GizmoGeometry.h"
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

    schemeSubId_ = EditorSettings::Instance().SubscribeControlSchemeChanged([this](ControlScheme s) { if (headerBar_) headerBar_->SetControlScheme(s); });
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] { RetranslateUi(); });

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
            float r = static_cast<float>(keys_.contains(Qt::Key_Right) - keys_.contains(Qt::Key_Left)) * speed;
            float u = static_cast<float>(keys_.contains(Qt::Key_PageUp) - keys_.contains(Qt::Key_PageDown)) * speed;
            float f = static_cast<float>(keys_.contains(Qt::Key_Up) - keys_.contains(Qt::Key_Down)) * speed;
            if (r || u || f) host_->MoveSelectedObject(r, u, f);
        }
        {
            EditorCommand c; c.type = EditorCommand::ViewportCamera;
            c.right = static_cast<float>(keys_.contains(Qt::Key_D) - keys_.contains(Qt::Key_A)) * speed;
            c.up = static_cast<float>(keys_.contains(Qt::Key_E) - keys_.contains(Qt::Key_Q)) * speed;
            c.forward = static_cast<float>(keys_.contains(Qt::Key_W) - keys_.contains(Qt::Key_S)) * speed;
            if (c.right || c.up || c.forward) host_->PostCommand(c);
        }
        hasCameraInfo_ = host_->GetCamera(lastCameraInfo_);
        if (hasCameraInfo_)
        {
            for (int i = 0; i < 3; ++i) camPos_[i] = lastCameraInfo_.position[i];
            float fx = lastCameraInfo_.viewMatrix[8], fy = lastCameraInfo_.viewMatrix[9], fz = lastCameraInfo_.viewMatrix[10];
            float len = std::sqrt(fx * fx + fy * fy + fz * fz);
            if (len > 1e-4f) { camPitch_ = std::asin(std::clamp(fy / len, -1.0f, 1.0f)); camYaw_ = std::atan2(fz / len, fx / len); }
        }

        ViewportOverlayHelper::UpdateOverlays(
            host_,
            lastCameraInfo_,
            hasCameraInfo_,
            hoveredHandle_,
            static_cast<int>(gizmoMode_),
            localOrientation_,
            overlayHash_,
            activeGizmoHandle_
        );
    });
    timer->start(16);
    setMinimumSize(80, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

ViewportWidget::~ViewportWidget()
{
    if (schemeSubId_) { EditorSettings::Instance().UnsubscribeControlSchemeChanged(schemeSubId_); schemeSubId_ = 0; }
    if (langSubId_) { EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_); langSubId_ = 0; }
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
    EditorCommand c; c.type = EditorCommand::ViewportAttach; c.hwnd = (void*)winId(); c.w = PhysW(); c.h = PhysH(); host_->PostCommand(c); attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c; c.type = EditorCommand::ViewportResize; c.w = PhysW(); c.h = PhysH(); host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
uint32_t ViewportWidget::PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

void ViewportWidget::setGizmoMode(EditorGizmoMode mode)
{
    if (gizmoMode_ == mode) return;
    gizmoMode_ = mode; host_->SetGizmoMode(mode); updateGizmoToolTip();
    if (headerBar_) headerBar_->SetGizmoMode(static_cast<int>(mode));
    emit gizmoModeChanged(mode);
}

void ViewportWidget::SetSnapEnabled(bool enabled) { snapEnabled_ = enabled; if (headerBar_) headerBar_->SetSnapActive(enabled); }
void ViewportWidget::ToggleSnap() { SetSnapEnabled(!snapEnabled_); }
void ViewportWidget::SetCoordinateSpaceLocal(bool local) { localOrientation_ = local; if (headerBar_) headerBar_->SetCoordinateSpaceLocal(local); }
void ViewportWidget::ToggleCoordinateSpace() { SetCoordinateSpaceLocal(!localOrientation_); }
void ViewportWidget::RetranslateUi() { updateGizmoToolTip(); if (headerBar_) headerBar_->RetranslateUi(); }

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

bool ViewportWidget::BeginGizmoDragGesture(const QPointF& pos, GizmoGeometry::Handle handle)
{
    if (draggingSelectedObject_) return true;
    if (!attached_ || !host_->HasScene() || host_->PlayState() != EditorQtHost::Edit) return false;
    if (!SelectedTransform(dragObjectId_, dragStartTransform_)) return false;

    draggingSelectedObject_ = true;
    activeGizmoHandle_ = handle;
    dragStartPos_ = pos;
    lastDragPos_ = pos;

    if (handle == GizmoGeometry::Handle::AxisX || handle == GizmoGeometry::Handle::PlaneYZ) activeAxis_ = GizmoMath::AxisLock::X;
    else if (handle == GizmoGeometry::Handle::AxisY || handle == GizmoGeometry::Handle::PlaneXZ) activeAxis_ = GizmoMath::AxisLock::Y;
    else if (handle == GizmoGeometry::Handle::AxisZ || handle == GizmoGeometry::Handle::PlaneXY) activeAxis_ = GizmoMath::AxisLock::Z;
    else activeAxis_ = GizmoMath::AxisLock::All;

    if (hasCameraInfo_)
    {
        float rayOrig[3], rayDir[3];
        GizmoGeometry::ScreenToWorldRay(static_cast<float>(pos.x()), static_cast<float>(pos.y()), lastCameraInfo_, rayOrig, rayDir);

        float ax[3], ay[3], az[3];
        GizmoGeometry::GetGizmoAxes(dragStartTransform_.rotation, localOrientation_, ax, ay, az);

        if (gizmoMode_ == EditorGizmoTranslate)
        {
            if (handle == GizmoGeometry::Handle::AxisX || handle == GizmoGeometry::Handle::AxisY || handle == GizmoGeometry::Handle::AxisZ)
            {
                const float* a = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
                GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartTransform_.position, a, dragStartHitPoint_, &dragStartS_);
            }
            else if (handle == GizmoGeometry::Handle::PlaneXY || handle == GizmoGeometry::Handle::PlaneXZ || handle == GizmoGeometry::Handle::PlaneYZ)
            {
                const float* n = (handle == GizmoGeometry::Handle::PlaneXY) ? az : (handle == GizmoGeometry::Handle::PlaneXZ ? ay : ax);
                GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, n, dragStartHitPoint_);
            }
            else
            {
                float camFwd[3] = {lastCameraInfo_.viewMatrix[2], lastCameraInfo_.viewMatrix[6], lastCameraInfo_.viewMatrix[10]};
                GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, camFwd, dragStartHitPoint_);
            }
        }
        else if (gizmoMode_ == EditorGizmoRotate)
        {
            const float* n = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, n, dragStartHitPoint_))
            {
                float vx = dragStartHitPoint_[0] - dragStartTransform_.position[0];
                float vy = dragStartHitPoint_[1] - dragStartTransform_.position[1];
                float vz = dragStartHitPoint_[2] - dragStartTransform_.position[2];
                float len = std::sqrt(vx * vx + vy * vy + vz * vz);
                if (len > 1e-4f) { dragStartRefVec_[0] = vx / len; dragStartRefVec_[1] = vy / len; dragStartRefVec_[2] = vz / len; }
                else { dragStartRefVec_[0] = 1.0f; dragStartRefVec_[1] = 0.0f; dragStartRefVec_[2] = 0.0f; }
            }
        }
        else if (gizmoMode_ == EditorGizmoScale)
        {
            if (handle == GizmoGeometry::Handle::Center)
            {
                float cs[2];
                if (GizmoGeometry::ProjectPointToScreen(dragStartTransform_.position, lastCameraInfo_.viewMatrix, lastCameraInfo_.projMatrix, lastCameraInfo_.width, lastCameraInfo_.height, cs))
                    dragStartDist_ = std::hypot(static_cast<float>(pos.x()) - cs[0], static_cast<float>(pos.y()) - cs[1]);
            }
            else
            {
                const float* a = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
                GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartTransform_.position, a, dragStartHitPoint_, &dragStartS_);
            }
        }
    }

    EditorCommand c;
    c.type = EditorCommand::BeginGizmoDrag;
    c.objectId = dragObjectId_;
    host_->PostCommand(c);
    gizmoDragSession_ = true;

    setCursor((handle == GizmoGeometry::Handle::PlaneXY || handle == GizmoGeometry::Handle::PlaneXZ ||
               handle == GizmoGeometry::Handle::PlaneYZ || handle == GizmoGeometry::Handle::Center)
              ? Qt::SizeAllCursor : Qt::PointingHandCursor);

    ViewportOverlayHelper::UpdateOverlays(
        host_,
        lastCameraInfo_,
        hasCameraInfo_,
        hoveredHandle_,
        static_cast<int>(gizmoMode_),
        localOrientation_,
        overlayHash_,
        activeGizmoHandle_
    );

    return true;
}

void ViewportWidget::ApplyGizmoDragGesture(const QPointF& pos)
{
    if (!draggingSelectedObject_ || !dragObjectId_ || !hasCameraInfo_) return;

    float rayOrig[3], rayDir[3];
    if (!GizmoGeometry::ScreenToWorldRay(static_cast<float>(pos.x()), static_cast<float>(pos.y()), lastCameraInfo_, rayOrig, rayDir))
        return;

    float ax[3], ay[3], az[3];
    GizmoGeometry::GetGizmoAxes(dragStartTransform_.rotation, localOrientation_, ax, ay, az);

    if (gizmoMode_ == EditorGizmoTranslate)
    {
        if (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX ||
            activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ||
            activeGizmoHandle_ == GizmoGeometry::Handle::AxisZ)
        {
            const float* a = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? ax :
                             (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? ay : az);
            float curPt[3]; float curS = 0.0f;
            if (GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartTransform_.position, a, curPt, &curS))
            {
                float deltaS = curS - dragStartS_;
                if (snapEnabled_ && moveSnapStep_ > 1e-4)
                    deltaS = static_cast<float>(std::round(deltaS / moveSnapStep_) * moveSnapStep_);

                EngineSceneTransform t = dragStartTransform_;
                for (int i = 0; i < 3; ++i) t.position[i] += deltaS * a[i];
                PostSelectedTransform(t);
            }
        }
        else if (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXY ||
                 activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXZ ||
                 activeGizmoHandle_ == GizmoGeometry::Handle::PlaneYZ)
        {
            const float* n = (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXY) ? az :
                             (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXZ ? ay : ax);
            const float* a1 = (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXY) ? ax :
                              (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXZ ? ax : ay);
            const float* a2 = (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXY) ? ay :
                              (activeGizmoHandle_ == GizmoGeometry::Handle::PlaneXZ ? az : az);
            float curPt[3];
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, n, curPt))
            {
                const float dx = curPt[0] - dragStartHitPoint_[0];
                const float dy = curPt[1] - dragStartHitPoint_[1];
                const float dz = curPt[2] - dragStartHitPoint_[2];
                float s1 = dx * a1[0] + dy * a1[1] + dz * a1[2];
                float s2 = dx * a2[0] + dy * a2[1] + dz * a2[2];
                if (snapEnabled_ && moveSnapStep_ > 1e-4)
                {
                    s1 = static_cast<float>(std::round(s1 / moveSnapStep_) * moveSnapStep_);
                    s2 = static_cast<float>(std::round(s2 / moveSnapStep_) * moveSnapStep_);
                }
                EngineSceneTransform t = dragStartTransform_;
                for (int i = 0; i < 3; ++i) t.position[i] += s1 * a1[i] + s2 * a2[i];
                PostSelectedTransform(t);
            }
        }
        else // Center handle
        {
            float camFwd[3] = {lastCameraInfo_.viewMatrix[2], lastCameraInfo_.viewMatrix[6], lastCameraInfo_.viewMatrix[10]};
            float curPt[3];
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, camFwd, curPt))
            {
                EngineSceneTransform t = dragStartTransform_;
                for (int i = 0; i < 3; ++i) t.position[i] += (curPt[i] - dragStartHitPoint_[i]);
                PostSelectedTransform(t);
            }
        }
    }
    else if (gizmoMode_ == EditorGizmoRotate)
    {
        const float* rotAxis = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? ax :
                               (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? ay : az);
        float curPt[3];
        if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartTransform_.position, rotAxis, curPt))
        {
            float angle = GizmoGeometry::ComputeRingAngle(dragStartTransform_.position, rotAxis, dragStartRefVec_, curPt);
            if (snapEnabled_ && rotateSnapStep_ > 1e-4)
            {
                const double stepRad = rotateSnapStep_ * 3.14159265358979323846 / 180.0;
                angle = static_cast<float>(std::round(angle / stepRad) * stepRad);
            }
            double deltaQ[4];
            GizmoMath::AxisAngleQuat(rotAxis[0], rotAxis[1], rotAxis[2], angle, deltaQ);
            double startQ[4] = {dragStartTransform_.rotation[0], dragStartTransform_.rotation[1], dragStartTransform_.rotation[2], dragStartTransform_.rotation[3]};
            double resQ[4];
            GizmoMath::MulQuat(deltaQ, startQ, resQ);
            GizmoMath::NormalizeQuat(resQ);

            EngineSceneTransform t = dragStartTransform_;
            for (int i = 0; i < 4; ++i) t.rotation[i] = static_cast<float>(resQ[i]);
            PostSelectedTransform(t);
        }
    }
    else if (gizmoMode_ == EditorGizmoScale)
    {
        if (activeGizmoHandle_ == GizmoGeometry::Handle::Center)
        {
            float cs[2];
            if (GizmoGeometry::ProjectPointToScreen(dragStartTransform_.position, lastCameraInfo_.viewMatrix, lastCameraInfo_.projMatrix, lastCameraInfo_.width, lastCameraInfo_.height, cs))
            {
                float curDist = std::hypot(static_cast<float>(pos.x()) - cs[0], static_cast<float>(pos.y()) - cs[1]);
                const float sDist = (dragStartDist_ > 5.0f) ? dragStartDist_ : 30.0f;
                float ratio = curDist / sDist;
                if (ratio < 0.01f) ratio = 0.01f;
                if (snapEnabled_ && scaleSnapStep_ > 1e-4)
                {
                    ratio = static_cast<float>(std::round(ratio / scaleSnapStep_) * scaleSnapStep_);
                    if (ratio < 0.01f) ratio = 0.01f;
                }
                EngineSceneTransform t = dragStartTransform_;
                for (int i = 0; i < 3; ++i) t.scale[i] = dragStartTransform_.scale[i] * ratio;
                PostSelectedTransform(t);
            }
        }
        else
        {
            const float* a = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? ax :
                             (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? ay : az);
            float curPt[3]; float curS = 0.0f;
            if (GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartTransform_.position, a, curPt, &curS))
            {
                const float sDist = (std::abs(dragStartS_) > 0.05f) ? dragStartS_ : 1.0f;
                float ratio = curS / sDist;
                if (ratio < 0.01f) ratio = 0.01f;
                if (snapEnabled_ && scaleSnapStep_ > 1e-4)
                {
                    ratio = static_cast<float>(std::round(ratio / scaleSnapStep_) * scaleSnapStep_);
                    if (ratio < 0.01f) ratio = 0.01f;
                }
                EngineSceneTransform t = dragStartTransform_;
                int idx = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? 0 :
                          (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? 1 : 2);
                t.scale[idx] = dragStartTransform_.scale[idx] * ratio;
                PostSelectedTransform(t);
            }
        }
    }
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
    activeGizmoHandle_ = GizmoGeometry::Handle::None;
    lmbPressed_ = false;
    lmbMoved_ = false;

    ViewportOverlayHelper::UpdateOverlays(
        host_,
        lastCameraInfo_,
        hasCameraInfo_,
        hoveredHandle_,
        static_cast<int>(gizmoMode_),
        localOrientation_,
        overlayHash_,
        GizmoGeometry::Handle::None
    );
}

void ViewportWidget::CancelObjectDrag()
{
    if (!draggingSelectedObject_) return;
    const uint64_t id = dragObjectId_;
    const EngineSceneTransform startT = dragStartTransform_;
    EndObjectDrag();
    if (id) {
        EditorCommand c; c.type = EditorCommand::UpdateObjectTransform; c.objectId = id; c.transform = startT; c.path = QStringLiteral("gizmo"); host_->PostCommand(c);
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
