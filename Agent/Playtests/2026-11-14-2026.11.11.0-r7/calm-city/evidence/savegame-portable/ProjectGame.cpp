#include "ProjectGame.h"
#include "CarMover.h"
#include "CalmScoreSubsystem.h"
#include "CalmLodRow.h"
#include <Engine/Components/Character.h>
#include <Engine/Components/Light/DirectionalLightComponent.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/Sound/Audio3D.h>
#include <stdexcept>
#include <filesystem>
#include <Engine/SaveGame/SaveGame.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <Windows.h>

namespace {
CarMover* FindTestCar(Scene& scene)
{
    for (const auto& entry : scene.GetComponents())
        if (auto* car = dynamic_cast<CarMover*>(entry.second.get())) return car;
    return nullptr;
}

std::filesystem::path ProjectSaveRoot();

void SaveTestLog(const std::string& line)
{
    const auto root = ProjectSaveRoot();
    std::filesystem::create_directories(root);
    std::ofstream log(root / "SaveGameSelfTest.log", std::ios::app);
    log << line << '\n';
    if (!log) throw std::runtime_error("Cannot write SaveGameSelfTest.log");
}

std::filesystem::path ProjectSaveRoot()
{
    // Resolve through the active /Game mount so an exported or relocated project
    // writes beside its own Content tree instead of the compile-time source path.
    const auto content = EngineModuleVfs::Resolve("/Game");
    if (content.empty()) throw std::runtime_error("Cannot resolve project content root from /Game");
    return std::filesystem::u8path(content).parent_path() / "SaveData";
}

uint64_t QpcNow100ns()
{
    LARGE_INTEGER qpc{}, frequency{};
    QueryPerformanceCounter(&qpc); QueryPerformanceFrequency(&frequency);
    return static_cast<uint64_t>((static_cast<long double>(qpc.QuadPart) * 10000000.0L) / frequency.QuadPart);
}
}

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    const auto saveRoot = ProjectSaveRoot();
    Saves().Configure("calm-city", saveRoot);
    if (const char* phase = std::getenv("CALM_SAVE_TEST")) {
        if (std::string(phase) == "save") saveTestMode_ = 1;
        else if (std::string(phase) == "load") saveTestMode_ = 2;
        else throw std::runtime_error("CALM_SAVE_TEST must be 'save' or 'load'");
        std::filesystem::create_directories(saveRoot);
        if (saveTestMode_ == 1) {
            std::ofstream log(saveRoot / "SaveGameSelfTest.log", std::ios::trunc);
            if (!log) throw std::runtime_error("Cannot initialize SaveGameSelfTest.log");
        }
    }
    if (const char* test = std::getenv("CALM_AUDIO_CASE")) {
        audioCase_ = test;
        auto* engine = GetAudioEngine();
        if (!engine) throw std::runtime_error("Audio probe: Game::GetAudioEngine returned null");
        const auto path = EngineModuleVfs::Resolve("/Game/Audio/Signal.wav");
        if (path.empty()) throw std::runtime_error("Audio probe: /Game/Audio/Signal.wav did not resolve");
        const auto root = ProjectSaveRoot();
        std::filesystem::create_directories(root);
        std::ofstream log(root / ("audio-" + audioCase_ + ".log"), std::ios::trunc);
        if (!log) throw std::runtime_error("Audio probe: cannot create its log");
        audioSound_ = engine->LoadSound(std::filesystem::u8path(path).wstring());
        if (!audioSound_.IsValid()) throw std::runtime_error("Audio probe: LoadSound rejected fixture");
        audio::AudioEngine::PlayOptions playOptions; playOptions.volume = 0.75f;
        bool accepted = false;
        float emitterDistance = -1.0f;
        if (audioCase_ == "2d") {
            accepted = engine->Play2D(audioSound_, playOptions);
        } else if (audioCase_ == "3d-near" || audioCase_ == "3d-far") {
            // Start only after the scene Player exists and has supplied the listener for ten frames.
            // The emitter is relative to that actual listener, not the project origin.
            audioEmitterDistance_ = audioCase_ == "3d-near" ? 2.0f : 20.0f;
            audioPlayPending_ = true;
        } else if (audioCase_ == "stream") {
            audio::AudioEngine::StreamOptions options; options.loop = true; options.volume = 0.75f; options.framesPerBuffer = 4096;
            audioStream_ = engine->PlayStream(std::filesystem::u8path(path).wstring(), options);
            accepted = audioStream_.IsValid();
        } else throw std::runtime_error("Audio probe case must be 2d, 3d-near, 3d-far or stream");
        if (!audioPlayPending_)
            log << "case=" << audioCase_ << " soundId=" << audioSound_.value << " accepted=" << accepted
                << " sample=/Game/Audio/Signal.wav volume=0.75 emitterDistance=" << emitterDistance
                << " playCallQpc100ns=" << QpcNow100ns() << "\n";
        if (!audioPlayPending_ && !accepted) throw std::runtime_error("Audio probe: play request was rejected");
    }
    // One full turn in four real seconds; the diagnostic scenario advances explicitly.
    SetSimulationTimeScale(saveTestMode_ ? 0.0f : 1.0f);
    auto& scene = GetScene();
    // An overview camera and a sun make the existing city readable at first Play.
    std::unique_ptr<GameComponent> camera = std::make_unique<SceneCamera>();
    camera->SetPosition({14.0f, 12.0f, 18.0f});
    camera->SetRotation({-28.0f, 38.0f, 0.0f});
    const auto cameraResult = scene.AddComponent("City overview", camera);
    if (cameraResult.result != SceneResult::Ok) throw std::runtime_error("Cannot create city camera");
    auto player = std::make_unique<Character>();
    player->SetPlayerControlled(true);
    player->standardControl = false;
    if (!audioCase_.empty()) { player->SetPosition({0.0f, 0.0f, 0.0f}); player->SetRotation({0.0f, 0.0f, 0.0f}); }
    player->camera = cameraResult.id;
    std::unique_ptr<GameComponent> playerComponent = std::move(player);
    if (scene.AddComponent("City viewer", playerComponent).result != SceneResult::Ok)
        throw std::runtime_error("Cannot create city viewer");
    bool hasSun = false;
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<DirectionalLightComponent*>(entry.second.get())) hasSun = true;
    if (!hasSun) {
        std::unique_ptr<GameComponent> sun = std::make_unique<DirectionalLightComponent>();
        sun->SetRotation({-50.0f, -30.0f, 0.0f});
        if (scene.AddComponent("City sun", sun, "Light.Directional").result != SceneResult::Ok)
            throw std::runtime_error("Cannot create city sun");
    }
    // Warm up one quiet block: lighter stone and a softer amber key light.
    if (auto* west = dynamic_cast<GameComponent*>(scene.FindComponentByName("West block"))) {
        west->SetMaterialAsset("/Game/Materials/WarmStone.mat");
        west->SetColor({0.72f, 0.58f, 0.39f, 1.0f});
    }
    if (auto* warm = dynamic_cast<PointLightComponent*>(scene.FindComponentByName("Warm Light"))) {
        warm->SetLightColor({1.0f, 0.76f, 0.48f});
        warm->SetIntensity(1.25f);
        warm->SetRadius(13.0f);
    }
    // A small row of street lamps is an actual instanced LOD target for the panel.
    bool hasLodProbe = false;
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<InstancedMeshComponent*>(entry.second.get())) hasLodProbe = true;
    if (!hasLodProbe) {
        std::unique_ptr<GameComponent> lamps = std::make_unique<CalmLodRow>();
        const auto added = scene.AddComponent("Street lamp LOD row", lamps, "CalmCity.LodRow");
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add street lamp LOD row");
        SetLodDiagnosticsTarget(added.id);
    }
    SetRenderDiagnosticsVisible(true);
    // A saved CarMover already supplies the example after loading/Play.
    bool hasCar = false;
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<CarMover*>(entry.second.get())) hasCar = true;
    if (!hasCar) {
        std::unique_ptr<GameComponent> cube = std::make_unique<CarMover>();
        cube->SetPosition({-5.6f, 0.55f, 0.0f});
        cube->SetScale({1.35f, 0.35f, 0.72f});
        std::string name = "Car mover";
        for (unsigned suffix = 2; scene.FindComponentByName(name); ++suffix)
            name = "Car mover " + std::to_string(suffix);
        const auto added = scene.AddComponent(name, cube, "CalmCity.CarMover");
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add the starter car");
    }
}

void ProjectGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    if (!instanceTestDone_ && std::getenv("CALM_INSTANCE_TEST")) {
        auto* row = dynamic_cast<CalmLodRow*>(GetScene().FindComponentByName("Street lamp LOD row"));
        if (!row || row->GetMeshInstanceCount() != 8)
            throw std::runtime_error("Instance probe expected the eight-lamp row");
        const auto before0 = row->GetMeshInstance(0).Position;
        const auto before1 = row->GetMeshInstance(1).Position;
        row->SetInstancePosition(0, {0.0f, 0.35f, 2.8f});
        row->SetInstanceVisible(1, false);
        const auto after0 = row->GetMeshInstance(0).Position;
        const bool afterVisible1 = row->GetMeshInstance(1).bVisible;
        const auto root = ProjectSaveRoot();
        std::ofstream log(root / "InstanceProbe.log", std::ios::trunc);
        log << "count=" << row->GetMeshInstanceCount()
            << " moved0=" << before0.x << ',' << before0.y << ',' << before0.z
            << "->" << after0.x << ',' << after0.y << ',' << after0.z
            << " hidden1=" << before1.x << ',' << before1.y << ',' << before1.z
            << " visible1=" << afterVisible1 << '\n';
        if (!log || after0.x != 0.0f || afterVisible1)
            throw std::runtime_error("Instance probe mutation did not persist in the component");
        instanceTestDone_ = true;
    }
    if (!audioCase_.empty()) {
        audioElapsed_ += deltaTime;
        if (audioPlayPending_ && ++audioWarmFrames_ >= 10) {
            auto* engine = GetAudioEngine();
            const auto* player = GetPlayer();
            if (!engine || !player) throw std::runtime_error("Audio probe: Player/listener is unavailable after warmup");
            const auto listener = player->GetPosition();
            audio::AudioEmitter3D emitter;
            emitter.position = {listener.x, listener.y, listener.z + audioEmitterDistance_};
            emitter.Commit(1);
            audio::AudioEngine::PlayOptions options; options.volume = 0.75f;
            const bool accepted = engine->Play3D(audioSound_, emitter, options);
            if (!accepted) throw std::runtime_error("Audio probe: Play3D rejected the warmed emitter");
            const auto root = ProjectSaveRoot();
            std::ofstream log(root / ("audio-" + audioCase_ + ".log"), std::ios::app);
            log << "case=" << audioCase_ << " soundId=" << audioSound_.value << " accepted=" << accepted
                << " listener=" << listener.x << ',' << listener.y << ',' << listener.z
                << " emitter=" << emitter.position.x << ',' << emitter.position.y << ',' << emitter.position.z
                << " actualDistance=" << audioEmitterDistance_ << " warmFrames=" << audioWarmFrames_
                << " playCallQpc100ns=" << QpcNow100ns() << "\n";
            audioPlayPending_ = false;
        }
        if (audioCase_ == "stream" && !audioStopped_ && audioElapsed_ >= 2.0f) {
            if (auto* engine = GetAudioEngine()) engine->StopStream(audioStream_);
            audioStopped_ = true;
            const auto root = ProjectSaveRoot();
            std::ofstream log(root / "audio-stream.log", std::ios::app);
            log << "stopRequestedAtSimulationSeconds=" << audioElapsed_ << " stopCallQpc100ns=" << QpcNow100ns() << "\n";
        }
    }
    if (!frameDumpRequested_ && std::getenv("CALM_FRAME_DUMP") && GetCompletedFrameStats().valid) {
        const auto root = ProjectSaveRoot();
        std::filesystem::create_directories(root);
        RequestFrameDump((root / "calm-city-lod.bmp").u8string());
        std::ofstream log(root / "LodPanel.log", std::ios::trunc);
        const auto& frame = GetCompletedFrameStats();
        log << "valid=" << frame.valid << " frame=" << frame.frameIndex
            << " drawCalls=" << frame.counters.DrawCalls << " instances=" << frame.counters.InstancesDrawn << '\n';
        if (auto* west = dynamic_cast<GameComponent*>(GetScene().FindComponentByName("West block")))
            log << "West block material=" << west->GetMaterialAsset()
                << " loaded=" << (west->GetMaterial() != nullptr) << '\n';
        for (const auto& line : GetRenderDiagnosticsLines()) log << line << '\n';
        frameDumpRequested_ = true;
    }
    if (!saveTestMode_ || saveTestStep_ >= 7) return;
    if (saveTestStep_ >= 3) {
        if (saveTestStep_ < 6) {
            ++saveTestStep_;
            return;
        }
        const auto root = ProjectSaveRoot();
        RequestFrameDump((root / (saveTestMode_ == 1 ? "save-load-same-process.bmp" : "save-load-new-process.bmp")).u8string());
        saveTestStep_ = 7;
        return;
    }
    auto* car = FindTestCar(GetScene());
    if (!car) throw std::runtime_error("SaveGame test cannot find CalmCar");
    if (saveTestMode_ == 1) {
        if (saveTestStep_ == 0) {
            auto* score = GetSubsystem<CalmScoreSubsystem>();
            if (!score) throw std::runtime_error("SaveGame test cannot find CalmScoreSubsystem");
            score->score = 42;
            car->Tick(10.0f); // Reach the right endpoint; route direction becomes left.
            saveTestX_ = car->GetLocalPosition().x;
            if (std::abs(saveTestX_ - 5.6f) > 0.001f)
                throw std::runtime_error("SaveGame test could not prepare the leftbound route");
            const auto request = Saves().RequestSave("manual-1");
            if (request.status != SaveGameResult::Status::Accepted)
                throw std::runtime_error("SaveGame test save request was not accepted: " + request.message);
            SaveTestLog("process=save request=accepted positionX=" + std::to_string(saveTestX_) + " route=left score=" + std::to_string(score->score));
            saveTestStep_ = 1;
        } else if (saveTestStep_ == 1) {
            if (Saves().LastResult().status != SaveGameResult::Status::Saved)
                throw std::runtime_error("SaveGame test did not finish saving: " + Saves().LastResult().message);
            car->Tick(1.0f);
            auto* score = GetSubsystem<CalmScoreSubsystem>();
            if (!score) throw std::runtime_error("SaveGame test lost CalmScoreSubsystem");
            score->score = 99;
            const float advancedX = car->GetLocalPosition().x;
            if (!(advancedX < saveTestX_)) throw std::runtime_error("Car did not advance after save");
            const auto request = Saves().RequestLoad("manual-1");
            if (request.status != SaveGameResult::Status::Accepted)
                throw std::runtime_error("SaveGame test load request was not accepted: " + request.message);
            SaveTestLog("process=save result=saved advancedX=" + std::to_string(advancedX) + " scoreMutated=" + std::to_string(score->score) + " request=load-accepted");
            saveTestStep_ = 2;
        } else {
            if (Saves().LastResult().status != SaveGameResult::Status::Loaded)
                throw std::runtime_error("SaveGame test did not finish loading: " + Saves().LastResult().message);
            car = FindTestCar(GetScene());
            if (!car) throw std::runtime_error("Loaded scene has no CalmCar");
            const float loadedX = car->GetLocalPosition().x;
            auto* score = GetSubsystem<CalmScoreSubsystem>();
            if (!score || score->score != 42) throw std::runtime_error("SaveGame test did not restore subsystem score");
            if (std::abs(loadedX - saveTestX_) > 0.001f)
                throw std::runtime_error("Loaded CalmCar position differs from the saved position");
            car->Tick(0.1f);
            const float continuedX = car->GetLocalPosition().x;
            if (!(continuedX < loadedX)) throw std::runtime_error("Loaded CalmCar did not continue its saved leftbound route");
            SaveTestLog("process=save result=loaded loadedX=" + std::to_string(loadedX) + " continuedX=" + std::to_string(continuedX) + " score=" + std::to_string(score->score) + " PASS");
            saveTestStep_ = 3;
        }
    } else {
        if (saveTestStep_ == 0) {
            const auto request = Saves().RequestLoad("manual-1");
            if (request.status != SaveGameResult::Status::Accepted)
                throw std::runtime_error("Cross-process load request was not accepted: " + request.message);
            SaveTestLog("process=new request=load-accepted");
            saveTestStep_ = 1;
        } else {
            if (Saves().LastResult().status != SaveGameResult::Status::Loaded)
                throw std::runtime_error("Cross-process load did not finish: " + Saves().LastResult().message);
            car = FindTestCar(GetScene());
            if (!car) throw std::runtime_error("Cross-process loaded scene has no CalmCar");
            const float loadedX = car->GetLocalPosition().x;
            auto* score = GetSubsystem<CalmScoreSubsystem>();
            if (!score || score->score != 42) throw std::runtime_error("Cross-process SaveGame did not restore subsystem score");
            if (std::abs(loadedX - 5.6f) > 0.001f)
                throw std::runtime_error("Cross-process CalmCar position is not the saved endpoint");
            car->Tick(0.1f);
            const float continuedX = car->GetLocalPosition().x;
            if (!(continuedX < loadedX)) throw std::runtime_error("Cross-process route did not continue leftbound");
            SaveTestLog("process=new result=loaded loadedX=" + std::to_string(loadedX) + " continuedX=" + std::to_string(continuedX) + " score=" + std::to_string(score->score) + " PASS");
            saveTestStep_ = 3;
        }
    }
}
void ProjectGame::TickComponents(float deltaTime) { Game::TickComponents(deltaTime); }
// Release playtest: preserve unsaved city edits across a rebuilt game module.



