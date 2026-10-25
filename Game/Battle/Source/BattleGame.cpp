#include "BattleGame.h"

#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Resources/Paths.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int WarmupFrames = 10;

    double NowMs()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
    }

    template <class T>
    T* Register(Game& game, const std::string& name, std::unique_ptr<T> component,
                const std::string& pixelShader = {}, const std::string& vertexShader = {})
    {
        const RegisterResult result = game.RegisterComponent(name, component.get(), pixelShader, vertexShader);
        if (result != RegisterResult::Ok)
        {
            throw std::runtime_error("Cannot register " + name + ": " + ToString(result));
        }
        return component.release();
    }
}

InstancedMeshComponent* BattleGame::CreateUnitBatch(UnitMesh::VehicleType type, const std::string& shaderPath,
                                                    const std::shared_ptr<Material>& material)
{
    auto units = std::make_unique<InstancedMeshComponent>();
    // Set before registration, so the sprite atlas is baked through the material right away.
    units->SetMaterial(material);
    for (int level = 0; level < 3; ++level)
    {
        MeshLodData lod = UnitMesh::BuildVehicleLod(type, level, Config.Detail);
        std::cout << UnitMesh::VehicleName(type) << " LOD" << level << ": "
                  << UnitMesh::CountTriangles(lod) << " triangles" << std::endl;
        if (!units->SetLod(level, std::move(lod)))
        {
            throw std::runtime_error(std::string("Cannot build LOD for ") + UnitMesh::VehicleName(type));
        }
    }

    // Switching by on-screen size: independent of model size, window size and field of view.
    // Three mesh levels plus the generated sprite level, so three thresholds.
    units->SetLodScreenHeights({45.0f, 16.0f, Config.SpritePixels});
    units->SetLodFadeFraction(0.06f);
    units->SetDrawDistance(2500.0f);
    units->SetShadowDistance(900.0f);
    units->SetShadowLodBias(1);
    units->SetLodDebugColors(Config.LodDebugColors);
    if (Config.Impostors)
    {
        units->SetImpostorEnabled(true);
    }

    return Register(*this, std::string("Units_") + UnitMesh::VehicleName(type), std::move(units),
                    shaderPath, shaderPath);
}

