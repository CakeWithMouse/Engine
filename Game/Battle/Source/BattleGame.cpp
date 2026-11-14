#include "../../../Engine/Source/Components/ComponentInternal.h"
#include "BattleGame.h"
#include <Engine/AI/Ai.h>

#include <Engine/Components/Light/PointLightComponent.h>
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
#include <fstream>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace
{
    constexpr float ArmyGap = 48.0f; // distance from the centre line to the first row on either side

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
    if (Config.Ai)
    {
        // The AI owns the vehicles, and its seconds must be real seconds.
        Config.Moving = false;
        SetSimulationTimeScale(1.0f);
    }
    GetSettings().SetRenderingType(Config.Forward ? Forward : Deffered);

    // One sun with cascaded shadows: the light model of a strategy battlefield.
    const float sunElevation = glm::radians(std::max(5.0f, std::min(Config.SunElevation, 89.0f)));
    const glm::vec3 sunDirection(-std::cos(sunElevation) * 0.83f, -std::sin(sunElevation), -std::cos(sunElevation) * 0.55f);
    SetDirectionalLight(sunDirection, glm::vec3(1.0f, 0.97f, 0.90f), 1.15f);
    SetShadowSettings(Config.Shadows, 900.0f);
    SetDeferredDebugOverlay(false);

    const int unitsPerRow = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(Config.UnitCount)))));
    const float fieldSize = unitsPerRow * Config.Spacing + (Config.Ai ? 2.0f * ArmyGap : 0.0f);
    FieldHalfSize = fieldSize * 0.5f;
    const int firstArmySize = (Config.UnitCount + 1) / 2;
    // In the AI demo each army fills its own rows, from the front line outwards.
    const auto spawn = [&](int index, float xJitter, float zJitter) {
        if (Config.Ai)
        {
            const bool secondArmy = index >= firstArmySize;
            const int member = secondArmy ? index - firstArmySize : index;
            const float x = (member % unitsPerRow - (unitsPerRow - 1) * 0.5f + xJitter) * Config.Spacing;
            const float z = ArmyGap + (member / unitsPerRow + 0.5f + zJitter) * Config.Spacing;
            return glm::vec3(x, 0.0f, secondArmy ? z : -z);
        }
        return glm::vec3((index % unitsPerRow - unitsPerRow * 0.5f + xJitter) * Config.Spacing, 0.0f,
                         (index / unitsPerRow - unitsPerRow * 0.5f + zJitter) * Config.Spacing);
    };
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

    EngineInternal::ComponentAccess::StaticShadow(*ground, true);
    Register(*this, "Ground", std::move(ground));

    std::mt19937 rng(1337u);

    // Map scenery: never moves, so its distant shadow is baked once; only the near cascades redraw it.
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
            EngineInternal::ComponentAccess::StaticShadow(*scenery, Config.BakedShadows);
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
            const float unitSpeed = speed(rng);
            MeshInstance instance;
            const float zJitter = jitter(rng);
            instance.Position = spawn(index, jitter(rng), zJitter);
            const bool bSecondArmy = Config.Ai ? instance.Position.z > 0.0f : row > unitsPerRow * 0.5f;
            instance.Yaw = Config.Ai ? (bSecondArmy ? glm::pi<float>() : 0.0f)
                                     : (bSecondArmy ? 1.5707963f : -1.5707963f);
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
        const glm::vec4 teamColors[2] = {
            Config.Ai ? glm::vec4(0.28f, 0.48f, 0.75f, 1.0f) : glm::vec4(0.35f, 0.42f, 0.30f, 1.0f),
            Config.Ai ? glm::vec4(0.73f, 0.30f, 0.23f, 1.0f) : glm::vec4(0.45f, 0.33f, 0.28f, 1.0f)};

        for (int index = 0; index < Config.UnitCount; ++index)
        {
            const int row = index / unitsPerRow;
            const int column = index % unitsPerRow;
            // Types alternate along the row, so every formation mixes vehicles.
            UnitBatch& batch = UnitBatches[static_cast<size_t>((row + column) % typeCount)];
            const float rowStagger = (row % 2 == 0) ? 0.0f : 0.5f;

            MeshInstance instance;
            const float zJitter = jitterAlong(rng);
            instance.Position = spawn(index, rowStagger + jitterAcross(rng), zJitter);
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

    if (Config.PointLight)
    {
        // Sized from the field, as the camera is, so the pool of light reads the same at any unit count.
        const float lightRadius = std::max(90.0f, FieldHalfSize * 0.5f);
        auto light = std::make_unique<PointLightComponent>(glm::vec3(0.0f, lightRadius * 0.2f, 0.0f),
                                                           glm::vec3(1.0f, 0.55f, 0.25f), 1.4f, lightRadius);
        light->SetCastShadows(Config.PointShadows);
        Register(*this, "PointLight", std::move(light));
    }

    Player* player = GetPlayer();
    player->SetCameraMode(CameraMode::Orbital);
    player->SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    player->SetMinMaximalOrbitRadius(40.0f, std::max(600.0f, fieldSize * 1.5f));
    SequenceCameraRadius = Config.CameraRadius > 0.0f ? Config.CameraRadius : std::max(180.0f, fieldSize * 0.37f);
    player->SetOrbitRadius(SequenceCameraRadius);
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

    // What the HUD looks like lives in the markup next to the exe, and the component watches it:
    // panels can be moved, recoloured or dropped while the scene runs. The scene only knows the
    // names it writes to, so a layout change needs no rebuild.
    const std::string markup = (Config.ContentRoot / "UI" / "Hud.ui").generic_u8string();
    if (!hud->LoadMarkup(markup))
    {
        // The component printed the line that is wrong; the field is worth more than the panels.
        return;
    }

    const std::string uiShaderPath = EnginePaths::Shader("UI.hlsl").generic_u8string();
    Hud = Register(*this, "Hud", std::move(hud), uiShaderPath, uiShaderPath);
    BindHud();
}

