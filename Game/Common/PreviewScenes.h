#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

// Built-in scene adapters belong to the example applications, never Engine.lib or Editor.dll.
// An empty identifier opens the editor shell; unknown identifiers fail explicitly.
std::function<std::unique_ptr<Game>()> FindPreviewScene(
    const std::string& identifier, const std::filesystem::path& contentRoot);
