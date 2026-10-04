#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

enum class RuntimeMode { Game, Editor };

// Host-only launch description. This is not part of the module SDK/ABI.
struct RuntimeLaunchOptions
{
    RuntimeMode mode = RuntimeMode::Game;
    std::filesystem::path module;
    std::string projectName;
    std::string projectFile;
    std::string contentRoot;
    uint64_t frames = 0; // 0: interactive, Enter steps a frame, q exits.
    bool failEngineForTest = false; // Explicit diagnostic fault injection.
};
