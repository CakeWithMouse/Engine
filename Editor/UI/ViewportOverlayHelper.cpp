#include "ViewportOverlayHelper.h"
#include "IconProvider.h"
#include "EditorIconData.h"
#include "ViewportOverlay.h"
#include "IEditorHost.h"
#include "SettingsService.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

namespace
{
    inline uint32_t GetBillboardIcon(const IEditorHost::SceneObject& obj, uint32_t& outColor)
    {
        if (obj.hasPointLight)
        {
            uint8_t r = static_cast<uint8_t>(std::clamp(obj.pointLight.color[0] * 255.0f, 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(obj.pointLight.color[1] * 255.0f, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(obj.pointLight.color[2] * 255.0f, 0.0f, 255.0f));
            outColor = ViewportOverlay::MakeRgba(r, g, b, 255);
            return static_cast<uint32_t>(EditorIcons::IconId::PointLight);
        }

        if (obj.type == "Light.Directional" || obj.type == "Environment.Sky" || obj.name.find("Sky") != std::string::npos || obj.name.find("Sun") != std::string::npos)
        {
            outColor = 0xFF30D0FF;
            return static_cast<uint32_t>(EditorIcons::IconId::SunSky);
        }

        if (obj.gameplayKind != 2 && (obj.type == "Camera" || obj.name.find("Camera") != std::string::npos))
        {
            outColor = 0xFFE0E0E0;
            return static_cast<uint32_t>(EditorIcons::IconId::Camera);
        }

        if (obj.gameplayKind == 2 || obj.type == "Gameplay.Camera" || obj.type == "GameCamera")
        {
            outColor = 0xFF80E080;
            return static_cast<uint32_t>(EditorIcons::IconId::GameCamera);
        }

        if (obj.type.rfind("Particle.", 0) == 0 || obj.type == "Particles" || obj.name.find("Particle") != std::string::npos)
        {
            outColor = 0xFFE080FF;
            return static_cast<uint32_t>(EditorIcons::IconId::Particles);
        }

        if (obj.gameplayKind == 1 || obj.type == "Character" || obj.type == "Player" || obj.name.find("Player") != std::string::npos)
        {
            outColor = 0xFF50E050;
            return static_cast<uint32_t>(EditorIcons::IconId::Character);
        }

        return 0;
    }
}

void ViewportOverlayHelper::UpdateOverlays(
    IEditorHost* host,
    const EngineCameraInfo& cam,
    bool hasCamera,
    GizmoGeometry::Handle hoveredHandle,
    int gizmoMode,
    bool localSpace,
    uint64_t& inOutHash)
{
    if (!host) return;

    IconProvider::UploadIcons(host);

    const bool gridEnabled = SettingsService::Instance().GetValue(QStringLiteral("view/grid"), true).toBool();
    const bool overlaysEnabled = SettingsService::Instance().GetValue(QStringLiteral("view/overlays"), true).toBool();

    const auto snapshot = host->ReadSnapshot();

    const IEditorHost::SceneObject* selectedObj = nullptr;
    for (const auto& obj : snapshot.objects)
    {
        if (obj.id == snapshot.selected)
        {
            selectedObj = &obj;
            break;
        }
    }

    const float* objPos = selectedObj ? selectedObj->transform.position : nullptr;
    const float* objRot = selectedObj ? selectedObj->transform.rotation : nullptr;
    float lightRad = (selectedObj && selectedObj->hasPointLight) ? selectedObj->pointLight.radius : 0.0f;

    const uint64_t hash = ViewportOverlay::ComputeOverlayHash(
        hasCamera ? cam.position : nullptr,
        hasCamera ? cam.viewMatrix : nullptr,
        gridEnabled,
        overlaysEnabled,
        snapshot.selected,
        static_cast<uint32_t>(gizmoMode),
        objPos,
        objRot,
        static_cast<int>(hoveredHandle),
        0,
        lightRad
    );

    if (hash == inOutHash)
        return;

    inOutHash = hash;

    std::vector<EngineOverlayLine> lines;
    std::vector<EngineOverlayBillboard> billboards;

    if (gridEnabled)
    {
        ViewportOverlay::Palette pal;
        float camX = hasCamera ? cam.position[0] : 0.0f;
        float camZ = hasCamera ? cam.position[2] : 0.0f;
        auto gridLines = ViewportOverlay::GenerateGridLines(camX, camZ, pal);
        lines.insert(lines.end(), gridLines.begin(), gridLines.end());
    }

    if (overlaysEnabled)
    {
        for (const auto& obj : snapshot.objects)
        {
            uint32_t iconColor = 0xFFFFFFFF;
            uint32_t iconId = GetBillboardIcon(obj, iconColor);
            if (iconId != 0)
            {
                EngineOverlayBillboard bb{};
                bb.iconId = iconId;
                bb.x = obj.transform.position[0];
                bb.y = obj.transform.position[1] + (obj.hasPointLight ? 0.3f : 0.0f);
                bb.z = obj.transform.position[2];
                bb.sizePixels = 32.0f;
                bb.rgbaTint = iconColor;
                bb.objectId = obj.id;
                billboards.push_back(bb);
            }
        }

        if (selectedObj && hasCamera)
        {
            if (selectedObj->hasPointLight && selectedObj->pointLight.radius > 0.01f)
            {
                uint32_t lightCol = ViewportOverlay::MakeRgba(
                    static_cast<uint8_t>(std::clamp(selectedObj->pointLight.color[0] * 255.0f, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(selectedObj->pointLight.color[1] * 255.0f, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(selectedObj->pointLight.color[2] * 255.0f, 0.0f, 255.0f)),
                    160
                );
                auto radiusLines = ViewportOverlay::GenerateLightRadiusLines(
                    selectedObj->transform.position[0],
                    selectedObj->transform.position[1],
                    selectedObj->transform.position[2],
                    selectedObj->pointLight.radius,
                    lightCol,
                    48
                );
                lines.insert(lines.end(), radiusLines.begin(), radiusLines.end());
            }

            GizmoMath::GizmoMode mode = (gizmoMode == 0) ? GizmoMath::GizmoMode::Translate :
                                        (gizmoMode == 1) ? GizmoMath::GizmoMode::Rotate :
                                                           GizmoMath::GizmoMode::Scale;
            auto gizmoLines = GizmoGeometry::BuildGizmoLines(
                mode,
                selectedObj->transform.position,
                selectedObj->transform.rotation,
                localSpace,
                cam,
                hoveredHandle,
                GizmoGeometry::Handle::None
            );
            lines.insert(lines.end(), gizmoLines.begin(), gizmoLines.end());
        }
    }

    host->SetOverlayLines(lines, 1.0f);
    host->SetOverlayBillboards(billboards);
}

uint64_t ViewportOverlayHelper::HitTestBillboard(
    IEditorHost* host,
    const EngineCameraInfo& cam,
    float screenX,
    float screenY,
    float tolerancePixels)
{
    if (!host || cam.width <= 0 || cam.height <= 0) return 0;

    const bool overlaysEnabled = SettingsService::Instance().GetValue(QStringLiteral("view/overlays"), true).toBool();
    if (!overlaysEnabled) return 0;

    const auto snapshot = host->ReadSnapshot();
    float bestDist = tolerancePixels;
    uint64_t bestId = 0;

    for (const auto& obj : snapshot.objects)
    {
        uint32_t col = 0;
        if (GetBillboardIcon(obj, col) == 0)
            continue;

        float pos[3] = {
            obj.transform.position[0],
            obj.transform.position[1] + (obj.hasPointLight ? 0.3f : 0.0f),
            obj.transform.position[2]
        };

        float screen[2]{};
        if (GizmoGeometry::ProjectPointToScreen(pos, cam.viewMatrix, cam.projMatrix, cam.width, cam.height, screen))
        {
            float dx = screen[0] - screenX;
            float dy = screen[1] - screenY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestId = obj.id;
            }
        }
    }

    return bestId;
}
