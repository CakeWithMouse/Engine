#pragma once

#include "ViewportOverlayHelper.h"
#include "IEditorHost.h"
#include "SettingsService.h"
#include "GizmoGeometry.h"
#include "BillboardMetrics.h"
#include "EditorTheme.h"
#include "ViewportOverlay.h"
#include "IconProvider.h"
#include "EditorIconData.h"

#include <QTemporaryDir>
#include <QSettings>
#include <algorithm>
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

inline bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-3f;
}

// Captures the real billboards produced by ViewportOverlayHelper::UpdateOverlays.
class Host final : public IEditorHost
{
public:
    Snapshot snapshot;
    EngineCameraInfo camera{};
    std::vector<EngineOverlayBillboard> billboards;
    std::vector<QImage> icons;

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
    void SetOverlayIcon(uint32_t id, uint32_t width, uint32_t height, const uint8_t* data) override
    {
        if (icons.size() <= id) icons.resize(id + 1);
        icons[id] = QImage(data, width, height, QImage::Format_RGBA8888).copy();
    }

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
    IconProvider::UploadIcons(&host);
    for (auto id : {EditorIcons::IconId::PointLight, EditorIcons::IconId::SunSky})
    {
        const auto& icon = host.icons.at(static_cast<size_t>(id));
        QRect ink;
        for (int y = 0; y < icon.height(); ++y)
            for (int x = 0; x < icon.width(); ++x)
                if (icon.pixelColor(x, y).alpha() >= 128) ink |= QRect(x, y, 1, 1);
        std::printf("Light icon %u alpha bounds: %dx%d of %dx%d\n",
            static_cast<unsigned>(id), ink.width(), ink.height(), icon.width(), icon.height());
        Check(ink.width() >= 38 && ink.height() >= 38, "light glyph occupies at least 59% of the plate");
        Check(ink.left() >= 8 && ink.top() >= 8 && ink.right() < 56 && ink.bottom() < 56,
            "light glyph keeps transparent margins without clipping");
    }
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

    // Size = 2.4% of the physical viewport height, clamped in physical pixels.
    // Physical height = logical height * devicePixelRatio.
    const float logicalHeights[] = {1080.0f, 1440.0f};
    const float dprs[] = {1.0f, 1.5f, 2.0f};
    for (float logical : logicalHeights)
    {
        for (float dpr : dprs)
        {
            const float physical = logical * dpr;
            const float expected = std::clamp(physical * 0.024f, 16.0f, 80.0f);
            Check(Near(BillboardMetrics::SizePixels(physical), expected),
                "billboard size matches 2.4% of physical height with clamp");
        }
    }
    Check(Near(BillboardMetrics::SizePixels(100.0f), 16.0f), "size clamps to minimum on tiny viewport");
    Check(Near(BillboardMetrics::SizePixels(10000.0f), 80.0f), "size clamps to maximum on huge viewport");

    Check(lb && Near(lb->sizePixels, std::clamp(600.0f * 0.024f, 16.0f, 80.0f)),
        "billboard sizePixels derived from physical camera height");

    // A viewport height change must re-upload billboards at the new size (height is hashed).
    host.camera.height = 1080;
    ViewportOverlayHelper::UpdateOverlays(&host, host.camera, true,
        GizmoGeometry::Handle::None, 0, false, hash, GizmoGeometry::Handle::None);
    const auto* resized = Find(host, 1);
    Check(resized && Near(resized->sizePixels, std::clamp(1080.0f * 0.024f, 16.0f, 80.0f)),
        "viewport height change re-uploads billboards at the new size");

    std::printf("Viewport billboard size checks: PASS\n");

    // T4.3: categorical plate tint comes from the ThemePalette, per object kind.
    EditorTheme::Instance().SetMode(ThemeMode::Dark);
    host.snapshot.objects.push_back(MakeObject(4, "Gameplay.Camera", 10.0f, 0.0f, 0.0f));
    host.snapshot.objects.push_back(MakeObject(5, "Particles", 11.0f, 0.0f, 0.0f));
    host.snapshot.objects.push_back(MakeObject(6, "Character", 12.0f, 0.0f, 0.0f));
    hash = 0;
    ViewportOverlayHelper::UpdateOverlays(&host, host.camera, true,
        GizmoGeometry::Handle::None, 0, false, hash, GizmoGeometry::Handle::None);

    const ThemePalette darkPal = EditorTheme::Instance().Palette();
    auto tintOf = [](const QColor& c) {
        return ViewportOverlay::MakeRgba(static_cast<uint8_t>(c.red()),
            static_cast<uint8_t>(c.green()), static_cast<uint8_t>(c.blue()), 255);
    };
    const auto* lbD = Find(host, 1);
    const auto* dbD = Find(host, 2);
    const auto* cbD = Find(host, 3);
    const auto* gbD = Find(host, 4);
    const auto* pbD = Find(host, 5);
    const auto* chD = Find(host, 6);
    Check(lbD && lbD->rgbaTint == tintOf(darkPal.iconPointLight), "point light tint from dark palette");
    Check(dbD && dbD->rgbaTint == tintOf(darkPal.iconSunSky), "sun/sky tint from dark palette");
    Check(cbD && cbD->rgbaTint == tintOf(darkPal.iconCamera), "camera tint from dark palette");
    Check(gbD && gbD->rgbaTint == tintOf(darkPal.iconGameCamera), "game camera tint from dark palette");
    Check(pbD && pbD->rgbaTint == tintOf(darkPal.iconParticles), "particles tint from dark palette");
    Check(chD && chD->rgbaTint == tintOf(darkPal.iconCharacter), "character tint from dark palette");

    // Switching theme must change the tint AND force a re-upload (theme is hashed).
    const uint64_t hashDark = hash;
    EditorTheme::Instance().SetMode(ThemeMode::Light);
    ViewportOverlayHelper::UpdateOverlays(&host, host.camera, true,
        GizmoGeometry::Handle::None, 0, false, hash, GizmoGeometry::Handle::None);
    Check(hash != hashDark, "theme change re-uploads billboards (theme hashed)");
    const ThemePalette lightPal = EditorTheme::Instance().Palette();
    const auto* lbL = Find(host, 1);
    Check(lbL && lbL->rgbaTint == tintOf(lightPal.iconPointLight), "point light tint from light palette");
    Check(lbL && lbL->rgbaTint != tintOf(darkPal.iconPointLight), "theme switch changes the tint");

    EditorTheme::Instance().SetMode(ThemeMode::Dark);
    std::printf("Viewport billboard tint checks: PASS\n");
}
} // namespace ViewportBillboardTests