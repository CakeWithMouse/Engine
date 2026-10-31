#include "BattleGame.h"

#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/SkeletalBaker.h>
#include <Engine/Render/SkeletalData.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include "../../Engine/Source/Resources/VfsRead.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace
{
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
    units->SetAssetValidation(Config.ValidateAssets, UnitMesh::VehicleName(type));
    // Set before registration, so the sprite atlas is baked through the material right away.
    units->SetMaterial(material);

    // Either the designer hands over every level, or only LOD0 and the engine builds the rest.
    const int suppliedLevels = Config.AutoLod ? 1 : 3;
    for (int level = 0; level < suppliedLevels; ++level)
    {
        MeshLodData lod = UnitMesh::BuildVehicleLod(type, level, Config.Detail);
        std::cout << UnitMesh::VehicleName(type) << " LOD" << level << ": "
                  << UnitMesh::CountTriangles(lod) << " triangles" << std::endl;
        if (!units->SetLod(level, std::move(lod)))
        {
            throw std::runtime_error(std::string("Cannot build LOD for ") + UnitMesh::VehicleName(type));
        }
    }
    if (Config.AutoLod)
    {
        // Same ratios the hand-made levels have: LOD1 about a ninth of LOD0, LOD2 a silhouette.
        units->SetAutoLod({0.12f, 0.025f});
    }

    // Switching by on-screen size: independent of model size, window size and field of view.
    // Three mesh levels plus the generated sprite level, so three thresholds.
    units->SetLodScreenHeights({45.0f, 16.0f, Config.SpritePixels});
    units->SetLodFadeFraction(0.06f);
    units->SetDrawDistance(2500.0f);
    units->SetShadowDistance(Config.ShadowDistance);
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

    if (Config.Animated)
    {
        // 1. Load CesiumMan model from standard game content
        SkeletalLoadResult loadResult = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
        if (!loadResult.Success || !loadResult.Model)
        {
            throw std::runtime_error("Failed to load CesiumMan for animated crowd: " + loadResult.ErrorMessage);
        }

        // 2. Create InstancedMeshComponent with baked animated model and skinned shaders
        const std::string skinnedShaderPath = EnginePaths::Shader("InstancedSkinnedMesh.hlsl").generic_u8string();
        auto units = std::make_unique<InstancedMeshComponent>();
        if (!units->SetAnimatedModel(GetDevice(), *loadResult.Model, 30.0f))
        {
            throw std::runtime_error("Failed to setup animated model on InstancedMeshComponent");
        }

        UnitVertexCount = 0;
        for (const auto& mesh : loadResult.Model->Meshes)
        {
            UnitVertexCount += static_cast<int>(mesh.Vertices.size());
        }

        units->SetLodScreenHeights({16.0f, 7.0f});

        units->SetLodFadeFraction(0.06f);
        units->SetDrawDistance(2500.0f);
        units->SetShadowDistance(900.0f);
        units->SetShadowLodBias(1);
        units->SetLodDebugColors(Config.LodDebugColors);


        InstancedMeshComponent* registered = Register(*this, "Units_CesiumMan", std::move(units),
                                                      skinnedShaderPath, skinnedShaderPath);

        UnitBatch batch;
        batch.Component = registered;
        batch.Type = UnitMesh::VehicleType::Tank;
        batch.Speeds.reserve(static_cast<size_t>(Config.UnitCount));

        std::uniform_real_distribution<float> jitter(-0.25f, 0.25f);
        std::uniform_real_distribution<float> speed(1.5f, 3.5f);
        std::uniform_real_distribution<float> phaseDist(0.0f, 1.0f);
        const glm::vec4 teamColors[2] = {glm::vec4(0.35f, 0.42f, 0.60f, 1.0f), glm::vec4(0.65f, 0.33f, 0.28f, 1.0f)};

        registered->ReserveInstances(static_cast<size_t>(Config.UnitCount));
        for (int index = 0; index < Config.UnitCount; ++index)
        {
            const int row = index / unitsPerRow;
            const int column = index % unitsPerRow;

            const bool bSecondArmy = (row - unitsPerRow * 0.5f) > 0.0f;
            const float unitSpeed = speed(rng);
            MeshInstance instance;
            instance.Position = glm::vec3((column - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing, 0.0f,
                                          (row - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing);
            instance.Yaw = bSecondArmy ? 1.5707963f : -1.5707963f;
            instance.Scale = 1.0f;
            instance.Color = teamColors[bSecondArmy ? 1 : 0];
            instance.ClipId = 0;
            instance.AnimationPhase = phaseDist(rng);
            instance.Speed = unitSpeed;

            registered->AddInstance(instance);
            batch.Speeds.push_back(bSecondArmy ? -unitSpeed : unitSpeed);
        }

        UnitBatches.push_back(std::move(batch));
    }
    else
    {
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

        // A formation on an exact lattice does not read as an army: every vehicle is the same box
        // at the same angle, so the rows merge into unbroken bars with the ground showing between
        // them, and from a distance the field looks striped. Rows are offset by half a step
        // against each other and every vehicle is turned a few degrees, which keeps the formation
        // but breaks the lattice. Sideways there is room to spare between the rows, along the row
        // there is not.
        std::uniform_real_distribution<float> jitterAcross(-0.3f, 0.3f);
        std::uniform_real_distribution<float> jitterAlong(-0.12f, 0.12f);
        std::uniform_real_distribution<float> yawJitter(-0.14f, 0.14f);
        std::uniform_real_distribution<float> speed(4.0f, 9.0f);
        std::uniform_real_distribution<float> wear(0.0f, 1.0f);
        const glm::vec4 teamColors[2] = {glm::vec4(0.35f, 0.42f, 0.30f, 1.0f), glm::vec4(0.45f, 0.33f, 0.28f, 1.0f)};

        for (int index = 0; index < Config.UnitCount; ++index)
        {
            const int row = index / unitsPerRow;
            const int column = index % unitsPerRow;
            // Types alternate along the row, so every formation mixes vehicles.
            UnitBatch& batch = UnitBatches[static_cast<size_t>((row + column) % typeCount)];
            const float rowStagger = (row % 2 == 0) ? 0.0f : 0.5f;

            MeshInstance instance;
            instance.Position = glm::vec3(
                (column - unitsPerRow * 0.5f + rowStagger + jitterAcross(rng)) * Config.Spacing,
                0.0f,
                (row - unitsPerRow * 0.5f + jitterAlong(rng)) * Config.Spacing);
            // Two armies facing each other across the middle of the field.
            const bool bSecondArmy = instance.Position.z > 0.0f;
            instance.Yaw = (bSecondArmy ? 3.14159f : 0.0f) + yawJitter(rng);
            instance.Scale = 1.0f;
            instance.Color = teamColors[bSecondArmy ? 1 : 0];
            // How muddy this unit is (the material's InstanceData.x; ignored by the legacy shader).
            instance.CustomData = glm::vec3(wear(rng), 0.0f, 0.0f);
            batch.Component->AddInstance(instance);

            const float typeSpeed = speed(rng) * UnitMesh::VehicleSpeedScale(batch.Type);
            batch.Speeds.push_back(bSecondArmy ? -typeSpeed : typeSpeed);
        }
    }

    Player* player = GetPlayer();
    player->SetCameraMode(CameraMode::Orbital);
    player->SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    player->SetMinMaximalOrbitRadius(40.0f, std::max(600.0f, fieldSize * 1.5f));
    player->SetOrbitRadius(Config.CameraRadius > 0.0f ? Config.CameraRadius : std::max(180.0f, fieldSize * 0.37f));
    player->SetRotate(0.8f, glm::radians(std::max(10.0f, std::min(Config.CameraPitch, 85.0f))));
    // StopRotate enables the mouse control; a locked camera keeps the same framing between runs.
    player->StopRotate(!Config.LockCamera, !Config.LockCamera);
    player->EnableWASD(!Config.LockCamera);

    BuildHud();
}

void BattleGame::BuildHud()
{
    if (!Config.Hud)
    {
        return;
    }

    auto hud = std::make_unique<UIComponent>();
    FontAtlasSettings font;
    font.Face = L"Consolas";
    font.PixelHeight = 18;
    hud->SetFont(font);

    const glm::vec4 panelColor(0.05f, 0.06f, 0.07f, 0.62f);
    const glm::vec4 textColor(0.93f, 0.94f, 0.91f, 1.0f);
    const glm::vec4 dimColor(0.58f, 0.62f, 0.66f, 1.0f);
    const glm::vec4 accentColor(0.89f, 0.52f, 0.24f, 1.0f);

    // Left panel: what the renderer is doing with the field right now.
    UIPanel* leftPanel = hud->AddPanel(UIRect{UIAnchor::TopLeft, 16.0f, 16.0f, 296.0f, 166.0f}, panelColor);
    leftPanel->BorderWidth = 1.0f;
    leftPanel->BorderColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.12f);

    hud->AddLabel(UIRect{UIAnchor::TopLeft, 32.0f, 30.0f, 0.0f, 0.0f}, "КАДР", accentColor);
    HudFrameLabel = hud->AddLabel(UIRect{UIAnchor::TopLeft, 32.0f, 56.0f, 0.0f, 0.0f}, "", textColor);
    HudDrawLabel = hud->AddLabel(UIRect{UIAnchor::TopLeft, 32.0f, 78.0f, 0.0f, 0.0f}, "", dimColor);
    HudUnitLabel = hud->AddLabel(UIRect{UIAnchor::TopLeft, 32.0f, 100.0f, 0.0f, 0.0f}, "", dimColor);

    hud->AddLabel(UIRect{UIAnchor::TopLeft, 32.0f, 128.0f, 0.0f, 0.0f}, "Загрузка кадра", dimColor);
    // One frame of the 60 FPS budget: the bar fills as the frame approaches 16.7 ms.
    HudFrameBar = hud->AddBar(UIRect{UIAnchor::TopLeft, 32.0f, 150.0f, 264.0f, 10.0f});

    // Right panel: how the units are split between the detail levels.
    UIPanel* rightPanel = hud->AddPanel(UIRect{UIAnchor::TopRight, 16.0f, 16.0f, 238.0f, 166.0f}, panelColor);
    rightPanel->BorderWidth = 1.0f;
    rightPanel->BorderColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.12f);

    hud->AddLabel(UIRect{UIAnchor::TopRight, 238.0f, 30.0f, 0.0f, 0.0f}, "ДЕТАЛИЗАЦИЯ", accentColor);

    // The swatches repeat the colours of the F6 debug view, so the two read as one thing.
    const glm::vec4 lodColors[4] = {
        glm::vec4(0.25f, 1.0f, 0.35f, 1.0f),
        glm::vec4(1.0f, 0.85f, 0.2f, 1.0f),
        glm::vec4(1.0f, 0.4f, 0.2f, 1.0f),
        glm::vec4(0.6f, 0.4f, 1.0f, 1.0f)
    };
    const char* lodNames[4] = {"LOD0", "LOD1", "LOD2", "Спрайт"};
    for (int level = 0; level < 4; ++level)
    {
        const float row = 56.0f + static_cast<float>(level) * 24.0f;
        hud->AddPanel(UIRect{UIAnchor::TopRight, 222.0f, row + 4.0f, 10.0f, 10.0f}, lodColors[level]);
        hud->AddLabel(UIRect{UIAnchor::TopRight, 202.0f, row, 0.0f, 0.0f}, lodNames[level], dimColor);
        HudLodLabels[level] = hud->AddLabel(UIRect{UIAnchor::TopRight, 32.0f, row, 90.0f, 0.0f}, "0", textColor);
        HudLodLabels[level]->bAlignRight = true;
    }

    // The height is the line itself: a bottom anchor measures the far edge, so without it the
    // text would hang past the bottom of the window.
    HudHintLabel = hud->AddLabel(UIRect{UIAnchor::BottomLeft, 16.0f, 16.0f, 0.0f, 18.0f},
                                 "F6 — раскраска по уровням   F7 — скрыть панели   WASD — камера", textColor);

    const std::string uiShaderPath = EnginePaths::Shader("UI.hlsl").generic_u8string();
    Hud = Register(*this, "Hud", std::move(hud), uiShaderPath, uiShaderPath);
}

