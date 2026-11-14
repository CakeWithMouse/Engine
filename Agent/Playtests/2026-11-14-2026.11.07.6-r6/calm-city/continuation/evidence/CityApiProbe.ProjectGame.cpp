#include "ProjectGame.h"
#include "RotatingCube.h"
#include <Engine/Components/Character.h>
#include <Engine/Components/Light/DirectionalLightComponent.h>
#include <Engine/Components/Light/CubeMap.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/Sound/WavDecoder.h>
#include <Engine/Sound/AudioEngine.h>
#include <Engine/Sound/Audio3D.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::filesystem::path ProbeRoot() {
    if (const char* overridePath = std::getenv("CITY_PROBE_OUTPUT")) return std::filesystem::u8path(overridePath);
    return std::filesystem::temp_directory_path() / "PainEngine-CalmCityContinuation";
}
void Record(const std::string& line) {
    const auto dir = ProbeRoot() / "SaveData";
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "ContinuationProbe.log", std::ios::app);
    out << line << '\n';
    if (!out) throw std::runtime_error("Cannot write continuation probe log");
}
void Require(bool condition, const char* name) {
    Record(std::string(name) + (condition ? "=PASS" : "=FAIL"));
    if (!condition) throw std::runtime_error(std::string("Probe assertion failed: ") + name);
}
}

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    const auto log = ProbeRoot() / "SaveData/ContinuationProbe.log";
    std::filesystem::create_directories(log.parent_path());
    std::ofstream(log, std::ios::trunc).close();

    auto& settings = GetSettings();
    settings.SetVSync(false);
    settings.SetRenderingType(Deffered);
    settings.SetAntialiasing(1);
    Require(!settings.IsVSyncEnabled() && settings.GetRenderingType() == Deffered && settings.GetAntialiasing() == 1,
            "game-settings-roundtrip");

    // A designer-sized HUD button: two pointer samples (press, release) must produce one click.
    UIElement hudRoot;
    hudRoot.Layout({0, 0, 100, 80});
    auto button = std::make_unique<UIElement>();
    button->Rect = {UIAnchor::TopLeft, 10, 10, 30, 20};
    button->bInteractive = true;
    int clicks = 0;
    button->OnClick = [&] { ++clicks; };
    auto* buttonRaw = hudRoot.Add(std::move(button));
    hudRoot.Layout({0, 0, 100, 80});
    UIInputRouter router;
    router.Update(hudRoot, {15, 15, true});
    router.Update(hudRoot, {15, 15, false});
    Require(clicks == 1 && buttonRaw->IsHovered(), "hud-click-release");

    // Public VFS object: mount, write, read, and enumerate one private project file.
const std::string resolvedWav = EngineModuleVfs::Resolve("/Game/Audio/probe.wav");
    Require(!resolvedWav.empty(), "host-vfs-resolve-audio");
    Record(std::string("host-vfs-resolve-audio=") + resolvedWav);
    const auto content = std::filesystem::u8path(resolvedWav).parent_path().parent_path();
    VirtualFileSystem vfs;
    vfs.Mount("/Game", content);
    Require(vfs.WriteText("/Game/continuation-vfs.txt", "calm-city\n"), "vfs-write");
    Require(vfs.ReadText("/Game/continuation-vfs.txt") == "calm-city\n", "vfs-read-after-write");
    Require(vfs.GetFileInfo("/Game/continuation-vfs.txt").has_value(), "vfs-stat");
    Require(vfs.Unmount("/Game"), "vfs-unmount");

    // The public mesh helpers produce a lower-triangle far LOD and valid tangents.
    auto sphere = MeshPrimitives::Sphere(0.6f, 24, 12);
    MeshSimplifySettings simplify;
    simplify.TargetRatio = 0.45f;
    MeshSimplifyResult simplifiedInfo;
    auto simplified = SimplifyMesh(sphere, simplify, &simplifiedInfo);
    Require(!sphere.Indices.empty() && simplifiedInfo.ResultTriangles < simplifiedInfo.SourceTriangles &&
            !simplified.Indices.empty(), "mesh-tools-simplify-lod");

    auto& scene = GetScene();
