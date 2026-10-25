#pragma once
#include <Engine/Runtime/EditSessionApi.h>
#include <filesystem>

// Host-side RAII owner of an edit-only DLL session. Must be created, called and
// destroyed on the engine thread, before HWND destruction. No hot reload of live data.
class GameEditModule final
{
public:
    GameEditModule(const std::filesystem::path& module, const std::filesystem::path& engineContent,
        const std::filesystem::path& projectContent = {});
    ~GameEditModule();
    GameEditModule(const GameEditModule&) = delete;
    GameEditModule& operator=(const GameEditModule&) = delete;
    const EngineEditSessionApi& Api() const noexcept { return api_; }
private:
    void* library_ = nullptr;
    EngineEditModuleApi factory_{};
    EngineEditSessionApi api_{};
};
