#pragma once
#include "../../../Engine/Runtime/Project.h"
#include "../../../Engine/ThirdParty/nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>

namespace ProjectLibrary
{
using Json = nlohmann::json;
inline void Write(const std::filesystem::path& file, const Json& value)
{
    const auto text = value.dump(2) + "\n";
    auto temporary = file; temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
    try
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.write(text.data(), text.size()); out.close();
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot save: " + file.u8string());
    }
    catch (...) { std::error_code ec; std::filesystem::remove(temporary, ec); throw; }
}
inline RuntimeProject Validate(const std::filesystem::path& file)
{
    auto project = ReadRuntimeProject(file);
    if (!std::filesystem::is_regular_file(project.module)) throw std::runtime_error("Game module is missing: " + project.module.u8string());
    if (project.startupScene.empty() || !std::filesystem::is_regular_file(project.startupScene))
        throw std::runtime_error("Project requires an existing startupScene");
    return project;
}
inline std::filesystem::path Create(const std::filesystem::path& directory, const std::string& name,
    const std::filesystem::path& templateModule)
{
    if (name.empty() || name.size() > 200 || name.find_first_of(std::string("\0\r\n", 3)) != std::string::npos ||
        name.find_first_not_of(" \t") == std::string::npos)
        throw std::runtime_error("Enter a project name (1–200 UTF-8 bytes)");
    if (!std::filesystem::is_regular_file(templateModule)) throw std::runtime_error("SceneProject template is missing. Build EngineLauncher first.");
    const auto root = std::filesystem::absolute(directory).lexically_normal();
    // Never adopt or overwrite an existing directory, even an empty one.
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("Choose a NEW project folder; this folder already exists");
    try
    {
        std::filesystem::create_directories(root / "Content" / "Scenes");
        std::filesystem::copy_file(templateModule, root / "Game.dll");
        auto object = [](uint64_t id, const char* type, const char* label, Json position, Json scale) {
            return Json{{"id",id},{"type",type},{"name",label},{"parent",0},{"inheritParentScale",true},
                {"transform",{{"position",position},{"rotation",{0,0,0,1}},{"scale",scale}}}};
        };
        Write(root / "Content/Scenes/Main.scene.json", {{"version",1},{"name","Main"},{"objects",Json::array({
            object(1,"Primitive.Plane","Ground",{0,-0.5,0},{20,1,20}),
            object(2,"Primitive.Cube","Cube",{0,0,0},{1,1,1})})}});
        Write(root / "Project.json", {{"name",name},{"module","Game.dll"},{"content","Content"},
            {"startupScene","Content/Scenes/Main.scene.json"}});
        Validate(root / "Project.json");
        return root / "Project.json";
    }
    catch (...)
    {
        // root was exclusively created by this call; pre-existing user folders never reach here.
        std::error_code ec; std::filesystem::remove_all(root, ec); throw;
    }
}
inline std::filesystem::path RegistryFile()
{
    wchar_t buffer[32768];
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
    if (!length || length >= 32768) throw std::runtime_error("LOCALAPPDATA is unavailable");
    return std::filesystem::path(buffer) / "EngineLauncher" / "projects.json";
}
inline std::vector<std::filesystem::path> ReadRegistry()
{
    const auto file = RegistryFile();
    if (!std::filesystem::exists(file)) return {};
    if (std::filesystem::file_size(file) > 1024 * 1024) throw std::runtime_error("Project library exceeds 1 MiB");
    std::ifstream input(file);
    auto json = Json::parse(input);
    if (!json.is_array()) throw std::runtime_error("Invalid project library");
    std::vector<std::filesystem::path> result;
    for (const auto& item : json) result.push_back(std::filesystem::u8path(item.get<std::string>()));
    return result;
}
inline void Remember(const std::filesystem::path& file)
{
    auto entries = ReadRegistry();
    const auto full = std::filesystem::absolute(file).lexically_normal();
    entries.erase(std::remove(entries.begin(), entries.end(), full), entries.end());
    entries.insert(entries.begin(), full);
    Json value = Json::array();
    for (const auto& entry : entries) value.push_back(entry.u8string());
    const auto registry = RegistryFile();
    std::filesystem::create_directories(registry.parent_path());
    Write(registry, value);
}
}