auto cameraTyped = std::make_unique<SceneCamera>();
    const bool horizonCamera = std::getenv("CITY_PROBE_CAMERA") && std::string(std::getenv("CITY_PROBE_CAMERA")) == "horizon";
    cameraTyped->orthographic = !horizonCamera;
    cameraTyped->viewPlane = horizonCamera ? 0 : 2;
    cameraTyped->orthoHeight = 12.0f;
    cameraTyped->SetPosition(horizonCamera ? glm::vec3(0.0f, 3.0f, 12.0f) : glm::vec3(0.0f, 14.0f, 0.0f));
    cameraTyped->SetRotation(horizonCamera ? glm::vec3(-12.0f, 0.0f, 0.0f) : glm::vec3(-90.0f, 0.0f, 0.0f));
    std::unique_ptr<GameComponent> camera = std::move(cameraTyped);
    const auto cameraAdded = scene.AddComponent("Continuation Camera", camera, "Gameplay.Camera");
    Require(cameraAdded.result == SceneResult::Ok || cameraAdded.result == SceneResult::Deferred, "camera-orthographic-added");

    auto characterTyped = std::make_unique<Character>();
    characterTyped->SetPlayerControlled(true);
    characterTyped->standardControl = false;
    characterTyped->camera = cameraAdded.id;
    characterTyped->cameraSettings.orthographic = true;
    characterTyped->cameraSettings.viewPlane = 2;
    characterTyped->cameraSettings.orthoHeight = 12.0f;
    std::unique_ptr<GameComponent> character = std::move(characterTyped);
    const auto characterAdded = scene.AddComponent("Continuation Character", character, "Gameplay.Character");
    Require(characterAdded.result == SceneResult::Ok || characterAdded.result == SceneResult::Deferred, "character-added");
    auto* hero = dynamic_cast<Character*>(scene.FindComponentByName("Continuation Character"));
    Require(hero != nullptr, "character-resolved");
    const auto before = hero->GetLocalPosition();
    hero->Move({1.0f, 0.0f, 0.0f}, 0.25f);
    Require(std::abs(hero->GetLocalPosition().x - before.x - 1.25f) < 0.001f, "character-move-world");

    SetCharacterController(std::make_unique<CharacterController>(*this));
    GetCharacterController()->Possess(characterAdded.id, cameraAdded.id);
    Require(GetCharacterController()->GetCharacter() == hero &&
            GetCharacterController()->GetCharacterId() == characterAdded.id, "controller-possession");

    auto* sun = dynamic_cast<DirectionalLightComponent*>(scene.FindComponentByName("Continuation Sun"));
    if (!sun) {
        std::unique_ptr<GameComponent> sunComponent = std::make_unique<DirectionalLightComponent>();
        sunComponent->SetRotation({-45.0f, -35.0f, 0.0f});
        const auto sunAdded = scene.AddComponent("Continuation Sun", sunComponent, "Light.Directional");
        Require(sunAdded.result == SceneResult::Ok || sunAdded.result == SceneResult::Deferred, "directional-light-added");
        sun = dynamic_cast<DirectionalLightComponent*>(scene.FindComponentByName("Continuation Sun"));
    }
    Require(sun != nullptr, "directional-light-resolved");
    const bool dimLight = std::getenv("CITY_PROBE_LIGHT") && std::string(std::getenv("CITY_PROBE_LIGHT")) == "dim";
    const bool spaceSky = std::getenv("CITY_PROBE_SKY") && std::string(std::getenv("CITY_PROBE_SKY")) == "space";
    sun->LoadProjectData(dimLight
        ? R"({"version":1,"color":[0.35,0.35,0.4],"intensity":0.0,"enabled":true,"shadows":false,"shadowDistance":150})"
        : R"({"version":1,"color":[1.0,0.9,0.75],"intensity":2.0,"enabled":true,"shadows":true,"shadowDistance":150})");
    sun->SetRotation(dimLight ? glm::vec3(-10.0f, 120.0f, 0.0f) : glm::vec3(-55.0f, -30.0f, 0.0f));
    Record(std::string("light-sun=") + (dimLight ? "dim" : "warm-strong"));
    auto* sky = dynamic_cast<SkyboxComponent*>(scene.FindComponentByName("Continuation Sky"));
    if (!sky) {
        std::unique_ptr<GameComponent> skyComponent = std::make_unique<SkyboxComponent>();
        const auto skyAdded = scene.AddComponent("Continuation Sky", skyComponent, "Environment.Sky");
        Require(skyAdded.result == SceneResult::Ok || skyAdded.result == SceneResult::Deferred, "skybox-added");
        sky = dynamic_cast<SkyboxComponent*>(scene.FindComponentByName("Continuation Sky"));
    }
