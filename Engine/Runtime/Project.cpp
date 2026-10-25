#define NOMINMAX
#include "Project.h"
#include <Windows.h>
#include <fstream>
#include <map>
#include <stdexcept>
#include <algorithm>
#include "../ThirdParty/nlohmann/json.hpp"

namespace
{
class ProjectJson
{
    const std::string& text;
    size_t pos = 0;
    [[noreturn]] void Invalid() const { throw std::runtime_error("Invalid Project.json near byte " + std::to_string(pos)); }
    void Space() { while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')) ++pos; }
    bool Take(char c) { Space(); if (pos < text.size() && text[pos] == c) { ++pos; return true; } return false; }
    unsigned Hex()
    {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (pos == text.size()) Invalid();
            const char c = text[pos++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) Invalid();
            value = value * 16 + digit;
        }
        return value;
    }
    void Unicode(std::string& value)
    {
        wchar_t chars[2] = {static_cast<wchar_t>(Hex()), 0};
        int count = 1;
        if (chars[0] >= 0xd800 && chars[0] <= 0xdbff)
        {
            if (pos + 2 > text.size() || text[pos++] != '\\' || text[pos++] != 'u') Invalid();
            chars[1] = static_cast<wchar_t>(Hex());
            if (chars[1] < 0xdc00 || chars[1] > 0xdfff) Invalid();
            count = 2;
        }
        else if (chars[0] >= 0xdc00 && chars[0] <= 0xdfff) Invalid();
        char buffer[8];
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, count, buffer, 8, nullptr, nullptr);
        if (!size) Invalid();
        value.append(buffer, size);
    }
    std::string String()
    {
        if (!Take('"')) Invalid();
        std::string value;
        while (pos < text.size())
        {
            const unsigned char c = text[pos++];
            if (c == '"') return value;
            if (c < 0x20) Invalid();
            if (c != '\\') { value += static_cast<char>(c); continue; }
            if (pos == text.size()) Invalid();
            switch (text[pos++])
            {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case 'u': Unicode(value); break;
            default: Invalid();
            }
        }
        Invalid();
    }
public:
    explicit ProjectJson(const std::string& source) : text(source) {}
    std::map<std::string, std::string> Read()
    {
        std::map<std::string, std::string> fields;
        if (!Take('{')) Invalid();
        if (!Take('}'))
        {
            do
            {
                const auto key = String();
                if (!Take(':')) Invalid();
                if (!fields.emplace(key, String()).second) Invalid();
                if (Take('}')) break;
                if (!Take(',')) Invalid();
            } while (true);
        }
        Space();
        if (pos != text.size()) Invalid();
        return fields;
    }
};
}

