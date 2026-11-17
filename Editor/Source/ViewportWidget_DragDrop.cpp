#include "ViewportWidget.h"
#include "AssetDrag.h"
#include "DropRules.h"
#include "CommandRegistry.h"
#include "AssetExtensions.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QVariantMap>

using namespace EditorCore;

bool ViewportWidget::DropWorldPosition(const QPointF& logicalPos, float outWorld[3])
{
    if (!RefreshCameraInfo())
        return false;
    const QPointF cameraPos = ToCameraPixels(logicalPos);
    float rayOrigin[3], rayDir[3];
    if (!GizmoGeometry::ScreenToWorldRay(static_cast<float>(cameraPos.x()), static_cast<float>(cameraPos.y()),
                                         lastCameraInfo_, rayOrigin, rayDir))
        return false;

    const float planePoint[3] = {0.0f, 0.0f, 0.0f};
    const float planeNormal[3] = {0.0f, 1.0f, 0.0f};
    if (GizmoGeometry::RayPlaneIntersection(rayOrigin, rayDir, planePoint, planeNormal, outWorld))
        return true;

    // Ray misses the ground plane (looking at the sky): place along the ray at a fixed distance.
    for (int i = 0; i < 3; ++i)
        outWorld[i] = rayOrigin[i] + rayDir[i] * 5.0f;
    return true;
}

void ViewportWidget::dragEnterEvent(QDragEnterEvent* e)
{
    const QMimeData* mime = e->mimeData();
    if (!mime || !AssetDrag::HasAssetMime(*mime) || !host_->HasScene() ||
        host_->PlayState() != EditorQtHost::Edit)
    {
        e->ignore();
        return;
    }
    const AssetDrag::Payload payload = AssetDrag::FromMime(*mime, QString::fromUtf8(host_->host().contentRoot));
    const DropDecision decision = DecideDrop(DragSource::ContentBrowser, DropTarget::Viewport, payload.kind);
    if (!decision.allowed)
    {
        e->ignore();
        return;
    }
    e->setDropAction(Qt::CopyAction);
    e->accept();
}

void ViewportWidget::dragMoveEvent(QDragMoveEvent* e)
{
    const QMimeData* mime = e->mimeData();
    if (!mime || !AssetDrag::HasAssetMime(*mime) || !host_->HasScene() ||
        host_->PlayState() != EditorQtHost::Edit)
    {
        e->ignore();
        return;
    }
    const AssetDrag::Payload payload = AssetDrag::FromMime(*mime, QString::fromUtf8(host_->host().contentRoot));
    const DropDecision decision = DecideDrop(DragSource::ContentBrowser, DropTarget::Viewport, payload.kind);
    if (!decision.allowed)
    {
        e->ignore();
        return;
    }
    e->setDropAction(Qt::CopyAction);
    e->accept();
}

void ViewportWidget::dropEvent(QDropEvent* e)
{
    const QMimeData* mime = e->mimeData();
    if (!mime || !AssetDrag::HasAssetMime(*mime) || !host_->HasScene() ||
        host_->PlayState() != EditorQtHost::Edit)
    {
        e->ignore();
        return;
    }
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    const AssetDrag::Payload payload = AssetDrag::FromMime(*mime, contentRoot);
    const DropDecision decision = DecideDrop(DragSource::ContentBrowser, DropTarget::Viewport, payload.kind);
    if (!decision.allowed || payload.virtualPaths.isEmpty())
    {
        e->ignore();
        return;
    }

    const QString virtualPath = payload.virtualPaths.first();
    QVariantMap params;
    if (decision.action == DropAction::SpawnModel)
    {
        float world[3] = {0.0f, 0.0f, 0.0f};
        if (!DropWorldPosition(e->position(), world))
        {
            e->ignore();
            return;
        }
        params[QStringLiteral("model")] = virtualPath;
        params[QStringLiteral("x")] = world[0];
        params[QStringLiteral("y")] = world[1];
        params[QStringLiteral("z")] = world[2];
    }
    else
    {
        const uint64_t selected = host_->ReadSnapshot().selected;
        if (selected == 0)
        {
            e->ignore();
            return;
        }
        params[QStringLiteral("objectId")] = static_cast<qulonglong>(selected);
        switch (decision.action)
        {
        case DropAction::AssignMaterial: params[QStringLiteral("material")] = virtualPath; break;
        case DropAction::AssignTexture: params[QStringLiteral("texture")] = virtualPath; break;
        case DropAction::AssignScript: params[QStringLiteral("script")] = virtualPath; break;
        case DropAction::AssignAiMachine: params[QStringLiteral("machine")] = virtualPath; break;
        default: e->ignore(); return;
        }
    }

    CommandRegistry::Instance().Execute(decision.commandId, params);
    e->setDropAction(Qt::CopyAction);
    e->accept();
}