#include "ProjectGame.h"
#include "ProjectComponent.h"
#include "BomberTarget.h"
#include "RenderHostProbe.h"
#include <Engine/Components/Character.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Display/Display.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Render/MeshValidation.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/Sound/AudioEngine.h>
#include <Engine/Scene/Trace.h>
#include <Engine/SaveGame/SaveGame.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <Windows.h>

namespace
{
audio::AudioEngine::StreamHandle gRoleAudioStream{};
audio::AudioEngine::SoundId gRoleProbeSound{};
std::string gRoleProbeMode;
bool gRoleProbePending = false;
LARGE_INTEGER gAudioProbeFrequency{}, gAudioProbeStarted{};
bool gAudioProbe3D = false;
bool gAudioProbeFirstFrameLogged = false;
ComponentId Add(Game& game, const std::string& name, std::unique_ptr<GameComponent> component, const std::string& type)
{
    const auto result = game.GetScene().AddComponent(name, component, type);
    if (result.result != SceneResult::Ok && result.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add playtest component: " + name);
    
    return result.id;
}
}

bool BomberSessionDiagnostics::Initialize(SubsystemContext&, std::string& error)
{
    error.clear();
    elapsed_ = 0.0f;
    std::cout << "[TiredBomber] subsystem initialized\n";
    return true;
}

void BomberSessionDiagnostics::OnSceneReady(Scene& scene)
{
    std::cout << "[TiredBomber] subsystem scene ready: " << scene.GetName() << "\n";
}

void BomberSessionDiagnostics::OnSceneClosing(Scene& scene)
{
    std::cout << "[TiredBomber] subsystem scene closing: " << scene.GetName() << "\n";
}

void BomberSessionDiagnostics::Tick(float deltaTime)
{
    elapsed_ += deltaTime;
    ++ticks_;
    if (std::getenv("TB_SUBSYSTEM_PROBE") && ticks_ % 20 == 0)
        std::cout << "[SubsystemSaveProbe] tick=" << ticks_ << " elapsed=" << elapsed_ << "\n";
    if (elapsed_ >= 5.0f)
    {
        elapsed_ = 0.0f;
        std::cout << "[TiredBomber] subsystem tick alive\n";
    }
}

void BomberSessionDiagnostics::SerializeSaveGame(SaveArchive& archive)
{
    GameSubsystem::SerializeSaveGame(archive);
    archive.Section("TiredBomber.Score", [&](SaveArchive& state) {
        state.Version(1);
        state.Field("ticks", ticks_);
        state.Field("elapsed", elapsed_);
    });
}

void BomberSessionDiagnostics::OnSaveGameLoaded(Scene& scene)
{
    GameSubsystem::OnSaveGameLoaded(scene);
    if (std::getenv("TB_SUBSYSTEM_PROBE"))
        std::cout << "[SubsystemSaveProbe] loaded tick=" << ticks_ << " elapsed=" << elapsed_ << "\n";
}

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    Saves().Configure("tired-bomber-round", EngineModuleVfs::Resolve("/Game") + "/SaveSlots");
    if (const char* slot = std::getenv("TB_CONTROLLER_LOAD"); slot && *slot)
    {
        const auto request = Saves().RequestLoad(slot);
        std::cout << "[TiredBomber] controller-probe load request=" << static_cast<int>(request.status) << " slot=" << slot << "\n";
    }
    else if (const char* slot = std::getenv("TB_LOAD_SLOT"); slot && *slot)
    {
        const auto request = Saves().RequestLoad(slot);
        std::cout << "[TiredBomber] startup load request=" << static_cast<int>(request.status) << " slot=" << slot << "\n";
    }
    if (std::getenv("TB_START_PAUSED")) SetSimulationEnabled(false);

    // Independent WASAPI loopback probe. This test-only branch is confined to
    // the private Bomber copy and uses the public SDK AudioEngine API.
    if (const char* mode = std::getenv("TB_AUDIO_MODE"); mode && *mode)
    {
        auto* audioEngine = GetAudioEngine();
        const auto wave = std::filesystem::u8path(EngineModuleVfs::Resolve("/Game") + "/Audio/CodedTone.wav").wstring();
        if (!audioEngine)
            std::cout << "[AudioProbe] engine=unavailable mode=" << mode << "\n";
        else
        {
            const auto sound = audioEngine->LoadSound(wave);
            const bool loaded = sound.IsValid();
            bool accepted = false;
            if (std::string(mode) == "play2d")
                accepted = loaded && audioEngine->Play2D(sound, audio::AudioEngine::PlayOptions{0.35f, 0.0f, true});
            else if (std::string(mode) == "play3d-near" || std::string(mode) == "play3d-far" ||
                     std::string(mode) == "play3d-near-warm" || std::string(mode) == "play3d-far-warm")
            {
                if (std::string(mode).find("-warm") != std::string::npos)
                {
                    gRoleProbeSound = sound;
                    gRoleProbeMode = mode;
                    gRoleProbePending = loaded;
                    accepted = loaded;
                    std::cout << "[AudioProbe] deferred mode=" << mode << " loaded=" << loaded << " startFrame=10\n";
                }
                else
                {
                    audio::AudioEmitter3D emitter;
                    const auto* player = GetPlayer();
                    const auto p = player ? player->GetPosition() : glm::vec3(0.0f);
                    emitter.position = {p.x, p.y, p.z + (std::string(mode) == "play3d-near" ? 2.0f : 2000.0f)};
                    emitter.Commit(1);
                    QueryPerformanceFrequency(&gAudioProbeFrequency);
                    accepted = loaded && audioEngine->Play3D(sound, emitter, audio::AudioEngine::PlayOptions{0.7f, 0.0f, true});
                    QueryPerformanceCounter(&gAudioProbeStarted);
                    gAudioProbe3D = accepted;
                    std::cout << "[AudioProbe] playReturnQpc=" << gAudioProbeStarted.QuadPart
                              << " frequency=" << gAudioProbeFrequency.QuadPart << "\n";
                }
            }
            else if (std::string(mode) == "stream")
                gRoleAudioStream = audioEngine->PlayStream(wave, audio::AudioEngine::StreamOptions{true, 0.35f, 4096});
            std::cout << "[AudioProbe] mode=" << mode << " loaded=" << loaded
                      << " accepted=" << accepted << " stream=" << gRoleAudioStream.value << "\n";
        }
    }
    SetSimulationTimeScale(1.0f);
    SetShadowSettings(true, 100.0f);
    GetSettings().SetVSync(false);
    GetSettings().SetAntialiasing(1);
    RenderType rendering = Forward;
    if (const char* mode = std::getenv("TB_RENDERING_MODE"))
    {
        if (std::string(mode) == "Deferred") rendering = Deffered;
        else if (std::string(mode) == "Custom") rendering = Custom;
    }
    GetSettings().SetRenderingType(rendering);
    std::cout << "[TiredBomber] settings vsync=" << GetSettings().IsVSyncEnabled()
              << " aa=" << GetSettings().GetAntialiasing()
              << " mode=" << GetSettings().GetRenderingType()
              << " adapter=" << GetAdapterName() << "\n";
    const auto gameRoot = EngineModuleVfs::Resolve("/Game");
    if (!gameRoot.empty()) RequestFrameDump(gameRoot + "/tired-bomber-frame.bmp");

    if (auto* floor = GetScene().FindComponentByName("Ground"))
        floor_ = floor->GetComponentId();

    if (std::getenv("TB_RENDER_HOST_PROBE"))
    {
        auto probe = std::make_unique<RenderHostProbe>();
        probe->SetPrimitive(MeshPrimitives::Cube());
        probe->SetPosition({-2.0f, 1.0f, 4.0f});
        Add(*this, "Bomber.RenderHostProbe", std::move(probe), "RenderHostProbe");
    }