RuntimeProject ReadRuntimeProject(const std::filesystem::path& path)
{
    const auto file = std::filesystem::absolute(path).lexically_normal();
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open project: " + file.u8string());
    if (std::filesystem::file_size(file) > 65536) throw std::runtime_error("Project.json exceeds 64 KiB");
    std::string json((std::istreambuf_iterator<char>(input)), {});
    if (json.compare(0, 3, "\xef\xbb\xbf") == 0) json.erase(0, 3);
    if (json.empty() || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), nullptr, 0))
        throw std::runtime_error("Project.json must be valid UTF-8");
    const auto fields = ProjectJson(json).Read();
    if (!fields.count("name") || !fields.count("module") || !fields.count("content") ||
        fields.size() != 3 + fields.count("preview") + fields.count("startupScene") + fields.count("engineVersion") + fields.count("engineConfiguration") + fields.count("templateId"))
        throw std::runtime_error("Unsupported Project.json fields; use the engine version that created this project");
    for (const auto& field : fields)
        if (field.second.empty() || field.second.find_first_of(std::string("\0\r\n", 3)) != std::string::npos)
            throw std::runtime_error("Project fields must be nonempty single-line strings without NUL");
    RuntimeProject project{fields.at("name"), file,
        (file.parent_path() / std::filesystem::u8path(fields.at("module"))).lexically_normal(),
        (file.parent_path() / std::filesystem::u8path(fields.at("content"))).lexically_normal()};
    if (!std::filesystem::is_directory(project.content)) throw std::runtime_error("Project content directory is missing");
    if (fields.count("preview")) project.preview = fields.at("preview");
    auto token = [](const std::string& s) { return !s.empty() && s.size() <= 64 && s != "." && s != ".." &&
        s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_") == std::string::npos; };
    if (fields.count("engineVersion"))
    {
        project.engineVersion = fields.at("engineVersion");
        if (!token(project.engineVersion)) throw std::runtime_error("Invalid engine version");
        if (!fields.count("engineConfiguration")) throw std::runtime_error("engineVersion requires engineConfiguration");
    }
    if (fields.count("engineConfiguration"))
    {
        project.engineConfiguration = fields.at("engineConfiguration");
        if (project.engineVersion.empty() || (project.engineConfiguration != "Debug" && project.engineConfiguration != "Release"))
            throw std::runtime_error("Invalid engine configuration");
    }
    if (fields.count("templateId")) project.templateId = fields.at("templateId");
    const auto lockFile = file.parent_path() / "Engine.lock.json";
    if (std::filesystem::exists(lockFile))
    {
        std::ifstream lock(lockFile);
        const auto pinned = nlohmann::json::parse(lock);
        if (pinned.at("format") != 1) throw std::runtime_error("Unsupported engine lock format");
        const auto version = pinned.at("engineVersion").get<std::string>();
        const auto configuration = pinned.at("engineConfiguration").get<std::string>();
        if (!token(version) || (configuration != "Debug" && configuration != "Release") ||
            (!project.engineVersion.empty() && (version != project.engineVersion || configuration != project.engineConfiguration)))
            throw std::runtime_error("Invalid or conflicting engine lock");
        project.engineVersion = version; project.engineConfiguration = configuration;
        project.templateId = pinned.value("templateId", project.templateId);
    }
    if (fields.count("startupScene"))
    {
        const auto relative = std::filesystem::u8path(fields.at("startupScene"));
        if (relative.has_root_path()) throw std::runtime_error("startupScene must be relative to Project.json");
        project.startupScene = (file.parent_path() / relative).lexically_normal();
    }
    return project;
}