void BattleGame::BuildScene(const Settings& settings)
{
    Config = settings;
    RenderingType = Config.Forward ? Forward : Deffered;

    // One sun with cascaded shadows: the light model of a strategy battlefield.
    const float sunElevation = glm::radians(std::max(5.0f, std::min(Config.SunElevation, 89.0f)));
    const glm::vec3 sunDirection(-std::cos(sunElevation) * 0.83f, -std::sin(sunElevation), -std::cos(sunElevation) * 0.55f);
    SetDirectionalLight(sunDirection, glm::vec3(1.0f, 0.97f, 0.90f), 1.15f);
    SetShadowSettings(Config.Shadows, 900.0f);
    SetDeferredDebugOverlay(false);

    const int unitsPerRow = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(Config.UnitCount)))));
    const float fieldSize = unitsPerRow * Config.Spacing;
    FieldHalfSize = fieldSize * 0.5f;
    const std::string unitShaderPath = EnginePaths::Shader("InstancedMesh.hlsl").generic_u8string();
    std::shared_ptr<Material> unitMaterial;
    std::shared_ptr<Material> buildingMaterial;
    if (Config.Materials)
    {
        auto load = [&](const char* file)
        {
            std::string error;
            std::shared_ptr<Material> material = LoadMaterial((Config.ContentRoot / "Materials" / file).generic_u8string(), &error);
            if (!material) throw std::runtime_error(error);
            return material;
        };
        unitMaterial = load("Vehicle.mat");
        buildingMaterial = load("Building.mat");
    }
    // The baked map covers the whole field, so scenery shadows never follow the camera.
    SetStaticShadowArea(glm::vec3(0.0f, 0.0f, 0.0f), FieldHalfSize * 1.2f + 50.0f);

    auto ground = std::make_unique<CubeComponent>(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f),
                                                  glm::vec3(fieldSize * 1.5f, 1.0f, fieldSize * 1.5f),
                                                  glm::vec4(0.33f, 0.37f, 0.27f, 1.0f));
    ground->InitCube();
    ground->SetStaticShadowCaster(true);
    Register(*this, "Ground", std::move(ground));

    std::mt19937 rng(1337u);

    // Map scenery: never moves, so its shadow is baked once instead of being redrawn per cascade.
    if (Config.SceneryCount > 0)
    {
        const int sceneryTypeCount = static_cast<int>(UnitMesh::SceneryType::Count);
        std::uniform_real_distribution<float> place(-FieldHalfSize, FieldHalfSize);
        std::uniform_real_distribution<float> turn(0.0f, 6.283f);

        for (int typeIndex = 0; typeIndex < sceneryTypeCount; ++typeIndex)
        {
            const UnitMesh::SceneryType type = static_cast<UnitMesh::SceneryType>(typeIndex);
            auto scenery = std::make_unique<InstancedMeshComponent>();
            for (int level = 0; level < 2; ++level)
            {
                scenery->SetLod(level, UnitMesh::BuildSceneryLod(type, level, Config.Detail));
            }
            scenery->SetLodScreenHeights({40.0f, 10.0f});
            scenery->SetLodFadeFraction(0.06f);
            scenery->SetStaticShadowCaster(Config.BakedShadows);
            scenery->SetMaterial(buildingMaterial);

            const int countForType = Config.SceneryCount / sceneryTypeCount +
                                     (typeIndex < Config.SceneryCount % sceneryTypeCount ? 1 : 0);
            scenery->ReserveInstances(static_cast<size_t>(countForType));
            for (int index = 0; index < countForType; ++index)
            {
                MeshInstance building;
                building.Position = glm::vec3(place(rng), 0.0f, place(rng));
                building.Yaw = turn(rng);
                building.Scale = 1.0f;
                building.Color = glm::vec4(0.55f, 0.52f, 0.47f, 1.0f);
                scenery->AddInstance(building);
            }
            InstancedMeshComponent* registered =
                Register(*this, std::string("Scenery_") + UnitMesh::SceneryName(type), std::move(scenery),
                         unitShaderPath, unitShaderPath);
            if (Scenery == nullptr)
            {
                Scenery = registered;
            }
        }
    }

    // One component per vehicle type; units are spread between them.
    const int typeCount = std::max(1, std::min(Config.VehicleTypes, static_cast<int>(UnitMesh::VehicleType::Count)));
    UnitBatches.reserve(static_cast<size_t>(typeCount));
    for (int typeIndex = 0; typeIndex < typeCount; ++typeIndex)
    {
        UnitBatch batch;
        batch.Type = static_cast<UnitMesh::VehicleType>(typeIndex);
        batch.Component = CreateUnitBatch(batch.Type, unitShaderPath, unitMaterial);
        UnitBatches.push_back(std::move(batch));
    }

    std::uniform_real_distribution<float> jitter(-0.25f, 0.25f);
    std::uniform_real_distribution<float> speed(4.0f, 9.0f);
    std::uniform_real_distribution<float> wear(0.0f, 1.0f);
    const glm::vec4 teamColors[2] = {glm::vec4(0.35f, 0.42f, 0.30f, 1.0f), glm::vec4(0.45f, 0.33f, 0.28f, 1.0f)};

    for (int index = 0; index < Config.UnitCount; ++index)
    {
        const int row = index / unitsPerRow;
        const int column = index % unitsPerRow;
        // Types alternate along the row, so every formation mixes vehicles.
        UnitBatch& batch = UnitBatches[static_cast<size_t>((row + column) % typeCount)];

        MeshInstance instance;
        instance.Position = glm::vec3(
            (column - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing,
            0.0f,
            (row - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing);
        // Two armies facing each other across the middle of the field.
        const bool bSecondArmy = instance.Position.z > 0.0f;
        instance.Yaw = bSecondArmy ? 3.14159f : 0.0f;
        instance.Scale = 1.0f;
        instance.Color = teamColors[bSecondArmy ? 1 : 0];
        // How muddy this unit is (the material's InstanceData.x; ignored by the legacy shader).
        instance.CustomData = glm::vec3(wear(rng), 0.0f, 0.0f);
        batch.Component->AddInstance(instance);

        const float typeSpeed = speed(rng) * UnitMesh::VehicleSpeedScale(batch.Type);
        batch.Speeds.push_back(bSecondArmy ? -typeSpeed : typeSpeed);
    }

    Player* player = GetPlayer();
    player->SetCameraMode(CameraMode::Orbital);
    player->SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    player->SetMinMaximalOrbitRadius(40.0f, std::max(600.0f, fieldSize * 1.5f));
    player->SetOrbitRadius(Config.CameraRadius > 0.0f ? Config.CameraRadius : std::max(180.0f, fieldSize * 0.37f));
    player->SetRotate(0.8f, glm::radians(std::max(10.0f, std::min(Config.CameraPitch, 85.0f))));
    player->StopRotate(true, true);
    player->EnableWASD(true);
}

void BattleGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    UpdateHotkeys();
    if (Config.Moving)
    {
        UpdateUnits(deltaTime);
    }

    // Frame timing of this process, so a run can be compared without a profiler.
    const double now = NowMs();
    if (LastTimestampMs > 0.0 && GetFrameIndex() > WarmupFrames)
    {
        ++MeasuredFrames;
        FrameMsSum += now - LastTimestampMs;
        GpuMsSum += LastGpuFrameMs;
        DrawCallSum += LastFrameStats.DrawCalls;
        InstancesSum += LastFrameStats.InstancesDrawn;
    }
    LastTimestampMs = now;

    if (Config.DumpShadows && GetFrameIndex() == 20)
    {
        DumpShadowMaps("shadow");
    }

    if (FrameLimit > 0 && GetFrameIndex() >= FrameLimit)
    {
        PostQuitMessage(0);
    }
}

