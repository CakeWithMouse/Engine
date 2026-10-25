#pragma once
#include <Engine/Runtime/EditSessionApi.h>
#include <filesystem>
#include <Engine/Runtime/LegacyEditApi.h>

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
    uint32_t PreferredSceneVersion() const noexcept { return preferredSceneVersion_; }
private:
    void* library_ = nullptr;
    EngineEditModuleApi factory_{};
    EngineEditSessionApi api_{};
    EngineLegacyV4::Factory legacyFactory_{};
    EngineLegacyV4::EditApi legacy_{};
    uint32_t preferredSceneVersion_ = 1;
};
