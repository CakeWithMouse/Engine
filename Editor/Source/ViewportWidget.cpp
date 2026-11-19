#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "GizmoMath.h"
#include "GizmoGeometry.h"
#include "ViewportOverlayHelper.h"
#include "ViewportHeaderBar.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "CameraGuide.h"

#include <type_traits>
#include <QPainter>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QGuiApplication>
#include <cmath>
#include <memory>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QCursor>
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
    setMouseTracking(true);
    setAcceptDrops(true);
    updateGizmoToolTip();

    schemeSubId_ = EditorSettings::Instance().SubscribeControlSchemeChanged([this](ControlScheme s) { if (headerBar_) headerBar_->SetControlScheme(s); });
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] { RetranslateUi(); });

    resizeTimer_ = new QTimer(this);
    resizeTimer_->setSingleShot(true);
    connect(resizeTimer_, &QTimer::timeout, this, &ViewportWidget::SendResize);

    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    connect(timer, &QTimer::timeout, this, [this, elapsed]
    {
        const float dt = std::min(0.05f, static_cast<float>(elapsed->restart()) / 1000.0f);
        const int state = host_->PlayState();
        if (inputState_ != state) { inputState_ = state; keys_.clear(); looking_ = rmbPressed_ = false; EndObjectDrag(); }
        // A monitor change can change physical size without a QWidget resize.
        // Keep this independent of focus (including Play and inactive windows).
        if (attached_ && !isResizing_ && (PhysW() != lastSentW_ || PhysH() != lastSentH_))
            RequestResize();
        if (!attached_) return;
        const bool activeInput = hasFocus() && isActiveWindow();
        if (activeInput) SendCursor(mapFromGlobal(QCursor::pos()), true); // refresh after Play/resume, resize and DPI changes
        if (state != EditorQtHost::Edit && host_->HasScene()) return;

        if (activeInput)
        {
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
        }
        RefreshCameraInfo();
        if (hasCameraInfo_)
        {
            for (int i = 0; i < 3; ++i) camPos_[i] = lastCameraInfo_.position[i];
            float fx = lastCameraInfo_.viewMatrix[8], fy = lastCameraInfo_.viewMatrix[9], fz = lastCameraInfo_.viewMatrix[10];
            float len = std::sqrt(fx * fx + fy * fy + fz * fz);
            if (len > 1e-4f) { camPitch_ = std::asin(std::clamp(fy / len, -1.0f, 1.0f)); camYaw_ = std::atan2(fz / len, fx / len); }
        }

        if (!isResizing_)
        {
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
        }
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
    if (resizeTimer_) resizeTimer_->stop();
    isResizing_ = false;
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
    if (!attached_)
    {
        if (host_->HasViewport() && PhysW() > 0 && PhysH() > 0)
            SendAttach();
        return;
    }
    RequestResize();
}

void ViewportWidget::RequestResize()
{
    if (!attached_ || !resizeTimer_ || PhysW() == 0 || PhysH() == 0) return;
    if (PhysW() == lastSentW_ && PhysH() == lastSentH_) return;
    if (draggingSelectedObject_) EndObjectDrag();
    hoveredHandle_ = GizmoGeometry::Handle::None;
    setCursor(Qt::ArrowCursor);
    hasCameraInfo_ = false;
    isResizing_ = true;
    resizeTimer_->start(120);
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
    const uint32_t pw = PhysW(), ph = PhysH();
    if (pw == 0 || ph == 0) return;
    lastSentW_ = pw; lastSentH_ = ph;
    EditorCommand c; c.type = EditorCommand::ViewportAttach; c.hwnd = (void*)winId(); c.w = lastSentW_; c.h = lastSentH_; host_->PostCommand(c); attached_ = true;
}

void ViewportWidget::SendResize()
{
    if (!attached_) return;
    const bool mouseHeld = (QGuiApplication::mouseButtons() & Qt::LeftButton) != 0;
    if (mouseHeld)
    {
        resizeTimer_->start(120);
        return;
    }
    isResizing_ = false;
    const uint32_t pw = PhysW(), ph = PhysH();
    if (pw == 0 || ph == 0) return;
    if (pw == lastSentW_ && ph == lastSentH_) return;
    lastSentW_ = pw; lastSentH_ = ph;
    EditorCommand c; c.type = EditorCommand::ViewportResize; c.w = pw; c.h = ph; host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
uint32_t ViewportWidget::PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

bool ViewportWidget::RefreshCameraInfo()
{
    // The engine consumes resize commands asynchronously. Do not pick with the
    // previous camera while the new framebuffer size is still being applied.
    hasCameraInfo_ = attached_ && !isResizing_ &&
        PhysW() == lastSentW_ && PhysH() == lastSentH_ &&
        host_->GetCamera(lastCameraInfo_) &&
        lastCameraInfo_.width == lastSentW_ && lastCameraInfo_.height == lastSentH_;
    return hasCameraInfo_;
}

QPointF ViewportWidget::ToCameraPixels(const QPointF& logicalPos) const
{
    // Use the actual framebuffer dimensions, including fractional-DPI rounding.
    return QPointF(logicalPos.x() * lastCameraInfo_.width / std::max(width(), 1),
                   logicalPos.y() * lastCameraInfo_.height / std::max(height(), 1));
}

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
    setToolTip(QString());
}

bool ViewportWidget::SelectedTransform(uint64_t& id, EngineSceneTransform& transform)
{
    id = 0;
    const auto view = host_->ReadSnapshot();
    if (!view.selected) return false;
    auto found = std::find_if(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == view.selected; });
    if (found == view.objects.end()) return false;
    id = found->id;
    transform = found->transform;
    return true;
}