EngineRelease ReadEngineRelease(const std::filesystem::path& runtime)
{
    std::ifstream in(runtime / "EngineVersion.json");
    if (!in) throw std::runtime_error("EngineVersion.json is missing: " + runtime.u8string());
    const auto json = nlohmann::json::parse(in);
    const auto version = json.at("version").get<std::string>();
    if (version.empty() || version.size() > 64 || version == "." || version == ".." ||
        version.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_") != std::string::npos)
        throw std::runtime_error("Invalid engine release version");
    std::ifstream config(runtime / "EngineConfiguration.txt");
    std::string configuration; config >> configuration;
    if (configuration != "Debug" && configuration != "Release") throw std::runtime_error("Invalid engine release configuration");
    for (auto file : {"EngineRuntime.exe", "Editor.dll", "Templates/SceneProject/Game.dll", "EngineContent/Shaders/Common.hlsli"})
        if (!std::filesystem::is_regular_file(runtime / file)) throw std::runtime_error(std::string("Incomplete engine release: ") + file);
    return {version,configuration,std::filesystem::absolute(runtime).lexically_normal()};
}
std::filesystem::path EngineStore()
{
    wchar_t buffer[32768];
    auto size = GetEnvironmentVariableW(L"PAINENGINE_ENGINE_STORE", buffer, 32768);
    if (size && size < 32768)
    {
        const std::filesystem::path path(buffer);
        if (!path.is_absolute()) throw std::runtime_error("PAINENGINE_ENGINE_STORE must be absolute");
        return path;
    }
    size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
    if (!size || size >= 32768) throw std::runtime_error("LOCALAPPDATA is unavailable");
    return std::filesystem::path(buffer) / "PainEngine/Engines";
}
EngineRelease InstallEngineRelease(const std::filesystem::path& runtime)
{
    const auto release = ReadEngineRelease(runtime);
    const auto parent = EngineStore() / release.version;
    const auto target = parent / release.configuration;
    const auto relativeTarget = std::filesystem::weakly_canonical(target).lexically_relative(std::filesystem::weakly_canonical(release.runtime));
    if (!relativeTarget.empty() && !relativeTarget.is_absolute() && *relativeTarget.begin() != ".." && relativeTarget != ".")
        throw std::runtime_error("Engine store cannot be inside the release being installed");
    if (std::filesystem::exists(target))
    {
        const auto installed = ReadEngineRelease(target);
        if (installed.version != release.version || installed.configuration != release.configuration)
            throw std::runtime_error("Installed engine release identity mismatch");
        for (const auto* name : {"EngineRuntime.exe","Editor.dll","Templates/SceneProject/Game.dll"})
        {
            const auto oldFile = target / name, newFile = release.runtime / name;
            std::ifstream oldBytes(oldFile,std::ios::binary), newBytes(newFile,std::ios::binary);
            if (std::filesystem::file_size(oldFile) != std::filesystem::file_size(newFile) ||
                !std::equal(std::istreambuf_iterator<char>(oldBytes),std::istreambuf_iterator<char>(),std::istreambuf_iterator<char>(newBytes)))
                throw std::runtime_error("This engine version is already installed with different binaries. Assign a new release version; the installed release was preserved.");
        }
        return installed;
    }
    std::filesystem::create_directories(parent);
    const auto staging = parent / (release.configuration + ".install-" + std::to_string(GetCurrentProcessId()));
    if (!std::filesystem::create_directory(staging)) throw std::runtime_error("Engine installation is already in progress");
    try
    {
        for (auto it = std::filesystem::recursive_directory_iterator(release.runtime); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if ((GetFileAttributesW(it->path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) || (it->is_directory() && (it->path().filename() == ".shadow_cache" || it->path().filename() == "Projects")))
            { if (it->is_directory()) it.disable_recursion_pending(); continue; }
            const auto dest = staging / std::filesystem::relative(it->path(), release.runtime);
            if (it->is_directory()) std::filesystem::create_directories(dest);
            else if (it->is_regular_file() && it->path().extension() != ".pdb" && it->path().extension() != ".lib" && it->path().extension() != ".exp" && it->path().filename() != "EditorUiTests.exe")
                std::filesystem::copy_file(it->path(), dest);
        }
        ReadEngineRelease(staging);
        std::filesystem::rename(staging, target); // complete release becomes visible atomically
    }
    catch (...) { std::error_code ec; std::filesystem::remove_all(staging, ec); throw; }
    return ReadEngineRelease(target);
}
EngineRelease ResolveEngineRelease(const RuntimeProject& project, const std::filesystem::path& currentRuntime)
{
    const auto current = ReadEngineRelease(currentRuntime);
    if (project.engineVersion.empty()) return current; // legacy DLLs are negotiated by the adapter
    const auto saved = EngineStore() / project.engineVersion / project.engineConfiguration;
    auto matching = [&](const std::filesystem::path& path) {
        const auto release = ReadEngineRelease(path);
        if (release.version != project.engineVersion || release.configuration != project.engineConfiguration)
            throw std::runtime_error("Installed engine manifest does not match the pinned version");
        return release;
    };
    if (std::filesystem::exists(saved)) return matching(saved);
    const auto bundled = currentRuntime.parent_path() / "Engines" / project.engineVersion / project.engineConfiguration;
    if (std::filesystem::exists(bundled)) return matching(bundled);
    if (project.engineVersion == current.version && project.engineConfiguration == current.configuration) return current;
    throw std::runtime_error("Project requires PainEngine " + project.engineVersion + " " + project.engineConfiguration +
        ". Install that release using 'Add engine version'. The project has not been changed.");
}
