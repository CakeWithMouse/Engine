#include <Engine/MainGame/GameSession.h>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <cstring>

// A generated translation unit supplies the strong symbol. Old consumers without
// generation get an empty catalog. Both symbols stay in the owning EXE/DLL.
extern "C" GameSubsystemCatalog EngineGeneratedSubsystemCatalog() noexcept;
extern "C" GameSubsystemCatalog EngineEmptySubsystemCatalog() noexcept { return {}; }
#pragma comment(linker, "/alternatename:EngineGeneratedSubsystemCatalog=EngineEmptySubsystemCatalog")

Scene* SubsystemContext::GetScene() const { return session.GetScene(); }
GameSession::GameSession() = default;
GameSession::~GameSession() { Stop(); assert(callbacks_ == 0); }
void GameSession::CheckThread() const
{
    if (thread_ != std::this_thread::get_id()) throw std::logic_error("GameSession requires its owner thread");
}
bool GameSession::Initialize(std::string& error) { return Initialize(EngineGeneratedSubsystemCatalog(), error); }
bool GameSession::Initialize(GameSubsystemCatalog catalog, std::string& error)
{
    CheckThread();
    error.clear();
    if (running_ || initializing_ || finished_) { error = "Session already started or stopped"; return false; }
    initializing_ = true;
    ++callbacks_; // Stop from constructors/Initialize is deferred through rollback.
    try {
        if (catalog.count && !catalog.types) throw std::runtime_error("Invalid subsystem catalog");
        std::vector<GameSubsystemType> types;
        for (size_t i = 0; i < catalog.count; ++i) {
            const auto& type = catalog.types[i];
            if (!type.key || !*type.key || !type.create) throw std::runtime_error("Invalid subsystem factory");
            types.push_back(type);
        }
        std::sort(types.begin(), types.end(), [](const auto& a, const auto& b) { return std::strcmp(a.key,b.key) < 0; });
        for (size_t i = 1; i < types.size(); ++i)
            if (std::strcmp(types[i-1].key, types[i].key) == 0) throw std::runtime_error("Duplicate subsystem key");
        for (const auto& type : types) {
            auto object = type.create();
            if (!object) throw std::runtime_error("Null subsystem factory result");
            entries_.push_back({std::move(object)});
            if (stopping_) throw std::runtime_error("Session stopped during creation");
        }
        for (auto& entry : entries_) {
            entry.initialized = true; // Includes the partially initialized failing object.
            if (!entry.object->Initialize(context_, error)) {
                if (error.empty()) error = "Subsystem initialization failed";
                throw std::runtime_error(error);
            }
            if (stopping_) throw std::runtime_error("Session stopped during initialization");
        }
        running_ = true;
    } catch (const std::exception& e) { error = e.what(); stopping_ = true; }
      catch (...) { error = "Subsystem initialization threw"; stopping_ = true; }
    initializing_ = false;
    EndCallback();
    return IsRunning();
}
void GameSession::EndCallback() noexcept { --callbacks_; if (!callbacks_ && stopping_) FinishStop(); }
void GameSession::SceneReady(Scene& scene)
{
    CheckThread();
    if (!IsRunning() || callbacks_) throw std::logic_error("Session is not ready for a scene transition");
    if (scene_ == &scene) return;
    if (scene_) throw std::logic_error("Close previous scene before attaching another");
    scene_ = &scene;
    ++callbacks_;
    try {
        for (auto& entry : entries_) {
            entry.ready = true;
            entry.object->OnSceneReady(scene);
            if (stopping_) break;
        }
    } catch (...) { stopping_ = true; EndCallback(); throw; }
    EndCallback();
}
void GameSession::SceneClosing(Scene& scene) noexcept
{
    assert(thread_ == std::this_thread::get_id());
    if (scene_ != &scene) return;
    assert(!callbacks_); // Game destruction/transition is only legal outside callbacks.
    ++callbacks_;
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (!it->ready) continue;
        it->ready = false;
        try { it->object->OnSceneClosing(scene); } catch (...) { stopping_ = true; }
    }
    scene_ = nullptr;
    EndCallback();
}
void GameSession::Tick(float dt)
{
    CheckThread();
    if (!IsRunning() || !scene_ || callbacks_) return;
    ++callbacks_;
    try {
        for (auto& entry : entries_) {
            if (entry.object->bTickEnabled) entry.object->Tick(dt);
            if (stopping_) break;
        }
    } catch (...) { EndCallback(); throw; } // Editor pauses via EngineSceneSimulationFailed.
    EndCallback();
}
void GameSession::Stop() noexcept
{
    assert(thread_ == std::this_thread::get_id());
    stopping_ = true;
    if (!callbacks_) FinishStop();
}
void GameSession::FinishStop() noexcept
{
    if (finished_) return;
    ++callbacks_;
    if (scene_) for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (!it->ready) continue;
        it->ready = false;
        try { it->object->OnSceneClosing(*scene_); } catch (...) {}
    }
    scene_ = nullptr;
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
        if (it->initialized) it->object->Deinitialize();
    while (!entries_.empty()) entries_.pop_back();
    running_ = false; finished_ = true;
    --callbacks_;
}