bool ViewportWidget::SelectedWorldTransform(uint64_t& id, EngineSceneTransform& local, float worldPos[3], float worldRot[4], glm::mat4* parentWorldOut) const
{
    id = 0;
    if (!attached_ || !host_->HasScene()) return false;
    const auto view = host_->ReadSnapshot();
    const auto found = std::find_if(view.objects.begin(), view.objects.end(), [&](const auto& o) { return o.id == view.selected; });
    if (found == view.objects.end() || found->type == "Scene.Folder") return false;
    id = found->id;
    local = found->transform;

    const glm::mat4 childWorld = CameraGuide::World(*found, view);
    worldPos[0] = childWorld[3].x; worldPos[1] = childWorld[3].y; worldPos[2] = childWorld[3].z;

    glm::mat3 rotM(
        glm::length(glm::vec3(childWorld[0])) > 1e-6f ? glm::normalize(glm::vec3(childWorld[0])) : glm::vec3(1, 0, 0),
        glm::length(glm::vec3(childWorld[1])) > 1e-6f ? glm::normalize(glm::vec3(childWorld[1])) : glm::vec3(0, 1, 0),
        glm::length(glm::vec3(childWorld[2])) > 1e-6f ? glm::normalize(glm::vec3(childWorld[2])) : glm::vec3(0, 0, 1)
    );
    glm::quat wq = glm::quat_cast(rotM);
    worldRot[0] = wq.x; worldRot[1] = wq.y; worldRot[2] = wq.z; worldRot[3] = wq.w;

    if (parentWorldOut)
    {
        if (found->parent)
        {
            for (const auto& p : view.objects)
            {
                if (p.id == found->parent)
                {
                    glm::mat4 pw = CameraGuide::World(p, view);
                    if (!found->inheritScale)
                    {
                        for (int i = 0; i < 3; ++i)
                        {
                            const float l = glm::length(glm::vec3(pw[i]));
                            if (l > 1e-6f) pw[i] /= l;
                        }
                    }
                    *parentWorldOut = pw;
                    return true;
                }
            }
        }
        *parentWorldOut = glm::mat4(1.0f);
    }
    return true;
}