void BattleGame::UpdateUnits(float deltaTime)
{
    // Every unit moves, so the instance streams are rebuilt and uploaded every frame: the
    // worst case for CPU cost, which is what the measurement should show.
    for (UnitBatch& batch : UnitBatches)
    {
        const size_t count = batch.Component->GetMeshInstanceCount();
        for (size_t index = 0; index < count; ++index)
        {
            const MeshInstance& instance = batch.Component->GetMeshInstance(index);
            glm::vec3 position = instance.Position;
            position.z += batch.Speeds[index] * deltaTime;
            if (position.z > FieldHalfSize) position.z -= 2.0f * FieldHalfSize;
            if (position.z < -FieldHalfSize) position.z += 2.0f * FieldHalfSize;
            batch.Component->SetInstancePosition(index, position);
        }
    }
}

void BattleGame::UpdateHotkeys()
{
    InputDevice* input = GetInputDevice();
    if (input == nullptr)
    {
        return;
    }

    // F7: LOD debug colours (F6 is the engine's shader reload).
    const bool bDown = input->IsKeyDown(Keys::F7);
    if (bDown && !bLodDebugKeyWasDown)
    {
        Config.LodDebugColors = !Config.LodDebugColors;
        for (UnitBatch& batch : UnitBatches)
        {
            batch.Component->SetLodDebugColors(Config.LodDebugColors);
        }
    }
    bLodDebugKeyWasDown = bDown;
}

uint32_t BattleGame::CountForLod(int level) const
{
    uint32_t total = 0;
    for (const UnitBatch& batch : UnitBatches)
    {
        total += batch.Component->GetDrawnInstancesForLod(level);
    }
    return total;
}

std::string BattleGame::BuildReport() const
{
    const double frames = static_cast<double>(MeasuredFrames == 0 ? 1 : MeasuredFrames);
    const double frameMs = FrameMsSum / frames;

    uint32_t sprites = 0;
    uint32_t culled = 0;
    bool bHasImpostors = false;
    for (const UnitBatch& batch : UnitBatches)
    {
        sprites += batch.Component->GetDrawnImpostors();
        culled += batch.Component->GetCulledInstances();
        bHasImpostors = bHasImpostors || batch.Component->HasImpostor();
    }

    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "units=%d types=%zu scenery=%d mode=%s shading=%s shadows=%s sprites=%s frames=%llu frame_ms=%.2f fps=%.1f gpu_ms=%.2f draws=%.1f instances=%.0f lod0=%u lod1=%u lod2=%u sprite=%u culled=%u",
                  Config.UnitCount,
                  UnitBatches.size(),
                  Config.SceneryCount,
                  Config.Forward ? "forward" : "deferred",
                  Config.Materials ? "materials" : "legacy",
                  Config.Shadows ? "on" : "off",
                  bHasImpostors ? "on" : "off",
                  static_cast<unsigned long long>(MeasuredFrames),
                  frameMs,
                  frameMs > 0.0 ? 1000.0 / frameMs : 0.0,
                  GpuMsSum / frames,
                  static_cast<double>(DrawCallSum) / frames,
                  static_cast<double>(InstancesSum) / frames,
                  CountForLod(0), CountForLod(1), CountForLod(2), sprites, culled);
    return buffer;
}
