#include <Engine/Resources/Paths.h>
#include <Windows.h>
#include <stdexcept>
#include <string>

namespace { std::filesystem::path contentRoot; }

std::filesystem::path EnginePaths::ExecutableDirectory()
{
    std::wstring buffer(32768, L'\0');
    const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) throw std::runtime_error("Cannot resolve executable directory");
    buffer.resize(count);
    return std::filesystem::path(buffer).parent_path();
}

void EnginePaths::SetContentRoot(const std::filesystem::path& root)
{
    contentRoot = std::filesystem::absolute(root).lexically_normal();
}

std::filesystem::path EnginePaths::ContentRoot()
{
    return contentRoot.empty() ? ExecutableDirectory() / "EngineContent" : contentRoot;
}

std::filesystem::path EnginePaths::Shader(const char* name)
{
    return ContentRoot() / "Shaders" / name;
}
