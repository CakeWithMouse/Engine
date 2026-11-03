#pragma once
#include <memory>
#include <string>
#include <cstddef>
#include <type_traits>

class Scene;
class GameSession;
struct SubsystemContext
{
    GameSession& session;
    Scene* GetScene() const;
};

// Annotation consumed by ComponentGenerator; no static registration.
#define ENGINE_SUBSYSTEM()
class GameSubsystem
{
public:
    virtual ~GameSubsystem() = default;
    bool bTickEnabled = false;
    virtual bool Initialize(SubsystemContext&, std::string&) { return true; }
    virtual void OnSceneReady(Scene&) {}
    virtual void OnSceneClosing(Scene&) {}
    virtual void Tick(float) {}
    virtual void Deinitialize() noexcept {}
};

struct GameSubsystemType
{
    const char* key;
    std::unique_ptr<GameSubsystem> (*create)();
};
struct GameSubsystemCatalog
{
    const GameSubsystemType* types = nullptr;
    size_t count = 0;
};
template<class T> std::unique_ptr<GameSubsystem> CreateGameSubsystem()
{
    static_assert(std::is_base_of_v<GameSubsystem, T>, "ENGINE_SUBSYSTEM requires GameSubsystem");
    return std::make_unique<T>();
}
