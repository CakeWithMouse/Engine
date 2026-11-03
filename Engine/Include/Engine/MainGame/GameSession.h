#pragma once
#include <Engine/MainGame/GameSubsystem.h>
#include <thread>
#include <vector>

// Module-local owner. Never pass this C++ API through the host/DLL boundary.
class GameSession final
{
public:
    GameSession();
    ~GameSession();
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    bool Initialize(std::string& error);
    bool Initialize(GameSubsystemCatalog catalog, std::string& error);
    void SceneReady(Scene& scene);
    void SceneClosing(Scene& scene) noexcept;
    void Tick(float deltaTime);
    void Stop() noexcept;
    bool IsRunning() const { return running_ && !stopping_; }
    Scene* GetScene() const { return scene_; }
    template<class T> T* GetSubsystem() const
    {
        CheckThread();
        if (!IsRunning() || initializing_) return nullptr;
        for (const auto& entry : entries_)
            if (auto* value = dynamic_cast<T*>(entry.object.get())) return value;
        return nullptr;
    }
private:
    struct Entry { std::unique_ptr<GameSubsystem> object; bool initialized = false; bool ready = false; };
    std::vector<Entry> entries_;
    std::thread::id thread_ = std::this_thread::get_id();
    SubsystemContext context_{*this};
    Scene* scene_ = nullptr;
    bool running_ = false, initializing_ = false, stopping_ = false, finished_ = false;
    unsigned callbacks_ = 0;
    void CheckThread() const;
    void FinishStop() noexcept;
    void EndCallback() noexcept;
};