void BattleGame::BindHud()
{
    if (Hud == nullptr)
    {
        return;
    }
    HudRevision = Hud->GetMarkupRevision();

    HudFrameLabel = Hud->Find<UILabel>("frame");
    HudDrawLabel = Hud->Find<UILabel>("draws");
    HudUnitLabel = Hud->Find<UILabel>("units");
    HudAiStats = Hud->Find<UILabel>("aiStats");
    HudAiState = Hud->Find<UILabel>("aiState");
    HudAiAlive = Hud->Find<UILabel>("aiAlive");
    HudAiCombat = Hud->Find<UILabel>("aiCombat");
    if (auto* aiPanel = Hud->Find<UIPanel>("aiPanel")) aiPanel->bVisible = Config.Ai;
    if (auto* hint = Hud->Find<UILabel>("hint"); hint && Config.Ai)
        hint->SetText("F6 — уровни детализации   F7 — панели   F9 — отладчик ИИ   WASD — камера");
    HudFrameBar = Hud->Find<UIBar>("load");
    for (int level = 0; level < 4; ++level)
    {
        HudLodLabels[level] = Hud->Find<UILabel>("lod" + std::to_string(level));
    }

    // The markup says what the button looks like; what it does stays here.
    Hud->BindClick("lodToggle", [this]() { SetLodDebugColors(!Config.LodDebugColors); });
}

void BattleGame::SetLodDebugColors(bool enabled)
{
    Config.LodDebugColors = enabled;
    for (UnitBatch& batch : UnitBatches)
    {
        batch.Component->SetLodDebugColors(enabled);
    }
}

