#include <Engine/Resources/Paths.h>
#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif
#include <stdexcept>
#include <string>
#include <vector>

namespace { std::filesystem::path contentRoot; }

std::filesystem::path EnginePaths::ExecutableDirectory()
{
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) throw std::runtime_error("Cannot resolve executable directory");
    buffer.resize(count);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw std::runtime_error("Cannot resolve executable directory");
    return std::filesystem::canonical(buffer.data()).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(1024);
    for (;;)
    {
        const auto count = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (count < 0) throw std::runtime_error("Cannot resolve executable directory");
        if (static_cast<size_t>(count) < buffer.size())
            return std::filesystem::path(std::string(buffer.data(), static_cast<size_t>(count))).parent_path();
        buffer.resize(buffer.size() * 2);
    }
#else
#error Implement executable directory discovery for this platform
#endif
}

void EnginePaths::SetContentRoot(const std::filesystem::path& root)
{
    contentRoot = root.empty() ? std::filesystem::path{} : std::filesystem::absolute(root).lexically_normal();
}

std::filesystem::path EnginePaths::ContentRoot()
{
    return contentRoot.empty() ? ExecutableDirectory() / "EngineContent" : contentRoot;
}

std::filesystem::path EnginePaths::Shader(const char* name)
{
    return ContentRoot() / "Shaders" / name;
}
