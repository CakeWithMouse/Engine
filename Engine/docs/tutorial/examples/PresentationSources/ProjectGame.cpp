#include "ProjectGame.h"
#include "RotatingCube.h"
#include "CityEconomySubsystem.h"
#include "CityInteractionComponent.h"
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Trace.h>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    // One full turn in four real seconds, independent of the frame rate.
    SetSimulationTimeScale(1.0f);
    auto& scene = GetScene();
    Saves().Configure("curious-city-r2", std::filesystem::path(std::getenv("CURIOUS_CITY_SAVE_ROOT") ? std::getenv("CURIOUS_CITY_SAVE_ROOT") : ""));
    if (const char* path = std::getenv("CURIOUS_CITY_LOG")) {
        auto* ground = scene.FindComponentByName("Ground");
        if (ground) {
            const Trace::Ray ray{{5, 10, 5}, {0, -1, 0}, 100};
            Trace::Query any; any.channels = Trace::AllChannels; any.maxDistance = 100;
            const auto allHit = scene.Raycast(ray, any);
            const uint32_t oldChannels = ground->traceChannels;
            ground->traceChannels = 2;
            Trace::Query defaultChannel; defaultChannel.channels = Trace::DefaultChannel;
            defaultChannel.maxDistance = 100;
            const auto filtered = scene.Raycast(ray, defaultChannel);
            Trace::Query channelTwo = defaultChannel; channelTwo.channels = 2;
            const auto alternate = scene.Raycast(ray, channelTwo);
            Trace::Query ignoreGround = any; ignoreGround.ignore = {ground->GetComponentId()};
            const auto ignored = scene.Raycast(ray, ignoreGround);
            ground->traceChannels = oldChannels;
            DebugDrawTrace(ray, allHit, 5.0f);
            const size_t debugLines = GetDebugTraceLineCount();
            ClearDebugTraces();
            const size_t clearedLines = GetDebugTraceLineCount();
            std::ofstream log(std::filesystem::u8path(path), std::ios::app);
            if (log) log << "trace_filter_debug all=" << Trace::StatusName(allHit.status)
                         << " default=" << Trace::StatusName(filtered.status)
                         << " channel2=" << Trace::StatusName(alternate.status)
                         << " ignoreGround=" << Trace::StatusName(ignored.status)
                         << " debugLines=" << debugLines << " clearedLines=" << clearedLines << '\n';
        }
    }
    // Keep the starter cube, but always install city controls alongside it.
    if (scene.FindComponentByName("CityControls")) return;
    auto controls = std::make_unique<CityInteractionComponent>();
    std::unique_ptr<GameComponent> component = std::move(controls);
    const auto added = scene.AddComponent("CityControls", component, "CuriousCity.Interaction");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add city controls");
    if (const char* path = std::getenv("CURIOUS_CITY_LOG")) {
        const auto contentFile = EngineModuleVfs::Resolve("/Game/Config/city-settings.txt");
        std::ifstream contentInput(std::filesystem::u8path(contentFile));
        std::string contentLine;
        std::getline(contentInput, contentLine);
        if (contentInput || !contentLine.empty()) {
            std::ofstream log(std::filesystem::u8path(path), std::ios::app);
            if (log) log << "vfs_read path=/Game/Config/city-settings.txt value=" << contentLine << '\n';
        }
        for (const auto& entry : scene.GetComponents()) {
            auto* skeletal = dynamic_cast<SkeletalMeshComponent*>(entry.second.get());
            if (!skeletal) continue;
            const auto slots = skeletal->GetMaterialSlots();
            if (slots.empty()) continue;
            const uint32_t sourceId = slots.begin()->first;
            const std::string originalPath = slots.begin()->second;
            const std::string material = "/Game/Models/CesiumMan/Materials/Material_0.mat";
            const bool slotSet = skeletal->SetMaterialSlot(sourceId, material);
            const bool unknownSlotSet = skeletal->SetMaterialSlot(UINT32_MAX, material);
            const bool overrideSet = skeletal->SetMaterialAsset(material);
            const std::string overridePath = skeletal->GetMaterialAsset();
            const bool overrideCleared = skeletal->SetMaterialAsset("");
            const auto& restored = skeletal->GetMaterialSlots();
            std::ofstream log(std::filesystem::u8path(path), std::ios::app);
            if (log) log << "skeletal_material_api slots=" << slots.size()
                         << " source_id=" << sourceId << " original=" << originalPath
                         << " set_slot=" << slotSet << " invalid_slot=" << unknownSlotSet
                         << " set_override=" << overrideSet << " override=" << overridePath
                         << " clear_override=" << overrideCleared
                         << " restored=" << (restored.count(sourceId) ? restored.at(sourceId) : "missing") << '\n';
            break;
        }
    }
}

void ProjectGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    static unsigned dumpFrame = 0;
    static bool dumpRequested = false;
    const char* dumpPath = std::getenv("CURIOUS_CITY_FRAME_DUMP_PATH");
    const char* dumpAt = std::getenv("CURIOUS_CITY_FRAME_DUMP_AT");
    if (dumpPath && *dumpPath && !dumpRequested) {
        ++dumpFrame;
        if (dumpAt && dumpFrame >= static_cast<unsigned>(std::max(1, std::atoi(dumpAt)))) {
            RequestFrameDump(dumpPath);
            dumpRequested = true;
        }
    }
    const char* probe = std::getenv("CURIOUS_CITY_FRAME_STATS");
    if (!probe || !*probe || *probe == '0') return;
    static uint64_t previous = UINT64_MAX;
    static bool initial = false;
    const auto& frame = GetCompletedFrameStats();
    const char* path = std::getenv("CURIOUS_CITY_LOG");
    if (!path) return;
    std::ofstream log(std::filesystem::u8path(path), std::ios::app);
    if (!log) return;
    if (!frame.valid && !initial) {
        initial = true;
        log << "completed_frame valid=false before_first_render\n";
    }
    if (frame.valid && frame.frameIndex != previous) {
        previous = frame.frameIndex;
        log << "completed_frame valid=true index=" << frame.frameIndex
            << " draw_calls=" << frame.counters.DrawCalls
            << " visible=" << frame.counters.VisibleObjects
            << " culled=" << frame.counters.CulledObjects << '\n';
    }
}
void ProjectGame::TickComponents(float deltaTime)
{
    Game::TickComponents(deltaTime);
    const char* inputProbe = std::getenv("CURIOUS_CITY_INPUT_PROBE");
    if (inputProbe && *inputProbe && *inputProbe != '0') {
        static unsigned inputFrame = 0;
        ++inputFrame;
        const auto cursor = GetInput().GetCursorPosition();
        const char* path = std::getenv("CURIOUS_CITY_LOG");
        if (path) {
            std::ofstream log(std::filesystem::u8path(path), std::ios::app);
            if (log) log << "input_state frame=" << inputFrame
                         << " focused=" << GetInput().IsContextEnabled(Input::Context::Game)
                         << " left=" << GetInput().IsPhysicalKeyDown(Keys::LeftButton)
                         << " cursor_valid=" << cursor.valid << " x=" << cursor.x << " y=" << cursor.y
                         << " size=" << cursor.width << 'x' << cursor.height << '\n';
        }
    }
    const char* mode = std::getenv("CURIOUS_CITY_CYCLE");
    if (!mode || !*mode || *mode == '0') return;
    static unsigned frame = 0;
    ++frame;
    auto* economy = GetSubsystem<CityEconomySubsystem>();
    if (!economy) throw std::runtime_error("City economy subsystem missing");
    auto record = [&](const char* event) {
        const char* path = std::getenv("CURIOUS_CITY_LOG");
        if (!path) return;
        std::ofstream log(std::filesystem::u8path(path), std::ios::app);
        if (log) log << "cycle=" << mode << " frame=" << frame << " event=" << event
                     << " balance=" << economy->Balance() << " houses=" << economy->Houses()
                     << " save_status=" << static_cast<int>(Saves().LastResult().status) << '\n';
    };
    if (std::string(mode) == "first") {
        if (frame == 1) {
            if (!economy->BuildOneHouse(GetScene())) throw std::runtime_error("First house was rejected");
            const auto request = Saves().RequestSave("city");
            if (request.status != SaveGameResult::Status::Accepted) throw std::runtime_error("Save request was not accepted");
            record("house_built_and_save_queued");
        } else if (frame == 2 && Saves().LastResult().status == SaveGameResult::Status::Saved) {
            record("saved");
        } else if (frame == 62) {
            if (!economy->BuildOneHouse(GetScene())) throw std::runtime_error("Second house was rejected");
            record("second_house_after_income");
        } else if (frame == 63) {
            const auto request = Saves().RequestLoad("city");
            if (request.status != SaveGameResult::Status::Accepted) throw std::runtime_error("Load request was not accepted");
            record("load_queued");
        } else if (frame == 64 && Saves().LastResult().status == SaveGameResult::Status::Loaded) {
            record("loaded_scene_callbacks_complete");
        } else if (frame == 125) record("income_continued_after_load");
    } else if (std::string(mode) == "second") {
        if (frame == 1) {
            const auto request = Saves().RequestLoad("city");
            if (request.status != SaveGameResult::Status::Accepted) throw std::runtime_error("Second-process load request was not accepted");
            record("second_process_load_queued");
        } else if (frame == 2 && Saves().LastResult().status == SaveGameResult::Status::Loaded) {
            record("second_process_loaded");
        } else if (frame == 70) record("second_process_income_continued");
    }
}