    for (int i = 0; i < 4; ++i)
    {
        auto unit = std::make_unique<BomberPiece>();
        unit->SetPrimitive(MeshPrimitives::Cube(1.0f));
        unit->SetScale({1.2f, 0.6f, 0.75f});
        unit->SetPosition({-6.0f + i * 4.0f, 0.42f, (i % 2 ? 2.0f : -2.0f)});
        unit->SetColor(i % 2 ? glm::vec4(0.18f, 0.48f, 0.85f, 1.0f) : glm::vec4(0.85f, 0.32f, 0.16f, 1.0f));
        unit->traceChannels = 2u;
        units_.push_back(Add(*this, "Bomber.Unit." + std::to_string(i + 1), std::move(unit), "BomberPiece"));
    }

    // The imported Suzanne is a real moving bomber target. Keep the source
    // material IDs from the asset map; they are identifiers, not slot order.
    if (auto* imported = dynamic_cast<SkeletalMeshComponent*>(GetScene().FindComponentByName("Bomber.ImportedTarget")))
    {
        imported->SetPosition({10.0f, 0.8f, 0.0f});
        const auto& slots = imported->GetMaterialSlots();
        std::cout << "[MaterialSlotsProbe] target slots=" << slots.size();
        for (const auto& slot : slots)
            std::cout << " sourceID=" << slot.first << " path=" << slot.second;
        std::cout << "\n";
        if (slots.size() >= 2)
        {
            auto second = slots.begin();
            ++second;
            const auto savedSecondPath = second->second;
            const bool slotChanged = imported->SetMaterialSlot(second->first, slots.begin()->second);
            const bool badIdRejected = !imported->SetMaterialSlot(0xFFFFFFFFu, slots.begin()->second);
            const bool overrideSet = imported->SetMaterialAsset(slots.begin()->second);
            const bool overrideCleared = imported->SetMaterialAsset("");
            const bool slotRestored = imported->SetMaterialSlot(second->first, savedSecondPath);
            std::cout << "[MaterialSlotsProbe] changed=" << slotChanged
                      << " unknownIdRejected=" << badIdRejected
                      << " overrideSet=" << overrideSet
                      << " overrideCleared=" << overrideCleared
                      << " slotRestored=" << slotRestored << "\n";
        }
        units_.push_back(imported->GetComponentId());
    }

    auto instances = std::make_unique<BomberMarkers>();
    instances->SetLod(0, MeshPrimitives::Cube());
    instances->SetAutoLod({0.5f, 0.25f});
    instances->SetLodDistances({9.0f, 17.0f});
    instances->SetLodDebugColors(true);
    instances->SetDrawDistance(60.0f);
    instances->ReserveInstances(13);
    for (int i = 0; i < 6; ++i)
    {
        for (int side : {-1, 1})
        {
            MeshInstance marker;
            marker.Position = {-8.0f + float(i) * 3.2f, 0.2f, float(side) * 6.0f};
            marker.Scale = 0.35f;
            marker.Color = side < 0 ? glm::vec4(0.20f, 0.70f, 0.38f, 1.0f) : glm::vec4(0.35f, 0.60f, 0.90f, 1.0f);
            instances->AddInstance(marker);
        }
    }
    MeshInstance outsideView;
    outsideView.Position = {40.0f, 0.2f, 0.0f};
    outsideView.Scale = 0.35f;
    outsideView.Color = {0.9f, 0.2f, 0.8f, 1.0f};
    instances->AddInstance(outsideView); // inside draw distance, outside the camera frustum
    instances->SetInstanceVisible(0, false); // explicit per-instance visibility filter
    markers_ = instances.get();
    Add(*this, "Bomber.Markers.LOD", std::move(instances), "BomberMarkers");

    auto screenProbe = std::make_unique<BomberMarkers>();
    screenProbe->SetLod(0, MeshPrimitives::Cube());
    screenProbe->SetAutoLod({0.5f, 0.25f});
    screenProbe->SetLodScreenHeights({50.0f, 20.0f});
    screenProbe->SetLodDebugColors(true);
    MeshInstance probe;
    probe.Position = {0.0f, 0.0f, 0.0f};
    probe.Scale = 1.0f;
    probe.Color = {0.95f, 0.88f, 0.22f, 1.0f};
    screenProbe->AddInstance(probe);
    Add(*this, "Bomber.LOD.ScreenHeightProbe", std::move(screenProbe), "BomberMarkers");

    const auto cube = MeshPrimitives::Cube();
    const auto validation = ValidateMesh(cube);
    MeshValidationSettings cubeSettings;
    cubeSettings.MinLengthMeters = 0.25f;
    cubeSettings.PivotGroundTolerance = 0.6f;
    cubeSettings.PivotCenterTolerance = 0.5f;
    cubeSettings.bExpectNoseAlongZ = false;
    const auto configuredValidation = ValidateMesh(cube, cubeSettings);
    MeshSimplifyResult simplifyResult;
    MeshSimplifySettings simplifySettings;
    simplifySettings.TargetRatio = 0.5f;
    const auto lowerLod = SimplifyMesh(cube, simplifySettings, &simplifyResult);
    const auto denseMesh = MeshPrimitives::Sphere(1.0f, 16, 8);
    MeshSimplifyResult denseSimplifyResult;
    const auto simplifiedDenseMesh = SimplifyMesh(denseMesh, simplifySettings, &denseSimplifyResult);
    std::cout << "[TiredBomber] mesh check usable=" << validation.IsUsable()
              << " triangles=" << validation.Triangles << " configured=" << configuredValidation.IsUsable()
              << " lowLOD=" << simplifyResult.ResultTriangles
              << " sphere=" << denseMesh.Indices.size() / 3
              << " sphereLOD=" << simplifiedDenseMesh.Indices.size() / 3
              << " error=" << simplifyResult.Error << "\n";
    std::cout << validation.ToText("procedural cube (default unit profile)") << "\n";

