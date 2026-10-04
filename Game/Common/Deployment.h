#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ExampleDeployment
{
inline std::filesystem::path ContentRoot(const wchar_t* variable, const std::filesystem::path& fallback)
{
    const DWORD size = GetEnvironmentVariableW(variable, nullptr, 0);
    if (!size) return fallback;
    std::wstring value(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(variable, value.data(), size);
    if (!written || written >= size) throw std::runtime_error("Cannot read content root override");
    value.resize(written);
    return std::filesystem::absolute(value);
}

inline std::filesystem::path ExecutableDirectory()
{
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!size || size >= buffer.size()) throw std::runtime_error("Cannot resolve executable directory");
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
}

// A manifest is deployed with each game. Both direct startup and launcher use it.
inline void ValidateContent(const std::filesystem::path& directory)
{
    const auto manifest = directory / "RequiredFiles.txt";
    std::ifstream input(manifest);
    if (!input) throw std::runtime_error("Missing manifest: " + manifest.u8string());
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        auto file = directory / std::filesystem::u8path(line);
        if (line.rfind("EngineContent/", 0) == 0)
            file = ContentRoot(L"ENGINE_CONTENT_ROOT", directory / "EngineContent") / std::filesystem::u8path(line.substr(14));
        else if (line.rfind("GameContent/", 0) == 0)
            file = ContentRoot(L"GAME_CONTENT_ROOT", directory / "GameContent") / std::filesystem::u8path(line.substr(12));
        if (!std::filesystem::is_regular_file(file))
            throw std::runtime_error("Missing required file: " + file.u8string());
    }
    if (input.bad()) throw std::runtime_error("Cannot read manifest: " + manifest.u8string());
}

inline std::wstring Wide(const std::string& text)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
}
