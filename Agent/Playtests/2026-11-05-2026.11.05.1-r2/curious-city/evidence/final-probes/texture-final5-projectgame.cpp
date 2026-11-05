#include "ProjectGame.h"
#include "RotatingCube.h"
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Scene/SceneAssets.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
class CityRenderProbe final : public CubeComponent
{
public:
    CityRenderProbe() : CubeComponent({3.2f, 0.2f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.55f, 0.55f, 0.55f}, {0.95f, 0.2f, 0.65f, 1.0f}) {}
    void Render(ID3D11DeviceContext* context) override
    {
        GameComponent::Render(context);
        const auto call = ++calls;
        const char* path = std::getenv("CURIOUS_CITY_RENDER_PROBE");
        if (path && *path && (call == 1 || call % 25 == 0))
        {
            std::ofstream log(std::filesystem::u8path(path), std::ios::app);
            if (log) log << "render_call=" << call << " context=" << (context != nullptr) << '\n';
        }
    }
private:
    inline static unsigned calls = 0;
};
}

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    SetSimulationTimeScale(1.0f);
    auto& scene = GetScene();
    if (!scene.FindComponentByName("CesiumResident")) {
        auto model = SceneAssets::CreateModel("/Game/Models/CesiumMan/CesiumMan.glb", false);
        auto* resident = dynamic_cast<SkeletalMeshComponent*>(model.get());
        if (!resident || !resident->GetModel() || resident->GetModel()->Clips.empty())
            throw std::runtime_error("CesiumMan model or animation clip failed to load");
        resident->SetPosition({0.0f, 0.0f, 0.0f});
        resident->SetScale({0.01f, 0.01f, 0.01f});
        resident->SetClipIndex(0);
        resident->SetPlaybackSpeed(1.0f);
        const auto added = scene.AddComponent("CesiumResident", model, SceneAssets::ModelType);
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add CesiumMan resident");
    }
    if (!scene.FindComponentByName("BlenderParcel")) {
        auto parcel = SceneAssets::CreateModel("/Game/Models/blend-packed/blend-packed.glb", true);
        if (!parcel) throw std::runtime_error("Blender fixture model failed to load");
        parcel->SetPosition({0.0f, 2.0f, 0.0f});
        parcel->SetScale({0.45f, 0.45f, 0.45f});
        const auto added = scene.AddComponent("BlenderParcel", parcel, SceneAssets::ModelType);
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add Blender fixture model");
    }
    if (!scene.FindComponentByName("RenderHookProof")) {
        std::unique_ptr<GameComponent> probe = std::make_unique<CityRenderProbe>();
        const auto added = scene.AddComponent("RenderHookProof", probe, "Probe.RenderCube");
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add render hook probe");
        const char* path = std::getenv("CURIOUS_CITY_RENDER_PROBE");
        std::ofstream log(path && *path ? std::filesystem::u8path(path) : std::filesystem::path{}, std::ios::app);
        if (log) log << "device=" << (GetDevice() != nullptr) << " context=" << (GetContext() != nullptr) << " add_result=" << static_cast<unsigned>(added.result) << '\n';
    }
}

void ProjectGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    static unsigned frame = 0;
    ++frame;
    const char* textureProbe = std::getenv("CURIOUS_CITY_TEXTURE_PROBE");
    if (textureProbe && *textureProbe && (frame == 1 || frame == 2 || frame == 3 || frame == 10 || frame == 20)) {
        auto* home = GetScene().FindComponentByName("WestHome");
        bool textureAssigned = false;

        std::ofstream log(std::filesystem::u8path(textureProbe), std::ios::app);
        if (log) log << "frame=" << frame << " found=" << (home != nullptr)
            << " texture_path=" << (home ? home->GetTexturePath() : std::string("<missing>"))
            << " has_texture=" << (home && home->HasTexture())
            << " texture_srv=" << (home && home->GetTexture() != nullptr)
            << " try_texture=" << textureAssigned
            << " device=" << (GetDevice() != nullptr) << " context=" << (GetContext() != nullptr) << '\n';
    }
    const char* path = std::getenv("CURIOUS_CITY_FRAME_DUMP_PATH");
    if (path && *path && frame == 20) RequestFrameDump(path);
}

void ProjectGame::TickComponents(float deltaTime)
{
    Game::TickComponents(deltaTime);
    static unsigned frame = 0;
    ++frame;
    const char* enabled = std::getenv("CURIOUS_CITY_SKELETAL_PROBE");
    const char* path = std::getenv("CURIOUS_CITY_LOG");
    if (!enabled || !*enabled || *enabled == '0' || !path || frame % 10 != 0) return;
    auto* resident = dynamic_cast<SkeletalMeshComponent*>(GetScene().FindComponentByName("CesiumResident"));
    if (!resident) return;
    const auto& model = resident->GetModel();
    std::ofstream log(std::filesystem::u8path(path), std::ios::app);
    if (!log || !model) return;
    log << "skeletal frame=" << frame << " loaded=1 meshes=" << model->Meshes.size()
        << " clips=" << model->Clips.size() << " clip=" << resident->GetClipIndex()
        << " clip_name=" << (resident->GetClipIndex() < model->Clips.size() ? model->Clips[resident->GetClipIndex()].Name : "invalid")
        << " animation_time=" << resident->GetAnimationTime() << " bones=" << resident->GetBonePalette().size() << '\n';
}