    auto camera = std::make_unique<SceneCamera>();
    camera->orthographic = true;
    camera->viewPlane = 2;
    camera->orthoHeight = 24.0f;
    camera->nearClip = 0.1f;
    camera->farClip = 100.0f;
    camera->SetPosition({0.0f, 24.0f, 0.0f});
    if (std::getenv("TB_TRACE_DEBUG_PROBE")) {
        camera->orthographic = false;
        camera->viewPlane = 0;
        camera->SetPosition({15.0f, 10.0f, -15.0f});
        camera->SetRotation({-25.0f, 135.0f, 0.0f});
    } else if (std::getenv("TB_SKELETAL_PROBE")) {
        camera->orthographic = false;
        camera->viewPlane = 0;
        camera->SetPosition({0.0f, 1.5f, 10.0f});
    }
    diagnosticsCamera_ = camera.get();
    const ComponentId cameraId = Add(*this, "Bomber.Camera", std::move(camera), "Gameplay.Camera");
    cameraId_ = cameraId;
    if (std::getenv("TB_SKELETAL_PROBE")) {
        auto skeletal = std::make_unique<BomberSkeletal>();
        if (skeletal->LoadFromVfs("/Game/Models/CesiumMan.glb")) {
            skeletal->SetPosition({0.0f, -1.0f, 2.0f});
            skeletal->SetScale({2.0f, 2.0f, 2.0f});
            const auto& model = skeletal->GetModel();
            std::cout << "[SkeletalProbe] loaded bones=" << model->ModelSkeleton.Bones.size()
                      << " clips=" << model->Clips.size()
                      << " duration=" << (model->Clips.empty() ? 0.0f : model->Clips[0].Duration) << "\n";
            Add(*this, "Bomber.SkeletalProbe", std::move(skeletal), "BomberSkeletal");
        } else {
            std::cerr << "[SkeletalProbe] LoadFromVfs failed for /Game/Models/CesiumMan.glb\n";
        }
    }
    SetRenderDiagnosticsVisible(true);
    if (std::getenv("TB_INSTANCE_FALLBACK_PROBE") || std::getenv("TB_HUD_PROBE") || std::getenv("TB_TRACE_DEBUG_PROBE")) SetRenderDiagnosticsVisible(false);
    if (std::getenv("TB_PARTICLE_COMPARE")) SetRenderDiagnosticsVisible(false);
    if (std::getenv("TB_INSTANCE_FALLBACK_PROBE")) {
        auto fallbackProbe = std::make_unique<BomberMarkers>();
        fallbackProbe->SetLod(0, MeshPrimitives::Cube());
        fallbackProbe->SetDrawDistance(100.0f);
        fallbackProbe->SetLodDebugColors(false);
        const glm::vec4 colors[] = {{0.95f, 0.12f, 0.10f, 1.0f}, {0.10f, 0.90f, 0.15f, 1.0f}, {0.10f, 0.30f, 0.98f, 1.0f}};
        for (int i = -1; i <= 1; ++i) {
            MeshInstance item;
            item.Position = {static_cast<float>(i) * 3.0f, 0.8f, 0.0f};
            item.Scale = 1.0f;
            item.Color = colors[i + 1];
            fallbackProbe->AddInstance(item);
        }
        const auto id = Add(*this, "Bomber.InstanceFallbackProbe", std::move(fallbackProbe), "BomberMarkers");
        std::cout << "[InstanceFallbackProbe] created three default-material instances id=" << id
                  << " positions=-3,0,3 colors=red,green,blue\n";
    }
    auto player = std::make_unique<Character>();
    player->SetPosition({-10.0f, 0.5f, 5.0f});
    player->SetPlayerControlled(true);
    player->standardControl = true;
    if (std::getenv("TB_CONTROLLER_PROBE")) {
        player->camera = 0;
        player->cameraSettings.viewPlane = 0;
    } else player->camera = cameraId;
    player_ = Add(*this, "Bomber.Player", std::move(player), "Gameplay.Character");
    if (std::getenv("TB_CLASSWIZARD_PROBE")) {
        auto target = std::make_unique<BomberTarget>();
        target->SetPrimitive(MeshPrimitives::Cube());
        target->SetPosition({0.0f, 0.8f, 4.0f});
        target->SetColor({0.95f, 0.15f, 0.85f, 1.0f});
        const ComponentId id = Add(*this, "Bomber.GeneratedTarget", std::move(target), "BomberTarget");
        std::cout << "[ClassWizardProbe] created component id=" << id << " type=BomberTarget\n";
    }
    if (std::getenv("TB_SCENE_CODEC_PROBE")) {
        auto target = std::make_unique<ProjectComponent>();
        target->SetPrimitive(MeshPrimitives::Cube());
        target->SetPosition({3.0f, 0.0f, 0.0f});
        const auto targetId = Add(*this, "RoleCodec.Target", std::move(target), "ProjectComponent");
        auto owner = std::make_unique<BomberReferenceProbe>();
        owner->SetPrimitive(MeshPrimitives::Cube());
        owner->SetPosition({0.0f, 1.0f, 0.0f});
        owner->targetId = targetId;
        const auto ownerId = Add(*this, "RoleCodec.Owner", std::move(owner), "BomberReferenceProbe");
        auto* reference = dynamic_cast<BomberReferenceProbe*>(GetScene().FindComponent(ownerId));
        std::cout << "[SceneCodecProbe] created owner=" << ownerId << " version="
                  << (reference ? reference->codecVersion : 0) << " score="
                  << (reference ? reference->score : 0.0f) << " target="
                  << (reference ? reference->targetId : 0) << " data="
                  << (reference ? reference->SaveProjectData() : std::string("missing")) << "\n";
    }
    if (std::getenv("TB_INPUT_PROBE")) {
        Input::Binding gameBinding; gameBinding.action = "Role.Input.Game";
        gameBinding.context = Input::Context::Game; gameBinding.chord = {Keys::K};
        gameBinding.priority = 10; gameBinding.consume = true; GetInput().Bind(gameBinding);
        Input::Binding uiBinding; uiBinding.action = "Role.Input.UI";
        uiBinding.context = Input::Context::UI; uiBinding.chord = {Keys::K};
        uiBinding.priority = 20; uiBinding.consume = true; GetInput().Bind(uiBinding);
        GetInput().SetContextEnabled(Input::Context::Game, true);
        GetInput().SetContextEnabled(Input::Context::UI, true);
        GetInput().SetContextPriority(Input::Context::Game, 10);
        GetInput().SetContextPriority(Input::Context::UI, 20);
        inputProbeConnections_.push_back(GetInput().Subscribe(inputProbeScope_, Input::Context::Game,
            "Role.Input.Game", Input::Phase::Pressed, [](const Input::Event& e) {
                std::cout << "[InputProbe] game cancelled=" << e.cancelled << "\n";
            }));
        inputProbeConnections_.push_back(GetInput().Subscribe(inputProbeScope_, Input::Context::UI,
            "Role.Input.UI", Input::Phase::Pressed, [](const Input::Event& e) {
                std::cout << "[InputProbe] ui cancelled=" << e.cancelled << "\n";
            }));
        std::cout << "[InputProbe] bound K in Game/UI, priorities=10/20 scope=enabled\n";
    }
    if (std::getenv("TB_TRACE_DEBUG_PROBE") && !units_.empty()) {
        const auto* target = GetScene().FindComponent(units_.front());
        const auto position = target ? const_cast<GameComponent*>(target)->GetWorldPosition() : glm::vec3(0.0f);
        const Trace::Ray ray{{position.x, 3.0f, position.z}, {0, -1, 0}, 8.0f};
        const auto result = GetScene().Raycast(ray);
        DebugDrawTrace(ray, result, 5.0f);
        const auto normal = result.hit ? result.hit->normal : glm::vec3(0.0f);
        std::cout << "[TraceDebugProbe] status=" << Trace::StatusName(result.status)
                  << " hit=" << (result.hit ? result.hit->componentId : 0)
                  << " point=" << (result.hit ? result.hit->position.x : 0.0f) << ","
                  << (result.hit ? result.hit->position.y : 0.0f) << ","
                  << (result.hit ? result.hit->position.z : 0.0f)
                  << " normal=" << normal.x << "," << normal.y << "," << normal.z
                  << " debugLines=" << GetDebugTraceLineCount() << "\n";
    }
    if (std::getenv("TB_CASCADE_PROBE")) {
        SetSceneLightingEnabled(true);
        SetShadowSettings(true, 100.0f);
        SetDirectionalLight(glm::normalize(glm::vec3(0.35f, -1.0f, 0.2f)), {1.0f, 0.98f, 0.92f}, 1.0f);
        if (auto* camera = GetPlayer()) {
            camera->SetCameraMode(CameraMode::Free);
            camera->SetPosition({0.0f, 12.0f, -18.0f});
            camera->SetViewDirection(glm::normalize(glm::vec3(0.0f, -0.4f, 0.9f)), {0.0f, 1.0f, 0.0f});
        }
        std::cout << "[CascadeProbe] enabled directional cascades baseline camera=0,12,-18\n";
    }
    if (!units_.empty())
    {
        const auto* firstUnit = GetScene().FindComponent(units_.front());
        const auto unitPos = firstUnit ? firstUnit->GetLocalPosition() : glm::vec3(-6, 0.42f, -2);
        for (uint32_t channel : {1u, 2u})
        {
            Trace::Query filter;
            filter.channels = channel;
            const Trace::Ray probe{{unitPos.x, 3.0f, unitPos.z}, {0, -1, 0}, 8.0f};
            const auto result = GetScene().Raycast(probe, filter);
            const char* hitName = result.hit && result.hit->componentId == units_.front() ? "unit" :
                result.hit && result.hit->componentId == floor_ ? "ground" : Trace::StatusName(result.status);
            std::cout << "[TiredBomber] trace filter channel=" << channel << " -> " << hitName << "\n";
        }
        Trace::Query ignored;
        ignored.channels = 2u;
        const Trace::Ray sameRay{{unitPos.x, 3.0f, unitPos.z}, {0, -1, 0}, 8.0f};
        const auto beforeIgnore = GetScene().Raycast(sameRay, ignored);
        ignored.ignore.push_back(units_.front());
        const auto afterIgnore = GetScene().Raycast(sameRay, ignored);
        std::cout << "[TiredBomber] trace ignore channel=2 before="
                  << (beforeIgnore.hit ? beforeIgnore.hit->componentId : 0)
                  << " ignored=" << units_.front() << " after="
                  << (afterIgnore.hit ? afterIgnore.hit->componentId : 0)
                  << " status=" << Trace::StatusName(afterIgnore.status) << "\n";
    }