void BattleGame::UpdateHud()
{
    if (Hud == nullptr || !Hud->IsVisible())
    {
        return;
    }

    const double frameMs = LastTimestampMs > 0.0 ? (NowMs() - LastTimestampMs) : 0.0;
    // A moving average: the raw frame time jitters too much to read off the screen. The first
    // frames carry the cost of building atlases and shadow maps, and smoothing them in would keep
    // the number wrong for a hundred frames, so until the scene settles the average just follows.
    const bool bSettled = GetFrameIndex() > 10;
    HudFrameMs = (bSettled && HudFrameMs > 0.0) ? HudFrameMs * 0.9 + frameMs * 0.1 : frameMs;

    std::ostringstream frameText;
    frameText.precision(1);
    frameText << std::fixed << HudFrameMs << " мс   " << (HudFrameMs > 0.001 ? 1000.0 / HudFrameMs : 0.0) << " FPS";
    HudFrameLabel->SetText(frameText.str());

    HudDrawLabel->SetText("Вызовов отрисовки: " + std::to_string(LastFrameStats.DrawCalls));
    HudUnitLabel->SetText("Техники: " + std::to_string(Config.UnitCount) +
                          "   отсечено: " + std::to_string(LastFrameStats.CulledInstances));

    // 16.7 ms is the whole budget of a 60 FPS frame.
    HudFrameBar->Fill = static_cast<float>(HudFrameMs / 16.7);
    HudFrameBar->FillColor = HudFrameMs > 16.7 ? glm::vec4(0.85f, 0.3f, 0.25f, 0.9f)
                                               : glm::vec4(0.89f, 0.52f, 0.24f, 0.9f);

    for (int level = 0; level < 4; ++level)
    {
        HudLodLabels[level]->SetText(std::to_string(CountForLod(level)));
    }
}