void BattleGame::UpdateHud()
{
    if (Hud == nullptr || !Hud->IsVisible())
    {
        return;
    }
    if (Hud->GetMarkupRevision() != HudRevision)
    {
        // The markup was edited while the game runs: the tree, and every pointer into it, is new.
        BindHud();
    }

    const double frameMs = LastTimestampMs > 0.0 ? (NowMs() - LastTimestampMs) : 0.0;
    // A moving average: the raw frame time jitters too much to read off the screen. The first
    // frames carry the cost of building atlases and shadow maps, and smoothing them in would keep
    // the number wrong for a hundred frames, so until the scene settles the average just follows.
    const bool bSettled = UpdateCount > 10;
    HudFrameMs = (bSettled && HudFrameMs > 0.0) ? HudFrameMs * 0.9 + frameMs * 0.1 : frameMs;

    // Every element is optional: the markup is authored outside the code, and a panel someone
    // renamed or deleted must not take the scene down with it.
    if (HudFrameLabel != nullptr)
    {
        std::ostringstream frameText;
        frameText.precision(1);
        frameText << std::fixed << HudFrameMs << " мс   "
                  << (HudFrameMs > 0.001 ? 1000.0 / HudFrameMs : 0.0) << " FPS";
        HudFrameLabel->SetText(frameText.str());
    }
    if (HudDrawLabel != nullptr)
    {
        HudDrawLabel->SetText("Вызовов отрисовки: " + std::to_string(LastFrameStats.DrawCalls));
    }
    if (HudUnitLabel != nullptr)
    {
        HudUnitLabel->SetText("Техники: " + std::to_string(Config.UnitCount) +
                              "   отсечено: " + std::to_string(LastFrameStats.CulledInstances));
    }
    if (bAiReady) UpdateAiHud();

    if (HudFrameBar != nullptr)
    {
        // 16.7 ms is the whole budget of a 60 FPS frame.
        HudFrameBar->Fill = static_cast<float>(HudFrameMs / 16.7);
        HudFrameBar->FillColor = HudFrameMs > 16.7 ? glm::vec4(0.85f, 0.3f, 0.25f, 0.9f)
                                                   : glm::vec4(0.89f, 0.52f, 0.24f, 0.9f);
    }

    for (int level = 0; level < 4; ++level)
    {
        if (HudLodLabels[level] != nullptr)
        {
            HudLodLabels[level]->SetText(std::to_string(CountForLod(level)));
        }
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
    ++UpdateCount;
    Game::PreUpdate(deltaTime);
    if (!Config.CameraSequence.empty())
    {
        const auto directory = std::filesystem::u8path(Config.CameraSequence);
        // These are the matrices of the preceding completed render, paired with its BMP.
        if (UpdateCount >= 61 && UpdateCount <= 181)
        {
            std::ofstream samples(directory / "matrices.csv", UpdateCount == 61 ? std::ios::trunc : std::ios::app);
            if (!samples) throw std::runtime_error("Cannot write camera sequence matrices.csv");
            samples << std::setprecision(9);
            if (UpdateCount == 61) samples << "frame,cascade,m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23,m30,m31,m32,m33\n";
            for (int cascade = 0; cascade < MaxShadowCascades; ++cascade)
            {
                samples << (UpdateCount - 61) << ',' << cascade;
                const auto& matrix = GetShadowMatrices()[cascade];
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col) samples << ',' << matrix.m[row][col];
                samples << '\n';
            }
        }
        if (UpdateCount >= 60 && UpdateCount <= 180)
        {
            const float step = static_cast<float>(UpdateCount - 60);
            auto* camera = GetPlayer();
            camera->SetRotate(0.8f + (Config.CameraMotion == "orbit" ? step * 0.0005f : 0.0f),
                              glm::radians(std::max(10.0f, std::min(Config.CameraPitch, 85.0f))));
            camera->SetOrbitTarget(glm::vec3(Config.CameraMotion == "pan" ? step * 0.01f : 0.0f, 0.0f, 0.0f));
            camera->SetOrbitRadius(SequenceCameraRadius + (Config.CameraMotion == "dolly" ? step * 0.01f : 0.0f));
            std::ostringstream name;
            name << "frame-" << std::setw(3) << std::setfill('0') << (UpdateCount - 60) << ".bmp";
            RequestFrameDump((directory / name.str()).generic_u8string());
        }
    }
    UpdateHotkeys();
    if (Config.Ai && !bAiReady) SetupAi();
    LastAiMs = bAiReady ? Ai::Get(*this).GetStats().totalMs : 0.0;
    if (Config.Moving)
    {
        UpdateUnits(deltaTime);
    }
    UpdateHud();

    if (Bench.IsBenchActive())
    {
        if (GetSettings().IsVSyncEnabled())
        {
            std::cerr << "Error: VSync is enabled during benchmark, measurement is invalid!" << std::endl;
            PostQuitMessage(1);
            return;
        }

        const int slot = (GpuTimerSlot + 1) % GpuTimerLatency;
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
        const uint64_t frameIdx = UpdateCount;

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
            AiMsSum += LastAiMs;
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
        if (LastTimestampMs > 0.0 && UpdateCount > WarmupFrames)
        {
            ++MeasuredFrames;
            AiMsSum += LastAiMs;
            FrameMsSum += now - LastTimestampMs;
            GpuMsSum += LastGpuFrameMs;
            DrawCallSum += LastFrameStats.DrawCalls;
            InstancesSum += LastFrameStats.InstancesDrawn;
        }
        LastTimestampMs = now;
    }

    if (Config.DumpShadows && UpdateCount == 20)
    {
        DumpShadowMaps("shadow");
    }

    // Late enough that the scene has settled and the frame time on screen is the real one.
    if (!Config.DumpFrame.empty() && UpdateCount == Config.DumpFrameAt)
    {
        RequestFrameDump(Config.DumpFrame);
    }

    if (Config.DumpImpostors && UpdateCount == 20)
    {
        for (const UnitBatch& batch : UnitBatches)
        {
            batch.Component->DumpImpostorAtlas(std::string("impostor-") + UnitMesh::VehicleName(batch.Type));
        }
    }

    if (FrameLimit > 0 && UpdateCount >= FrameLimit)
    {
        PostQuitMessage(0);
    }
}