Require(sky != nullptr, "skybox-resolved");
    const auto skyPreset = spaceSky ? CubeMapPreset::Space : CubeMapPreset::Studio;
    Require(CreateProceduralCubeMap("ContinuationSky", skyPreset, 64), "cubemap-created");
    auto* skyTexture = GetCubeMap("ContinuationSky");
    Require(skyTexture != nullptr, "cubemap-resolved");
    sky->SetCubeMap(skyTexture);
    Record(std::string("sky-cubemap=") + (spaceSky ? "Space" : "Studio"));

    auto hud = std::make_unique<UIComponent>();
    hud->AddButton({UIAnchor::TopLeft, 18.0f, 18.0f, 180.0f, 42.0f}, "Downtown", [this] {
        Record("hud-button-clicked");
    });
    std::unique_ptr<GameComponent> hudComponent = std::move(hud);
    const auto hudAdded = scene.AddComponent("Continuation HUD", hudComponent, "UI.ProjectHUD");
    Require(hudAdded.result == SceneResult::Ok || hudAdded.result == SceneResult::Deferred, "hud-component-added");

    auto particleTyped = std::make_unique<ParticleSystemComponent>();
    particleTyped->SetParticleCount(256);
    particleTyped->SetDepthCollision(true);
    particleTyped->SetDepthCollisionResponse(0.05f, 0.3f, 0.2f);
    particleTyped->SetBaseVelocity({0.0f, 1.0f, 0.0f});
    std::unique_ptr<GameComponent> particles = std::move(particleTyped);
    const auto particleAdded = scene.AddComponent("Continuation Particles", particles, "Particle.Sparks");
    Require(particleAdded.result == SceneResult::Ok || particleAdded.result == SceneResult::Deferred, "particle-collision-configured");

    // Decode a small project-owned WAV; playback calls record acceptance and handles, not audible QA.
    const auto wav = std::filesystem::u8path(resolvedWav);
    audio::WavDecoder decoder;
    if (decoder.Open(wav.wstring())) {
        const auto frameCount = decoder.GetLengthFrames();
        std::vector<uint8_t> bytes(static_cast<size_t>(frameCount * decoder.GetFormat().blockAlign));
        const auto readFrames = decoder.ReadFrames(bytes.data(), frameCount);
        Require(frameCount == 800 && readFrames == frameCount, "audio-decode-wav");
    } else {
        Record("audio-decode-wav=FAIL");
        throw std::runtime_error("WavDecoder rejected project fixture");
    }
    if (auto* audio = GetAudioEngine()) {
        const auto sound = audio->LoadSound(wav.wstring());
        Record(std::string("audio-load=") + (sound.IsValid() ? "valid" : "invalid"));
        const bool played2d = sound.IsValid() && audio->Play2D(sound);
        Record(std::string("audio-play2d=") + (played2d ? "accepted" : "rejected"));
        audio::AudioEmitter3D emitter;
        emitter.position = {2.0f, 0.0f, 0.0f};
        emitter.Commit();
        const bool played3d = sound.IsValid() && audio->Play3D(sound, emitter);
        Record(std::string("audio-play3d=") + (played3d ? "accepted" : "rejected"));
        audio::AudioEngine::StreamOptions streamOptions;
        streamOptions.loop = false;
        streamOptions.volume = 0.5f;
        streamOptions.framesPerBuffer = 256;
        const auto stream = audio->PlayStream(wav.wstring(), streamOptions);
        Record(std::string("audio-stream=") + (stream.IsValid() ? "accepted" : "rejected"));
        if (stream.IsValid()) {
            for (int i = 0; i < 3; ++i) { audio->Update(); Sleep(10); }
            audio->StopStream(stream);
            audio->Update();
            Record("audio-stream-updates=3-stop-issued");
        }
    } else {
        Record("audio-engine=null");
    }
    SetRenderDiagnosticsVisible(true);
}

void ProjectGame::PreUpdate(float deltaTime) {
    Game::PreUpdate(deltaTime);
    static bool frameDumped = false;
    if (!frameDumped && GetCompletedFrameStats().valid) {
        const auto ray = ScreenPointToRay(Trace::ViewportPoint::Pixel(320, 240));
        Record(std::string("trace-screen-ray=") + Trace::StatusName(ray.status));
        const auto hit = RaycastScreen(Trace::ViewportPoint::Pixel(320, 240));
        Record(std::string("trace-screen-query=") + Trace::StatusName(hit.status));
        if (hit.hit) Record(std::string("trace-screen-hit-id=") + std::to_string(hit.hit->componentId));
        const char* capture = std::getenv("CITY_PROBE_CAPTURE");
        const std::string captureName = capture && *capture ? capture : "lighting-default";
        const auto path = ProbeRoot() / "SaveData" / std::filesystem::u8path(captureName + ".bmp");
        RequestFrameDump(path.string());
        Record(std::string("frame-dump=") + path.string());
        frameDumped = true;
    }
}
void ProjectGame::TickComponents(float deltaTime) { Game::TickComponents(deltaTime); }