void BattleGame::EnableBench(uint64_t benchFrames, uint64_t warmupFrames)
{
    Bench.Initialize(benchFrames, warmupFrames, GpuTimerLatency);
    WarmupFrames = warmupFrames;

    // Run warmup + bench + latency frames to retire in-flight GPU queries.
    SetFrameLimit(warmupFrames + benchFrames + static_cast<uint64_t>(GpuTimerLatency));
}

void BattleGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    UpdateHotkeys();
    if (Config.Moving)
    {
        UpdateUnits(deltaTime);
    }
    UpdateHud();

    if (Bench.IsBenchActive())
    {
        if (bVSync)
        {
            std::cerr << "Error: VSync is enabled during benchmark, measurement is invalid!" << std::endl;
            PostQuitMessage(1);
            return;
        }

        const int slot = static_cast<int>(GetFrameIndex() % GpuTimerLatency);
        if (GpuTimerIssued[slot] && GpuTimerEnd[slot] && GpuTimerDisjoint[slot] && GetContext())
        {
            UINT64 end = 0;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
            const auto startWait = std::chrono::steady_clock::now();
            while (GetContext()->GetData(GpuTimerEnd[slot].Get(), &end, sizeof(end), 0) == S_FALSE ||
                   GetContext()->GetData(GpuTimerDisjoint[slot].Get(), &disjoint, sizeof(disjoint), 0) == S_FALSE)
            {
                _mm_pause();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWait).count() > 1000)
                    break;
            }
        }

        const double now = NowMs();
        const uint64_t frameIdx = GetFrameIndex();

        if (frameIdx <= Bench.GetWarmupFrames())
        {
            Bench.OnWarmupFrame(GpuTimerSamplesReady);
        }
        else
        {
            if (LastTimestampMs > 0.0)
            {
                Bench.RecordCpuSample(now - LastTimestampMs);
            }
            Bench.RecordGpuSamples(GpuTimerSamplesReady, LastGpuFrameMs);

            ++MeasuredFrames;
            FrameMsSum += (LastTimestampMs > 0.0 ? (now - LastTimestampMs) : 0.0);
            GpuMsSum += LastGpuFrameMs;
            DrawCallSum += LastFrameStats.DrawCalls;
            InstancesSum += LastFrameStats.InstancesDrawn;
        }
        LastTimestampMs = now;
    }
    else
    {
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
    }

    if (Config.DumpShadows && GetFrameIndex() == 20)
    {
        DumpShadowMaps("shadow");
    }

    // Late enough that the scene has settled and the frame time on screen is the real one.
    if (!Config.DumpFrame.empty() && GetFrameIndex() == 60)
    {
        RequestFrameDump(Config.DumpFrame);
    }

    if (Config.DumpImpostors && GetFrameIndex() == 20)
    {
        for (const UnitBatch& batch : UnitBatches)
        {
            batch.Component->DumpImpostorAtlas(std::string("impostor-") + UnitMesh::VehicleName(batch.Type));
        }
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

            if (Config.Animated)
            {
                // Advance animation phase based on speed and delta time (clip duration is 2.0s)
                float phase = instance.AnimationPhase + (std::abs(batch.Speeds[index]) * 0.25f) * deltaTime;
                phase = std::fmod(phase, 1.0f);
                if (phase < 0.0f) phase += 1.0f;
                batch.Component->SetInstancePositionAndPhase(index, position, phase);
            }
            else
            {
                batch.Component->SetInstancePosition(index, position);
            }
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

    const bool bHudDown = input->IsKeyDown(Keys::F7);
    if (bHudDown && !bHudKeyWasDown && Hud != nullptr)
    {
        Hud->SetVisible(!Hud->IsVisible());
    }
    bHudKeyWasDown = bHudDown;
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

void BattleGame::PrintBenchHeader() const
{
    const int width = DisplayPtr ? DisplayPtr->GetWidth() : 0;
    const int height = DisplayPtr ? DisplayPtr->GetHeight() : 0;
    Bench.PrintHeader(width, height, bVSync, Config.UnitCount, GpuTimerLatency, GetDevice());
    std::cout << "Animated: " << (Config.Animated ? "yes (CesiumMan VAT)" : "no") << "\n"
              << "Shadows: " << (Config.Shadows ? "enabled" : "disabled") << "\n"
              << "===============================" << std::endl;
}

std::string BattleGame::BuildBenchReport() const
{
    const std::string bench = Bench.BuildReport(Config.UnitCount, Bench.GetWarmupFrames());
    uint32_t culled = 0;
    for (const UnitBatch& batch : UnitBatches)
        if (batch.Component) culled += batch.Component->GetCulledInstances();
    const uint32_t visible = static_cast<uint32_t>(Config.UnitCount > static_cast<int>(culled) ? Config.UnitCount - culled : 0);
    const double frames = static_cast<double>(MeasuredFrames == 0 ? 1 : MeasuredFrames);
    const double avgDraws = static_cast<double>(DrawCallSum) / frames;
    const uint64_t drawnVertices = static_cast<uint64_t>(visible) * static_cast<uint64_t>(UnitVertexCount > 0 ? UnitVertexCount : 100);

    char extra[256];
    std::snprintf(extra, sizeof(extra), " visible=%u draws=%.1f vertices=%llu",
                  visible, avgDraws, static_cast<unsigned long long>(drawnVertices));
    return bench + extra;
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
    const uint32_t visible = static_cast<uint32_t>(Config.UnitCount > static_cast<int>(culled) ? Config.UnitCount - culled : 0);
    const uint64_t drawnVertices = static_cast<uint64_t>(visible) * static_cast<uint64_t>(UnitVertexCount > 0 ? UnitVertexCount : 100);

    // Long enough for every field plus the adapter name, which snprintf would otherwise cut.
    char buffer[768];
    std::snprintf(buffer, sizeof(buffer),
                  "units=%d types=%zu scenery=%d mode=%s shading=%s shadows=%s anim=%s sprites=%s frames=%llu frame_ms=%.2f fps=%.1f gpu_ms=%.2f draws=%.1f instances=%.0f visible=%u vertices=%llu lod0=%u lod1=%u lod2=%u sprite=%u culled=%u gpu=%s",
                  Config.UnitCount,
                  UnitBatches.size(),
                  Config.SceneryCount,
                  Config.Forward ? "forward" : "deferred",
                  Config.Materials ? "materials" : "legacy",
                  Config.Shadows ? "on" : "off",
                  Config.Animated ? "on" : "off",
                  bHasImpostors ? "on" : "off",
                  static_cast<unsigned long long>(MeasuredFrames),
                  frameMs,
                  frameMs > 0.0 ? 1000.0 / frameMs : 0.0,
                  GpuMsSum / frames,
                  static_cast<double>(DrawCallSum) / frames,
                  static_cast<double>(InstancesSum) / frames,
                  visible,
                  static_cast<unsigned long long>(drawnVertices),
                  CountForLod(0), CountForLod(1), CountForLod(2), sprites, culled,
                  GetAdapterName().c_str());
    return buffer;
}
