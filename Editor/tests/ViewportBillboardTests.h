#pragma once

#include "ViewportOverlayHelper.h"
#include "IEditorHost.h"
#include "SettingsService.h"
#include "GizmoGeometry.h"

#include <QTemporaryDir>
#include <QSettings>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <string>
#include <functional>

namespace ViewportBillboardTests
{
inline void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

// Captures the real billboards produced by ViewportOverlayHelper::UpdateOverlays.
class Host final : public IEditorHost
{
public:
    Snapshot snapshot;
    EngineCameraInfo camera{};
    std::vector<EngineOverlayBillboard> billboards;

    Host()
    {
        camera.position[2] = -10.0f;
        camera.fov = 1.04719755f;
        camera.viewMatrix[0] = camera.viewMatrix[5] = camera.viewMatrix[10] = camera.viewMatrix[15] = 1.0f;
        camera.viewMatrix[14] = 10.0f;
        camera.projMatrix[0] = 1.3f;
        camera.projMatrix[5] = 1.73f;
        camera.projMatrix[10] = camera.projMatrix[11] = 1.0f;
        camera.width = 800;
        camera.height = 600;
    }

    Snapshot ReadSnapshot() override { return snapshot; }
    bool HasScene() const override { return true; }
    bool GetCamera(EngineCameraInfo& info) const override { info = camera; return true; }
    void SetOverlayBillboards(const std::vector<EngineOverlayBillboard>& bb) override { billboards = bb; }
    void SetOverlayLines(const std::vector<EngineOverlayLine>&, float) override {}
    void SetOverlayTriangles(const std::vector<EngineOverlayTriangle>&) override {}
    void SetOverlayIcon(uint32_t, uint32_t, uint32_t, const uint8_t*) override {}

    void PostCommand(const EditorCommand&) override {}
    void Log(const char*) override {}
    bool HasMaterials() const override { return false; }
    bool HasViewport() const override { return true; }
    bool ViewportActive() const override { return true; }
    EditorState PlayState() const override { return Edit; }
    double LastDt() const override { return 0.0; }
    uint64_t UpdateCount() const override { return 0; }
    const EngineModuleHost& host() const override { static EngineModuleHost h{}; return h; }
    const EngineEditSessionApi& scene() const override { static EngineEditSessionApi s{}; return s; }
    void MoveSelectedObject(float, float, float) override {}
    void FetchSceneAndSelectNext() override {}
    bool HasSelection() const override { return snapshot.selected != 0; }
    std::string GetSelectedObjectName() override { return {}; }
    void SetGizmoMode(EditorGizmoMode) override {}
    EditorGizmoMode GizmoMode() const override { return EditorGizmoTranslate; }
    void SetParent(uint64_t, uint64_t) override {}
    void RunOnUi(std::function<void()> action) override { if (action) action(); }
    void SetUiListener(IEditorUiListener*) override {}
};

inline IEditorHost::SceneObject MakeObject(uint64_t id, const char* type, float x, float y, float z)
{
    IEditorHost::SceneObject obj{};
    obj.id = id;
    obj.type = type;
    obj.transform.position[0] = x;
    obj.transform.position[1] = y;
    obj.transform.position[2] = z;
    return obj;
}

inline const EngineOverlayBillboard* Find(const Host& host, uint64_t id)
{
    for (const auto& bb : host.billboards)
        if (bb.objectId == id) return &bb;
    return nullptr;
}

inline void Run()
{
    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    SettingsService::Instance().SetValue(QStringLiteral("view/icons"), true);
    SettingsService::Instance().SetValue(QStringLiteral("view/overlays"), true);

    Host host;
    auto light = MakeObject(1, "Light.Point", 1.0f, 2.0f, 3.0f);
    light.hasPointLight = true;
    light.pointLight.color[0] = light.pointLight.color[1] = light.pointLight.color[2] = 1.0f;
    light.pointLight.radius = 1.0f;
    host.snapshot.objects.push_back(light);
    host.snapshot.objects.push_back(MakeObject(2, "Light.Directional", 4.0f, 5.0f, 6.0f));
    host.snapshot.objects.push_back(MakeObject(3, "Camera", 7.0f, 8.0f, 9.0f));

    uint64_t hash = 0;
    ViewportOverlayHelper::UpdateOverlays(&host, host.camera, true,
        GizmoGeometry::Handle::None, 0, false, hash, GizmoGeometry::Handle::None);

    Check(host.billboards.size() == 3, "one billboard per icon-bearing object");
    const auto* lb = Find(host, 1);
    Check(lb && lb->x == 1.0f && lb->y == 2.0f && lb->z == 3.0f,
        "point light billboard sits exactly at the world source (no +0.3 lift)");
    const auto* db = Find(host, 2);
    Check(db && db->x == 4.0f && db->y == 5.0f && db->z == 6.0f,
        "directional light billboard at world source");
    const auto* cb = Find(host, 3);
    Check(cb && cb->x == 7.0f && cb->y == 8.0f && cb->z == 9.0f,
        "camera billboard at world source");

    float screen[2]{};
    const float src[3] = {1.0f, 2.0f, 3.0f};
    Check(GizmoGeometry::ProjectPointToScreen(src, host.camera.viewMatrix, host.camera.projMatrix,
        host.camera.width, host.camera.height, screen), "source projects to screen");
    const uint64_t hit = ViewportOverlayHelper::HitTestBillboard(&host, host.camera, screen[0], screen[1], 2.0f);
    Check(hit == 1, "clicking the projected source selects the point light");

    std::printf("Viewport billboard position checks: PASS\n");
}
} // namespace ViewportBillboardTests