    Input::Binding dropBinding;
    dropBinding.action = "Bomber.Drop";
    dropBinding.context = Input::Context::Game;
    dropBinding.chord = {Keys::B};
    GetInput().Bind(std::move(dropBinding));
    inputConnections_.push_back(GetInput().Subscribe(inputScope_, Input::Context::Game,
        "Bomber.Drop", Input::Phase::Pressed, [this](const Input::Event& event) {
            if (!event.cancelled) {
                bombTimer_ = 0.0f;
                std::cout << "[TiredBomber] input B: request bomb drop\n";
            }
        }));
    Input::Binding saveBinding;
    saveBinding.action = "Bomber.Save";
    saveBinding.context = Input::Context::Game;
    saveBinding.chord = {Keys::F6};
    GetInput().Bind(std::move(saveBinding));
    inputConnections_.push_back(GetInput().Subscribe(inputScope_, Input::Context::Game,
        "Bomber.Save", Input::Phase::Pressed, [this](const Input::Event& event) {
            if (!event.cancelled) {
                const auto result = Saves().RequestSave("round-1");
                std::cout << "[TiredBomber] save request status=" << static_cast<int>(result.status) << "\n";
            }
        }));
    Input::Binding pauseBinding;
    pauseBinding.action = "Bomber.PauseToggle";
    pauseBinding.context = Input::Context::Game;
    pauseBinding.chord = {Keys::P};
    GetInput().Bind(std::move(pauseBinding));
    inputConnections_.push_back(GetInput().Subscribe(inputScope_, Input::Context::Game,
        "Bomber.PauseToggle", Input::Phase::Pressed, [this](const Input::Event& event) {
            if (!event.cancelled) {
                SetSimulationEnabled(!IsSimulationEnabled());
                std::cout << "[TiredBomber] input P: simulation=" << IsSimulationEnabled() << "\n";
            }
        }));

    auto ui = std::make_unique<UIComponent>();
    ui->AddPanel({UIAnchor::TopLeft, 12, 12, 460, 54}, {0.03f, 0.05f, 0.08f, 0.88f});
    auto* label = ui->AddLabel({UIAnchor::TopLeft, 24, 22, 438, 32}, "Tired Bomber | hits 0 | misses 0", {0.94f, 0.96f, 1.0f, 1.0f});
    label->Name = "score";
    hud_ = ui.get();
    Add(*this, "Bomber.HUD", std::move(ui), "BomberHud");
    if (std::getenv("TB_HIERARCHY_PROBE"))
    {
        auto parent = std::make_unique<ProjectComponent>();
        parent->SetPrimitive(MeshPrimitives::Cube());
        parent->SetPosition({10.0f, 0.0f, 0.0f});
        parent->SetRotation({0.0f, 0.0f, 90.0f});
        parent->SetScale({2.0f, 3.0f, 1.0f});
        const auto parentId = Add(*this, "RoleHierarchy.Parent", std::move(parent), "ProjectComponent");
        auto child = std::make_unique<ProjectComponent>();
        child->SetPrimitive(MeshPrimitives::Cube());
        child->SetPosition({1.0f, 0.0f, 0.0f});
        const auto childId = Add(*this, "RoleHierarchy.Child", std::move(child), "ProjectComponent");
        auto* parentComponent = GetScene().FindComponent(parentId);
        auto* childComponent = GetScene().FindComponent(childId);
        const bool attached = childComponent && childComponent->SetParent(parentComponent);
        const auto world = childComponent ? childComponent->GetWorldPosition() : glm::vec3(0.0f);
        const float error = glm::length(world - glm::vec3(10.0f, 2.0f, 0.0f));
        std::cout << "[HierarchyProbe] attached=" << attached << " parent=" << parentId
                  << " child=" << childId << " local=1,0,0 world=" << world.x << ","
                  << world.y << "," << world.z << " expected=10,2,0 error=" << error << "\n";
    }
    std::cout << "[TiredBomber] scene ready: 4 moving units, orthographic XZ camera, W/A/S/D controller, B drops a bomb\n";
}

void ProjectGame::SerializeSaveGame(SaveArchive& archive)
{
    Game::SerializeSaveGame(archive);
    archive.Section("TiredBomber.Round", [&](SaveArchive& state) {
        state.Version(1);
        state.Field("elapsed", elapsed_);
        state.Field("bombTimer", bombTimer_);
        state.Field("hits", hits_);
        state.Field("misses", misses_);
        uint64_t target = static_cast<uint64_t>(targetIndex_);
        state.Field("targetIndex", target);
        if (state.IsReading()) targetIndex_ = static_cast<size_t>(target);
        state.Field("sparksTimer", sparksTimer_);
        uint64_t unitCount = static_cast<uint64_t>(units_.size());
        state.Field("unitCount", unitCount);
        if (state.IsReading()) units_.resize(static_cast<size_t>(unitCount));
        uint64_t id = 0;
        auto reference = [&](const char* name, ComponentId& value) {
            id = value;
            state.Reference(name, id);
            if (state.IsReading()) value = id;
        };
        reference("player", player_);
        reference("floor", floor_);
        reference("activeBomb", activeBomb_);
        reference("sparks", sparksId_);
        reference("camera", cameraId_);
        for (size_t i = 0; i < static_cast<size_t>(unitCount); ++i) {
            const std::string key = "unit" + std::to_string(i);
            id = units_[i];
            state.Reference(key.c_str(), id);
            if (state.IsReading()) units_[i] = id;
        }
    });
}

