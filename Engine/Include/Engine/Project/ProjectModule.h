#pragma once
#include <Engine/Project/StandardTypes.h>
#include <Engine/Components/ProjectObject.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <fstream>
#include <filesystem>
#include <vector>

inline std::vector<ComponentType> CombineProjectTypes(ComponentTypeCatalog custom)
{
    auto standard = SceneProject::GetComponentTypes();
    std::vector<ComponentType> result(standard.data, standard.data + standard.size);
    for (size_t i = 0; i < custom.size; ++i)
    {
        for (const auto& type : result)
            if (std::string_view(type.typeName) == custom.data[i].typeName)
                throw std::runtime_error("Duplicate project type");
        result.push_back(custom.data[i]);
    }
    return result;
}
inline std::unique_ptr<Game> SelectProjectGame(GameTypeCatalog types)
{
    std::string selected;
    const auto root = EngineModuleVfs::Resolve("/Game");
    if (!root.empty()) {
        std::ifstream file(std::filesystem::u8path(root) / "ProjectGame.txt");
        std::getline(file, selected);
        if (!selected.empty() && selected.back() == '\r') selected.pop_back();
    }
    if (selected.empty()) return types.size ? types.data[0].create() : std::make_unique<Game>();
    for (size_t i = 0; i < types.size; ++i)
        if (selected == types.data[i].name) return types.data[i].create();
    throw std::runtime_error("Selected Game class is missing: " + selected);
}