void BattleGame::TickComponents(float deltaTime)
{
    Game::TickComponents(deltaTime);
    if (bAiReady) UpdateCombatEffects(deltaTime);
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

    // F6: LOD debug colours. F1-F5 belong to the engine and the camera; F7 hides the panels, and
    // both used to sit on F7, so one key toggled the colours and the whole HUD at once.
    const bool bDown = input->IsKeyDown(Keys::F6);
    if (bDown && !bLodDebugKeyWasDown)
    {
        SetLodDebugColors(!Config.LodDebugColors);
    }
    bLodDebugKeyWasDown = bDown;

    const bool bHudDown = input->IsKeyDown(Keys::F7);
    if (bHudDown && !bHudKeyWasDown && Hud != nullptr)
    {
        Hud->SetVisible(!Hud->IsVisible());
    }
    bHudKeyWasDown = bHudDown;

    // F9: what the AI is doing. The regiment nearest the centre of the field is selected.
    const bool bAiDebugDown = input->IsKeyDown(Keys::F9);
    if (bAiDebugDown && !bAiDebugKeyWasDown && bAiReady)
    {
        ShowAiDebug(!Ai::Get(*this).Debug().IsVisible());
    }
    bAiDebugKeyWasDown = bAiDebugDown;
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
    Bench.PrintHeader(width, height, GetSettings().IsVSyncEnabled(), Config.UnitCount, GpuTimerLatency, GetDevice());
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
    std::string report = bench + extra;
    if (Config.Ai)
    {
        char ai[96];
        std::snprintf(ai, sizeof(ai), " ai_ms=%.3f regiments=%zu", AiMsSum / frames, Regiments);
        report += ai;
        report += DescribeAiStates();
    }
    return report;
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
    std::string report = buffer;
    if (bAiReady)
    {
        char ai[96];
        std::snprintf(ai, sizeof(ai), " ai_ms=%.3f regiments=%zu", AiMsSum / frames, Regiments);
        report += ai;
        report += DescribeAiStates();
    }
    return report;
}