void ProjectGame::OnSaveGameLoaded()
{
    diagnosticsCamera_ = nullptr;
    if (auto* camera = GetScene().FindComponentByName("Bomber.Camera"))
    {
        diagnosticsCamera_ = dynamic_cast<SceneCamera*>(camera);
        cameraId_ = camera->GetComponentId();
    }
    units_.clear();
    for (int i = 1; i <= 4; ++i)
        if (auto* unit = GetScene().FindComponentByName("Bomber.Unit." + std::to_string(i)))
            units_.push_back(unit->GetComponentId());
    if (auto* imported = GetScene().FindComponentByName("Bomber.ImportedTarget"))
        units_.push_back(imported->GetComponentId());
    hud_ = dynamic_cast<UIComponent*>(GetScene().FindComponentByName("Bomber.HUD"));
    markers_ = dynamic_cast<InstancedMeshComponent*>(GetScene().FindComponentByName("Bomber.Markers.LOD"));
    sparksId_ = 0;
    if (auto* sparks = GetScene().FindComponentByName("Bomber.HitSparks")) sparksId_ = sparks->GetComponentId();
    std::cout << "[TiredBomber] loaded round elapsed=" << elapsed_ << " hits=" << hits_
              << " misses=" << misses_ << " units=" << units_.size()
              << " activeBomb=" << activeBomb_ << " sparks=" << sparksId_
              << " sparksTimer=" << sparksTimer_ << " simulation=" << IsSimulationEnabled() << "\n";
    if (std::getenv("TB_CONTROLLER_ENABLE_AFTER_LOAD"))
        if (auto* controller = GetCharacterController()) {
            controller->SetEnabled(true);
            std::cout << "[ControllerProbe] explicitly enabled controller after load\n";
        }
    if (!IsSimulationEnabled() && std::getenv("TB_RESUME_AFTER_LOAD")) {
        SetSimulationEnabled(true);
        std::cout << "[TiredBomber] explicit resume after loaded pause in OnSaveGameLoaded\n";
    }
    if (std::getenv("TB_HIERARCHY_VERIFY")) {
        auto* parent = GetScene().FindComponentByName("RoleHierarchy.Parent");
        auto* child = GetScene().FindComponentByName("RoleHierarchy.Child");
        const auto world = child ? child->GetWorldPosition() : glm::vec3(0.0f);
        const auto parentId = child && child->GetParent() ? child->GetParent()->GetComponentId() : 0;
        const float error = glm::length(world - glm::vec3(10.0f, 2.0f, 0.0f));
        const bool custom = dynamic_cast<ProjectComponent*>(child) != nullptr;
        auto* imported = GetScene().FindComponentByName("Bomber.ImportedTarget");
        std::cout << "[HierarchyProbe] restored parent=" << (parent ? parent->GetComponentId() : 0)
                  << " child=" << (child ? child->GetComponentId() : 0) << " childParent=" << parentId
                  << " customProjectComponent=" << custom
                  << " importedModel=" << (imported ? imported->GetModelAsset() : std::string("missing"))
                  << " local=" << (child ? child->GetLocalPosition().x : 0.0f) << ","
                  << (child ? child->GetLocalPosition().y : 0.0f) << ","
                  << (child ? child->GetLocalPosition().z : 0.0f) << " world=" << world.x << ","
                  << world.y << "," << world.z << " error=" << error << "\n";
    }
    if (std::getenv("TB_SCENE_CODEC_VERIFY")) {
        auto* reference = dynamic_cast<BomberReferenceProbe*>(GetScene().FindComponentByName("RoleCodec.Owner"));
        auto* target = reference ? GetScene().FindComponent(reference->targetId) : nullptr;
        const bool targetType = dynamic_cast<ProjectComponent*>(target) != nullptr;
        std::cout << "[SceneCodecProbe] restored type=" << (reference ? "BomberReferenceProbe" : "missing")
                  << " version=" << (reference ? reference->codecVersion : 0)
                  << " score=" << (reference ? reference->score : 0.0f)
                  << " target=" << (reference ? reference->targetId : 0)
                  << " targetExists=" << (target != nullptr)
                  << " targetProjectComponent=" << targetType
                  << " data=" << (reference ? reference->SaveProjectData() : std::string("missing")) << "\n";
    }
}

void ProjectGame::DropBomb()
{
    if (activeBomb_) return;
    auto bomb = std::make_unique<BomberPiece>();
    bomb->SetPrimitive(MeshPrimitives::Sphere(0.38f, 16, 8));
    targetIndex_ = static_cast<size_t>((hits_ + misses_) % static_cast<int>(units_.size()));
    const auto* target = GetScene().FindComponent(units_[targetIndex_]);
    const auto targetPosition = target ? target->GetLocalPosition() : glm::vec3(0, 0.42f, 0);
    bomb->SetPosition({targetPosition.x, 10.0f, targetPosition.z});
    bomb->SetColor({0.96f, 0.78f, 0.10f, 1.0f});
    activeBomb_ = Add(*this, "Bomber.ActiveBomb", std::move(bomb), "BomberPiece");
    std::cout << "[TiredBomber] bomb released at t=" << elapsed_ << "s\n";
}

void ProjectGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    ++diagnosticsFrame_;
    if (std::getenv("TB_HUD_PROBE") && !hudProbeInitialized_) {
        hudProbeInitialized_ = true;
        auto* hud = GetProjectHud();
        const bool label = hud && hud->SetLabelText("status", "БОМБА ГОТОВА");
        const bool bar = hud && hud->SetBarFill("charge", 1.5f);
        if (hud) hud->BindClick("fire", [this] { ++hudProbeClicks_; std::cout << "[HUDProbe] fire-click count=" << hudProbeClicks_ << "\n"; });
        std::cout << "[HUDProbe] loaded=" << (hud != nullptr) << " label=" << label << " bar=" << bar
                  << " markupError=" << (hud ? hud->GetMarkupError() : std::string("missing")) << "\n";
        if (hud) {
            auto* labelElement = hud->Find<UILabel>("status"); auto* barElement = hud->Find<UIBar>("charge"); auto* buttonElement = hud->Find<UIButton>("fire");
            auto* trElement = hud->Find<UIPanel>("anchorTR"); auto* blElement = hud->Find<UIPanel>("anchorBL"); auto* centerElement = hud->Find<UIPanel>("anchorCenter");
            hud->GetRoot().Layout({0, 0, 640, 480});
            const auto labelBox = labelElement ? labelElement->GetBox() : UIBox{}; const auto buttonBox = buttonElement ? buttonElement->GetBox() : UIBox{};
            auto printBox = [](const UIBox& b) { std::cout << b.X << "," << b.Y << "," << b.Width << "," << b.Height; };
            std::cout << "[HUDProbe] 640x480 labelBox="; printBox(labelBox); std::cout << " fillRaw=" << (barElement ? barElement->Fill : 0.0f) << " buttonBox="; printBox(buttonBox); std::cout << " tr="; printBox(trElement ? trElement->GetBox() : UIBox{}); std::cout << " bl="; printBox(blElement ? blElement->GetBox() : UIBox{}); std::cout << " center="; printBox(centerElement ? centerElement->GetBox() : UIBox{}); std::cout << "\n";
            hud->GetRoot().Layout({0, 0, 1280, 720}); const auto resizedBox = buttonElement ? buttonElement->GetBox() : UIBox{};
            std::cout << "[HUDProbe] 1280x720 buttonBox="; printBox(resizedBox); std::cout << " tr="; printBox(trElement ? trElement->GetBox() : UIBox{}); std::cout << " bl="; printBox(blElement ? blElement->GetBox() : UIBox{}); std::cout << " center="; printBox(centerElement ? centerElement->GetBox() : UIBox{}); std::cout << "\n";
        }
    }
    if (std::getenv("TB_HUD_PROBE")) {
        if (diagnosticsFrame_ == 64) { OnWindowResized(1280, 720, false); std::cout << "[HUDProbe] requested game viewport resize 1280x720\n"; }
        if (diagnosticsFrame_ == 72) {
            const auto root = EngineModuleVfs::Resolve("/Game");
            if (!root.empty()) { RequestFrameDump(root + "/hud-project-1280.bmp"); std::cout << "[HUDProbe] requested 1280x720 frame dump\n"; }
        }
    }
    if (std::getenv("TB_COMPLETED_FRAME_PROBE"))
    {
        const auto& completed = GetCompletedFrameStats();
        std::cout << "[CompletedFrameProbe] preUpdate=" << diagnosticsFrame_
                  << " valid=" << completed.valid << " frameIndex=" << completed.frameIndex
                  << " drawCalls=" << completed.counters.DrawCalls << "\n";
        if (diagnosticsFrame_ == 4) OnWindowResized(0, 0, true);
        if (diagnosticsFrame_ == 9) OnWindowResized(640, 480, false);
    }
    if (std::getenv("TB_CASCADE_PROBE")) {
        const auto* spheres = GetCascadeSpheres();
        const auto* matrices = GetShadowMatrices();
        if (diagnosticsFrame_ >= 2 && diagnosticsFrame_ <= 5 && spheres && matrices) {
            const auto& sphere = spheres[0]; const auto& matrix = matrices[0];
            const char* sample = diagnosticsFrame_ == 2 ? "base" : diagnosticsFrame_ == 3 ? "subtexel" : diagnosticsFrame_ == 4 ? "large" : "return-subtexel";
            const auto& completed = GetCompletedFrameStats();
            const auto& params = GetShadowParams(); const auto& splits = GetCascadeSplits();
            const float mapSize = params.w > 0.0f ? 1.0f / params.w : 0.0f;
            const float texelWorld = mapSize > 0.0f ? 2.0f * sphere.w / mapSize : 0.0f;
            const auto cameraPos = GetPlayer() ? GetPlayer()->GetPosition() : glm::vec3(-999.0f);
            std::cout << "[CascadeProbe] sample=" << sample << " prevFrame=" << completed.frameIndex
                      << " camera=" << cameraPos.x << "," << cameraPos.y << "," << cameraPos.z
                      << " sphere=" << sphere.x << "," << sphere.y << "," << sphere.z << "," << sphere.w
                      << " matrix=" << matrix._11 << "," << matrix._12 << "," << matrix._13 << "," << matrix._14 << "," << matrix._21 << "," << matrix._22 << "," << matrix._23 << "," << matrix._24 << "," << matrix._31 << "," << matrix._32 << "," << matrix._33 << "," << matrix._34 << "," << matrix._41 << "," << matrix._42 << "," << matrix._43 << "," << matrix._44
                      << " shadowParams=" << params.x << "," << params.y << "," << params.z << "," << params.w
                      << " splits=" << splits.x << "," << splits.y << "," << splits.z << " texelWorldFromSphere=" << texelWorld << "\n";
        }
        if (auto* camera = GetPlayer()) {
            if (diagnosticsFrame_ == 1) camera->SetPosition({0.0f, 12.0f, -18.0f});
            if (diagnosticsFrame_ == 2) camera->SetPosition({0.001f, 12.0f, -18.0f});
            if (diagnosticsFrame_ == 3) camera->SetPosition({1.0f, 12.0f, -18.0f});
            if (diagnosticsFrame_ == 4) camera->SetPosition({0.001f, 12.0f, -18.0f});
        }
    }
    if (std::getenv("TB_INPUT_PROBE")) {
        if (diagnosticsFrame_ == 5) {
            GetInput().SetContextEnabled(Input::Context::UI, false);
            std::cout << "[InputProbe] pop UI context\n";
        }
        if (diagnosticsFrame_ == 10) {
            for (auto& connection : inputProbeConnections_) connection.Disconnect();
            inputProbeConnections_.clear();
            std::cout << "[InputProbe] disconnected subscriptions\n";
        }
        if (diagnosticsFrame_ == 15) {
            inputProbeScope_.SetEnabled(true);
            inputProbeConnections_.push_back(GetInput().Subscribe(inputProbeScope_, Input::Context::Game,
                "Role.Input.Game", Input::Phase::Pressed, [](const Input::Event& e) {
                    std::cout << "[InputProbe] game-rebound cancelled=" << e.cancelled << "\n";
                }));
            std::cout << "[InputProbe] rebound Game subscription\n";
        }
    }
    if (std::getenv("TB_TIME_PROBE"))
    {
        roleTimeProbeAccum_ += deltaTime;
        if (diagnosticsFrame_ == 1) { roleTimeProbeAccum_ = 0.0f; SetSimulationTimeScale(0.5f); }
        if (diagnosticsFrame_ == 21)
        {
            std::cout << "[TimeProbe] scale=0.5 frames=20 simDelta=" << roleTimeProbeAccum_ << "\n";
            roleTimeProbeAccum_ = 0.0f;
            SetSimulationTimeScale(2.0f);
        }
        if (diagnosticsFrame_ == 41)
        {
            std::cout << "[TimeProbe] scale=2 frames=20 simDelta=" << roleTimeProbeAccum_
                      << " ratio=" << (roleTimeProbeAccum_ / 0.1666667f) << " pauseNow\n";
            SetSimulationEnabled(false);
        }
    }
    if (gAudioProbe3D && !gAudioProbeFirstFrameLogged)
    {
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        const double elapsedMs = gAudioProbeFrequency.QuadPart ?
            (1000.0 * (now.QuadPart - gAudioProbeStarted.QuadPart) / gAudioProbeFrequency.QuadPart) : 0.0;
        std::cout << "[AudioProbe] firstPreUpdateQpc=" << now.QuadPart
                  << " firstPreUpdateMs=" << elapsedMs << " frame=" << diagnosticsFrame_ << "\n";
        gAudioProbeFirstFrameLogged = true;
    }
    if (gRoleProbePending && diagnosticsFrame_ == 10)
    {
        const auto* player = GetPlayer();
        const auto p = player ? player->GetPosition() : glm::vec3(0.0f);
        const bool isNear = gRoleProbeMode == "play3d-near-warm";
        audio::AudioEmitter3D emitter;
        emitter.position = {p.x, p.y, p.z + (isNear ? 2.0f : 2000.0f)};
        emitter.Commit(1);
        QueryPerformanceFrequency(&gAudioProbeFrequency);
        auto* audioEngine = GetAudioEngine();
        const bool accepted = audioEngine && audioEngine->Play3D(
            gRoleProbeSound, emitter, audio::AudioEngine::PlayOptions{0.7f, 0.0f, true});
        QueryPerformanceCounter(&gAudioProbeStarted);
        gAudioProbe3D = accepted;
        gRoleProbePending = false;
        std::cout << "[AudioProbe] warmStart frame=" << diagnosticsFrame_
                  << " player=" << p.x << "," << p.y << "," << p.z
                  << " emitter=" << emitter.position.x << "," << emitter.position.y << "," << emitter.position.z
                  << " accepted=" << accepted << " playReturnQpc=" << gAudioProbeStarted.QuadPart
                  << " frequency=" << gAudioProbeFrequency.QuadPart << "\n";
    }
    if (gRoleAudioStream.IsValid() && diagnosticsFrame_ == 180)
    {
        if (auto* audioEngine = GetAudioEngine()) audioEngine->StopStream(gRoleAudioStream);
        std::cout << "[AudioProbe] stream stopped at frame=" << diagnosticsFrame_ << "\n";
        gRoleAudioStream = {};
    }
    if (std::getenv("TB_RESIZE_PROBE"))
    {
        if (diagnosticsFrame_ == 5) OnWindowResized(640, 480, false);
        if (diagnosticsFrame_ == 25) OnWindowResized(320, 240, false);
        if (diagnosticsFrame_ == 45) OnWindowResized(1000, 500, false);
        if (diagnosticsFrame_ == 20 || diagnosticsFrame_ == 40 || diagnosticsFrame_ == 60)
        {
            const auto* display = GetDisplay();
            std::cout << "[ResizeProbe] frame=" << diagnosticsFrame_
                      << " display=" << (display ? display->GetWidth() : 0) << "x"
                      << (display ? display->GetHeight() : 0) << " completed="
                      << GetCompletedFrameStats().frameIndex << "\n";
            if (const char* root = std::getenv("TB_EVIDENCE_DIR"))
                RequestFrameDump(std::string(root) + "/resize-" + std::to_string(diagnosticsFrame_) + ".bmp");
        }
    }
    if (std::getenv("TB_PARTICLE_COMPARE") && diagnosticsFrame_ == 145)
        RequestFrameDump(EngineModuleVfs::Resolve("/Game") + "/particle-late-collision.bmp");
    if (std::getenv("TB_HOTKEY_INPUT_PROBE")) {
        if (diagnosticsFrame_ == 2) GetInput().SetKey(Keys::F7, true);
        else if (diagnosticsFrame_ == 3) GetInput().SetKey(Keys::F7, false);
        else if (diagnosticsFrame_ == 4)
            std::cout << "[HotkeyInputProbe] after F7 hide visible=" << IsRenderDiagnosticsVisible() << "\n";
        else if (diagnosticsFrame_ == 5) GetInput().SetKey(Keys::F7, true);
        else if (diagnosticsFrame_ == 6) GetInput().SetKey(Keys::F7, false);
        else if (diagnosticsFrame_ == 7)
            std::cout << "[HotkeyInputProbe] after F7 show visible=" << IsRenderDiagnosticsVisible() << "\n";
        else if (diagnosticsFrame_ == 8) GetInput().SetKey(Keys::F8, true);
        else if (diagnosticsFrame_ == 9) GetInput().SetKey(Keys::F8, false);
        else if (diagnosticsFrame_ == 10) {
            std::cout << "[HotkeyInputProbe] after F8 visible=" << IsRenderDiagnosticsVisible() << "\n";
            for (const auto& line : GetRenderDiagnosticsLines())
                if (line.find("Component") != std::string::npos)
                    std::cout << "[HotkeyInputProbe] " << line << "\n";
        }
    }
    if (std::getenv("TB_CONTROLLER_PROBE"))
    {
        auto* controller = GetCharacterController();
        if (diagnosticsFrame_ == 3 && controller && !std::getenv("TB_CONTROLLER_LOAD"))
        {
            controller->SetEnabled(false);
            const auto request = Saves().RequestSave("controller-before");
            std::cout << "[ControllerProbe] disabled and saved status=" << static_cast<int>(request.status) << "\n";
        }
        if (diagnosticsFrame_ == 4 || (diagnosticsFrame_ >= 5 && diagnosticsFrame_ <= 10))
        {
            GetInput().SetKey(Keys::W, true);
            GetInput().SetKey(Keys::RightButton, true);
            GetInput().AddPointer(60.0f, 0.0f);
        }
        if (diagnosticsFrame_ == 12)
        {
            GetInput().SetKey(Keys::W, false);
            GetInput().SetKey(Keys::RightButton, false);
        }
        if (diagnosticsFrame_ == 20)
        {
            auto* character = GetScene().FindComponentByName("Bomber.Player");
            const auto position = character ? character->GetWorldPosition() : glm::vec3(0.0f);
            const auto cameraPosition = GetPlayer()->GetPosition();
            std::cout << "[ControllerProbe] frame=20 sim=" << IsSimulationEnabled()
                      << " character=" << position.x << "," << position.y << "," << position.z
                      << " camera=" << cameraPosition.x << "," << cameraPosition.y << "," << cameraPosition.z
                      << " cameraYaw=" << GetPlayer()->GetYaw() << "\n";
            if (std::getenv("TB_CONTROLLER_LOAD"))
            {
                const auto request = Saves().RequestSave("controller-after");
                std::cout << "[ControllerProbe] post-load input save status=" << static_cast<int>(request.status) << "\n";
            }
        }
    }
    if (const char* frameText = std::getenv("TB_SAVE_FRAME"); frameText && diagnosticsFrame_ == std::atoi(frameText)) {
        if (std::getenv("TB_PAUSE_BEFORE_SAVE")) SetSimulationEnabled(false);
        const auto result = Saves().RequestSave("round-1");
        std::cout << "[TiredBomber] save at frame=" << diagnosticsFrame_ << " status=" << static_cast<int>(result.status)
                  << " hits=" << hits_ << " misses=" << misses_ << " elapsed=" << elapsed_
                  << " sim=" << IsSimulationEnabled() << "\n";
    }
    if (WasLoadedFromSaveGame() && std::getenv("TB_RESUME_AFTER_LOAD") && diagnosticsFrame_ == 30) {
        SetSimulationEnabled(true);
        std::cout << "[TiredBomber] explicit resume after loaded pause at frame=30\n";
    }
    if (diagnosticsFrame_ == 101) {
        const auto& result = Saves().LastResult();
        std::cout << "[TiredBomber] save completion status=" << static_cast<int>(result.status)
                  << " ticket=" << result.ticket << " message=" << result.message << "\n";
    }
    if ((diagnosticsFrame_ == 30 || diagnosticsFrame_ == 120) && std::getenv("TB_SKELETAL_PROBE")) {
        if (auto* skeletal = dynamic_cast<SkeletalMeshComponent*>(GetScene().FindComponentByName("Bomber.SkeletalProbe"))) {
            const auto& model = skeletal->GetModel();
            std::cout << "[SkeletalProbe] frame=" << diagnosticsFrame_ << " animationTime=" << skeletal->GetAnimationTime()
                      << " clip=" << skeletal->GetClipIndex()
                      << " bones=" << (model ? model->ModelSkeleton.Bones.size() : 0) << "\n";
        }
    }
    if (!IsSimulationEnabled()) return;
    const auto gameRoot = EngineModuleVfs::Resolve("/Game");
    if (diagnosticsFrame_ == 60 && !gameRoot.empty())
        RequestFrameDump(gameRoot + (std::getenv("TB_HUD_PROBE") ? "/hud-project-frame.bmp" : (std::getenv("TB_SKELETAL_PROBE") ? "/skeletal-frame.bmp" :
            (std::getenv("TB_INSTANCE_FALLBACK_PROBE") ? "/instance-fallback-3colors.bmp" : "/lod-camera-24.bmp"))));
    if (diagnosticsFrame_ == 80 && !gameRoot.empty() && std::getenv("TB_TRACE_DEBUG_PROBE"))
    {
        if (activeBomb_) {
            if (auto* bomb = GetScene().FindComponent(activeBomb_)) {
                const Trace::Ray longRay{bomb->GetWorldPosition(), {0.0f, -1.0f, 0.0f}, 20.0f};
                Trace::Query longQuery;
                longQuery.geometry = Trace::Geometry::Exact;
                longQuery.ignore.push_back(activeBomb_);
                const auto hit = GetScene().Raycast(longRay, longQuery);
                DebugDrawTrace(longRay, hit, 2.0f);
                std::cout << "[TraceDebugProbe] ray status=" << Trace::StatusName(hit.status)
                          << " hit=" << static_cast<bool>(hit)
                          << " distance=" << (hit.hit ? hit.hit->distance : 0.0f)
                          << " debugLines=" << GetDebugTraceLineCount() << "\n";
            }
        }
        RequestFrameDump(gameRoot + "/trace-debug-active-bomb.bmp");
        std::cout << "[TraceDebugProbe] requested real frame dump while bomb ray is active\n";
    }
    if (diagnosticsFrame_ == 90 && diagnosticsCamera_ && !std::getenv("TB_SKELETAL_PROBE"))
    {
        diagnosticsCamera_->SetPosition({0.0f, 8.0f, 0.0f});
        std::cout << "[TiredBomber] diagnostics camera moved to Y=8 at frame " << diagnosticsFrame_ << "\n";
    }
    if (diagnosticsFrame_ == 130 && diagnosticsCamera_ && !std::getenv("TB_SKELETAL_PROBE"))
    {
        diagnosticsCamera_->SetPosition({0.0f, 24.0f, 0.0f});
        std::cout << "[TiredBomber] screen-height probe same orthoHeight=24 at camera Y=24\n";
    }
    if (diagnosticsFrame_ == 170 && diagnosticsCamera_ && !std::getenv("TB_SKELETAL_PROBE"))
    {
        diagnosticsCamera_->orthoHeight = 8.0f;
        std::cout << "[TiredBomber] screen-height probe zoom orthoHeight=8 at camera Y=24\n";
    }
    if (diagnosticsFrame_ == 110)
        std::cout << "[TiredBomber] completed-frame snapshot after near-camera move\n";
    if (diagnosticsFrame_ == 110 && !gameRoot.empty())
        RequestFrameDump(gameRoot + "/lod-camera-8.bmp");
    if (diagnosticsFrame_ == 150 && !gameRoot.empty())
        RequestFrameDump(gameRoot + "/lod-screenheight-ortho24-y24.bmp");
    if (diagnosticsFrame_ == 190 && !gameRoot.empty())
        RequestFrameDump(gameRoot + "/lod-screenheight-zoom8.bmp");
    if (diagnosticsFrame_ == 60 || diagnosticsFrame_ == 110 || diagnosticsFrame_ == 150 || diagnosticsFrame_ == 190)
    {
        const auto& completed = GetCompletedFrameStats();
        std::cout << "[TiredBomber] completed frame valid=" << completed.valid
                  << " index=" << completed.frameIndex << " draws=" << completed.counters.DrawCalls
                  << " instances=" << completed.counters.InstancesDrawn
                  << " visible=" << completed.counters.VisibleObjects
                  << " culled=" << completed.counters.CulledObjects << "\n";
        for (const auto& lod : completed.lodComponents)
        {
            std::cout << "[TiredBomber] LOD component=" << lod.name << " frame=" << lod.frameIndex
                      << " prepared=" << lod.prepared << " drawn=" << lod.drawn << "/" << lod.total
                      << " hidden=" << lod.hidden << " distance=" << lod.distanceCulled
                      << " frustum=" << lod.frustumCulled << " camera=";
            if (lod.hasDistance) std::cout << lod.minDistance << ".." << lod.maxDistance;
            else std::cout << "none";
            std::cout << " thresholds=";
            for (float threshold : lod.thresholds) std::cout << threshold << ",";
            std::cout << " perLOD=";
            for (uint32_t count : lod.perLod) std::cout << count << ",";
            std::cout << "\n";
        }
        for (const auto& line : GetRenderDiagnosticsLines()) std::cout << "[LOD panel] " << line << "\n";
    }
    elapsed_ += deltaTime;
    if (!sessionObserved_)
    {
        sessionObserved_ = true;
        std::cout << "[TiredBomber] GetSubsystem after startup="
                  << (GetSubsystem<BomberSessionDiagnostics>() ? "available" : "null") << "\n";
    }
    diagnosticsTimer_ += deltaTime;
    if (diagnosticsTimer_ >= 5.0f)
    {
        diagnosticsTimer_ = 0.0f;
        const auto& stats = GetFrameStats();
        std::cout << "[TiredBomber] frame stats draws=" << stats.DrawCalls
                  << " instances=" << stats.InstancesDrawn << " visible=" << stats.VisibleObjects
                  << " culled=" << stats.CulledObjects << " adapter=" << GetAdapterName() << "\n";
    }
    playerLogTimer_ += deltaTime;
    if (playerLogTimer_ >= 1.0f)
    {
        playerLogTimer_ = 0.0f;
        const auto* player = GetScene().FindComponent(player_);
        if (player)
        {
            const auto position = player->GetLocalPosition();
            std::cout << "[TiredBomber] player position x=" << position.x
                      << " y=" << position.y << " z=" << position.z
                      << " t=" << elapsed_ << "s\n";
        }
    }
    bombTimer_ -= deltaTime;
    for (size_t i = 0; i < units_.size(); ++i)
    {
        auto* unit = GetScene().FindComponent(units_[i]);
        if (!unit) continue;
        const float phase = elapsed_ * (0.8f + 0.12f * float(i)) + float(i) * 1.4f;
        unit->SetPosition({-6.0f + float(i) * 4.0f + std::sin(phase) * 1.35f,
                           0.42f, (i % 2 ? 2.0f : -2.0f) + std::cos(phase * 0.7f) * 0.55f});
    }
    if (!activeBomb_ && bombTimer_ <= 0.0f)
    {
        DropBomb();
        bombTimer_ = 2.8f;
    }
    auto* bomb = activeBomb_ ? GetScene().FindComponent(activeBomb_) : nullptr;
    if (bomb)
    {
        auto pos = bomb->GetLocalPosition();
        pos.y -= 9.0f * deltaTime;
        if (pos.y > 3.0f && targetIndex_ < units_.size())
            if (auto* target = GetScene().FindComponent(units_[targetIndex_]))
            {
                const auto targetPosition = target->GetLocalPosition();
                pos.x = targetPosition.x;
                pos.z = targetPosition.z;
            }
        bomb->SetPosition(pos);
        Trace::Ray ray{pos, {0.0f, -1.0f, 0.0f}, 1.0f};
        Trace::Query query;
        query.geometry = Trace::Geometry::Exact;
        query.ignore.push_back(activeBomb_);
        const auto result = GetScene().Raycast(ray, query);
        DebugDrawTrace(ray, result, 0.15f);
        if (result)
        {
            const auto hitId = result.hit->componentId;
            const bool hitUnit = std::find(units_.begin(), units_.end(), hitId) != units_.end();
            if (hitUnit)
            {
                ++hits_;
                std::cout << "[TiredBomber] HIT unit id=" << hitId << " t=" << elapsed_
                          << "s trace=" << Trace::StatusName(result.status)
                          << " queries=" << GetScene().GetTraceStatistics().queries
                          << " triangles=" << GetScene().GetTraceStatistics().triangles << "\n";
                pendingHitDumpFrames_ = 3;
                if (!sparksId_)
                {
                    auto sparks = std::make_unique<BomberSparks>(result.hit->position,
                        glm::vec3(0), glm::vec3(1), glm::vec4(1.0f, 0.5f, 0.12f, 0.95f));
                    sparks->SetParticleCount(256);
                    sparks->SetBaseLifetime(0.9f);
                    sparks->SetSpreadRadius(0.45f);
                    sparks->SetRadialSpeed(3.0f);
                    sparks->SetBaseVelocity({0.0f, 2.2f, 0.0f});
                    sparks->SetAcceleration({0.0f, -2.0f, 0.0f});
                    sparks->SetMaxDistance(5.0f);
                    sparks->SetBaseSize(std::getenv("TB_PARTICLE_COMPARE") ? 0.55f : 0.12f);
                    sparks->SetDepthCollision(true);
                    sparks->SetDepthCollisionResponse(0.05f, 0.25f, 0.2f);
                    sparks->SetSortingEnabled(true);
                    if (const char* variant = std::getenv("TB_PARTICLE_VARIANT")) {
                        if (std::string(variant) == "no-collision") sparks->SetDepthCollision(false);
                        if (std::string(variant) == "no-sort") sparks->SetSortingEnabled(false);
                        std::cout << "[ParticleCompare] variant=" << variant << " collision="
                                  << (std::string(variant) != "no-collision") << " sorting="
                                  << (std::string(variant) != "no-sort") << "\n";
                    }
                    sparksId_ = Add(*this, "Bomber.HitSparks", std::move(sparks), "BomberSparks");
                    sparksTimer_ = 1.4f;
                    std::cout << "[TiredBomber] GPU spark burst started (256, depth collision, sorting)\n";
                }
            }
            else if (hitId == floor_)
            {
                ++misses_;
                std::cout << "[TiredBomber] MISS floor t=" << elapsed_ << "s trace=" << Trace::StatusName(result.status) << "\n";
            }
            GetScene().RemoveComponent(activeBomb_);
            activeBomb_ = 0;
        }
    }
    if (sparksId_)
    {
        sparksTimer_ -= deltaTime;
        if (sparksTimer_ <= 0.0f)
        {
            GetScene().RemoveComponent(sparksId_);
            sparksId_ = 0;
        }
    }
    if (markers_ && markers_->GetDrawnInstances() != lastDrawnMarkers_)
    {
        lastDrawnMarkers_ = markers_->GetDrawnInstances();
        std::cout << "[TiredBomber] marker instances drawn=" << lastDrawnMarkers_
                  << " total=" << markers_->GetMeshInstanceCount() << " LODs=" << markers_->GetLodCount();
        for (int lod = 0; lod < markers_->GetLodCount(); ++lod)
            std::cout << " lod" << lod << "=" << markers_->GetDrawnInstancesForLod(lod);
        std::cout << "\n";
    }
    if (hud_)
        hud_->SetLabelText("score", "Tired Bomber | hits " + std::to_string(hits_) + " | misses " + std::to_string(misses_));
    if (pendingHitDumpFrames_ >= 0 && --pendingHitDumpFrames_ == 0)
    {
        const auto gameRoot = EngineModuleVfs::Resolve("/Game");
        if (!gameRoot.empty())
            RequestFrameDump(gameRoot + "/tired-bomber-hit-" + std::to_string(hits_) + ".bmp");
        pendingHitDumpFrames_ = -1;
    }
}