bool ViewportWidget::BeginGizmoDragGesture(const QPointF& logicalPos, GizmoGeometry::Handle handle)
{
    if (draggingSelectedObject_) return true;
    if (!attached_ || !host_->HasScene() || host_->PlayState() != EditorQtHost::Edit) return false;
    if (!SelectedWorldTransform(dragObjectId_, dragStartTransform_, dragStartWorldPos_, dragStartWorldRot_, &dragParentWorld_)) return false;

    draggingSelectedObject_ = true;
    activeGizmoHandle_ = handle;
    dragStartPos_ = logicalPos;
    lastDragPos_ = logicalPos;
    const QPointF pos = ToCameraPixels(logicalPos);

    if (handle == GizmoGeometry::Handle::AxisX || handle == GizmoGeometry::Handle::PlaneYZ) activeAxis_ = GizmoMath::AxisLock::X;
    else if (handle == GizmoGeometry::Handle::AxisY || handle == GizmoGeometry::Handle::PlaneXZ) activeAxis_ = GizmoMath::AxisLock::Y;
    else if (handle == GizmoGeometry::Handle::AxisZ || handle == GizmoGeometry::Handle::PlaneXY) activeAxis_ = GizmoMath::AxisLock::Z;
    else activeAxis_ = GizmoMath::AxisLock::All;

    if (hasCameraInfo_)
    {
        float rayOrig[3], rayDir[3];
        GizmoGeometry::ScreenToWorldRay(static_cast<float>(pos.x()), static_cast<float>(pos.y()), lastCameraInfo_, rayOrig, rayDir);

        float ax[3], ay[3], az[3];
        GizmoGeometry::GetGizmoAxes(dragStartWorldRot_, localOrientation_, ax, ay, az);

        if (gizmoMode_ == EditorGizmoTranslate)
        {
            if (handle == GizmoGeometry::Handle::AxisX || handle == GizmoGeometry::Handle::AxisY || handle == GizmoGeometry::Handle::AxisZ)
            {
                const float* a = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
                GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartWorldPos_, a, dragStartHitPoint_, &dragStartS_);
            }
            else if (handle == GizmoGeometry::Handle::PlaneXY || handle == GizmoGeometry::Handle::PlaneXZ || handle == GizmoGeometry::Handle::PlaneYZ)
            {
                const float* n = (handle == GizmoGeometry::Handle::PlaneXY) ? az : (handle == GizmoGeometry::Handle::PlaneXZ ? ay : ax);
                GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, n, dragStartHitPoint_);
            }
            else
            {
                float camFwd[3] = {lastCameraInfo_.viewMatrix[2], lastCameraInfo_.viewMatrix[6], lastCameraInfo_.viewMatrix[10]};
                GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, camFwd, dragStartHitPoint_);
            }
        }
        else if (gizmoMode_ == EditorGizmoRotate)
        {
            const float* n = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, n, dragStartHitPoint_))
            {
                float vx = dragStartHitPoint_[0] - dragStartWorldPos_[0];
                float vy = dragStartHitPoint_[1] - dragStartWorldPos_[1];
                float vz = dragStartHitPoint_[2] - dragStartWorldPos_[2];
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
                if (GizmoGeometry::ProjectPointToScreen(dragStartWorldPos_, lastCameraInfo_.viewMatrix, lastCameraInfo_.projMatrix, lastCameraInfo_.width, lastCameraInfo_.height, cs))
                    dragStartDist_ = std::hypot(static_cast<float>(pos.x()) - cs[0], static_cast<float>(pos.y()) - cs[1]);
            }
            else
            {
                const float* a = (handle == GizmoGeometry::Handle::AxisX) ? ax : (handle == GizmoGeometry::Handle::AxisY ? ay : az);
                GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartWorldPos_, a, dragStartHitPoint_, &dragStartS_);
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

void ViewportWidget::ApplyGizmoDragGesture(const QPointF& logicalPos)
{
    if (!draggingSelectedObject_ || !dragObjectId_ || !hasCameraInfo_) return;
    const QPointF pos = ToCameraPixels(logicalPos);

    float rayOrig[3], rayDir[3];
    if (!GizmoGeometry::ScreenToWorldRay(static_cast<float>(pos.x()), static_cast<float>(pos.y()), lastCameraInfo_, rayOrig, rayDir))
        return;

    float ax[3], ay[3], az[3];
    GizmoGeometry::GetGizmoAxes(dragStartWorldRot_, localOrientation_, ax, ay, az);

    if (gizmoMode_ == EditorGizmoTranslate)
    {
        float newWorldPos[3] = {dragStartWorldPos_[0], dragStartWorldPos_[1], dragStartWorldPos_[2]};
        if (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX ||
            activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ||
            activeGizmoHandle_ == GizmoGeometry::Handle::AxisZ)
        {
            const float* a = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? ax :
                             (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? ay : az);
            float curPt[3]; float curS = 0.0f;
            if (GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartWorldPos_, a, curPt, &curS))
            {
                float deltaS = curS - dragStartS_;
                if (snapEnabled_ && moveSnapStep_ > 1e-4)
                    deltaS = static_cast<float>(std::round(deltaS / moveSnapStep_) * moveSnapStep_);
                for (int i = 0; i < 3; ++i) newWorldPos[i] += deltaS * a[i];
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
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, n, curPt))
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
                for (int i = 0; i < 3; ++i) newWorldPos[i] += s1 * a1[i] + s2 * a2[i];
            }
        }
        else // Center handle
        {
            float camFwd[3] = {lastCameraInfo_.viewMatrix[2], lastCameraInfo_.viewMatrix[6], lastCameraInfo_.viewMatrix[10]};
            float curPt[3];
            if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, camFwd, curPt))
            {
                float delta[3] = {curPt[0] - dragStartHitPoint_[0], curPt[1] - dragStartHitPoint_[1], curPt[2] - dragStartHitPoint_[2]};
                if (snapEnabled_ && moveSnapStep_ > 1e-4)
                    for (int i = 0; i < 3; ++i) delta[i] = static_cast<float>(std::round(delta[i] / moveSnapStep_) * moveSnapStep_);
                for (int i = 0; i < 3; ++i) newWorldPos[i] += delta[i];
            }
        }

        glm::vec4 localPos4 = glm::inverse(dragParentWorld_) * glm::vec4(newWorldPos[0], newWorldPos[1], newWorldPos[2], 1.0f);
        EngineSceneTransform t = dragStartTransform_;
        t.position[0] = localPos4.x;
        t.position[1] = localPos4.y;
        t.position[2] = localPos4.z;
        PostSelectedTransform(t);
    }
    else if (gizmoMode_ == EditorGizmoRotate)
    {
        const float* rotAxis = (activeGizmoHandle_ == GizmoGeometry::Handle::AxisX) ? ax :
                               (activeGizmoHandle_ == GizmoGeometry::Handle::AxisY ? ay : az);
        float curPt[3];
        if (GizmoGeometry::RayPlaneIntersection(rayOrig, rayDir, dragStartWorldPos_, rotAxis, curPt))
        {
            float angle = GizmoGeometry::ComputeRingAngle(dragStartWorldPos_, rotAxis, dragStartRefVec_, curPt);
            if (snapEnabled_ && rotateSnapStep_ > 1e-4)
            {
                const double stepRad = rotateSnapStep_ * 3.14159265358979323846 / 180.0;
                angle = static_cast<float>(std::round(angle / stepRad) * stepRad);
            }
            double deltaQ[4];
            GizmoMath::AxisAngleQuat(rotAxis[0], rotAxis[1], rotAxis[2], angle, deltaQ);
            glm::quat deltaWorldQ(static_cast<float>(deltaQ[3]), static_cast<float>(deltaQ[0]), static_cast<float>(deltaQ[1]), static_cast<float>(deltaQ[2]));
            glm::quat startWorldQ(dragStartWorldRot_[3], dragStartWorldRot_[0], dragStartWorldRot_[1], dragStartWorldRot_[2]);
            glm::quat newWorldQ = glm::normalize(deltaWorldQ * startWorldQ);

            glm::mat3 parentRotM(
                glm::length(glm::vec3(dragParentWorld_[0])) > 1e-6f ? glm::normalize(glm::vec3(dragParentWorld_[0])) : glm::vec3(1, 0, 0),
                glm::length(glm::vec3(dragParentWorld_[1])) > 1e-6f ? glm::normalize(glm::vec3(dragParentWorld_[1])) : glm::vec3(0, 1, 0),
                glm::length(glm::vec3(dragParentWorld_[2])) > 1e-6f ? glm::normalize(glm::vec3(dragParentWorld_[2])) : glm::vec3(0, 0, 1)
            );
            glm::quat parentQ = glm::quat_cast(parentRotM);
            glm::quat newLocalQ = glm::normalize(glm::inverse(parentQ) * newWorldQ);

            EngineSceneTransform t = dragStartTransform_;
            t.rotation[0] = newLocalQ.x;
            t.rotation[1] = newLocalQ.y;
            t.rotation[2] = newLocalQ.z;
            t.rotation[3] = newLocalQ.w;
            PostSelectedTransform(t);
        }
    }
    else if (gizmoMode_ == EditorGizmoScale)
    {
        if (activeGizmoHandle_ == GizmoGeometry::Handle::Center)
        {
            float cs[2];
            if (GizmoGeometry::ProjectPointToScreen(dragStartWorldPos_, lastCameraInfo_.viewMatrix, lastCameraInfo_.projMatrix, lastCameraInfo_.width, lastCameraInfo_.height, cs))
            {
                float curDist = std::hypot(static_cast<float>(pos.x()) - cs[0], static_cast<float>(pos.y()) - cs[1]);
                const float sDist = (dragStartDist_ > 5.0f) ? dragStartDist_ : 30.0f;
                float ratio = curDist / sDist;
                if (std::abs(ratio) < 0.001f) ratio = (ratio < 0.0f) ? -0.001f : 0.001f;
                if (snapEnabled_ && scaleSnapStep_ > 1e-4)
                {
                    ratio = static_cast<float>(std::round(ratio / scaleSnapStep_) * scaleSnapStep_);
                    if (std::abs(ratio) < 0.001f) ratio = (ratio < 0.0f) ? -0.001f : 0.001f;
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
            if (GizmoGeometry::ClosestPointOnAxis(rayOrig, rayDir, dragStartWorldPos_, a, curPt, &curS))
            {
                const float sDist = (std::abs(dragStartS_) > 0.05f) ? dragStartS_ : 1.0f;
                float ratio = curS / sDist;
                if (std::abs(ratio) < 0.001f) ratio = (ratio < 0.0f) ? -0.001f : 0.001f;
                if (snapEnabled_ && scaleSnapStep_ > 1e-4)
                {
                    ratio = static_cast<float>(std::round(ratio / scaleSnapStep_) * scaleSnapStep_);
                    if (std::abs(ratio) < 0.001f) ratio = (ratio < 0.0f) ? -0.001f : 0.001f;
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
