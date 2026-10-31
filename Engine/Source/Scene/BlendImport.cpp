#define NOMINMAX
#include <Windows.h>
#include "ModelImport.h"
#include <Engine/Resources/Paths.h>
#include <algorithm>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
namespace
{
struct Handle
{
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring Environment(const wchar_t* name)
{
    const auto size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(name, value.data(), size));
    return value;
}
fs::path BlenderExecutable()
{
    const auto configured = Environment(L"PAINENGINE_BLENDER");
    if (!configured.empty())
    {
        if (fs::is_regular_file(configured)) return fs::absolute(configured);
        throw std::runtime_error("PAINENGINE_BLENDER does not point to blender.exe");
    }
    // A portable copy can accompany the runtime without changing PATH or installing Blender.
    const auto portable = EnginePaths::ExecutableDirectory() / "Tools/Blender/blender.exe";
    if (fs::is_regular_file(portable)) return portable;
    std::wstring path(32768, L'\0');
    const auto count = SearchPathW(nullptr, L"blender.exe", nullptr, DWORD(path.size()), path.data(), nullptr);
    if (count && count < path.size()) { path.resize(count); return path; }
    const auto programFiles = Environment(L"ProgramFiles");
    if (!programFiles.empty())
    {
        const auto foundation = fs::path(programFiles) / "Blender Foundation";
        std::vector<fs::path> installed;
        if (fs::is_directory(foundation))
            for (const auto& entry : fs::directory_iterator(foundation))
                if (fs::is_regular_file(entry.path() / "blender.exe")) installed.push_back(entry.path() / "blender.exe");
        std::sort(installed.rbegin(), installed.rend());
        if (!installed.empty()) return installed.front();
    }
    throw std::runtime_error("BLEND import requires Blender 4.2 or newer. Install Blender, set PAINENGINE_BLENDER to blender.exe, "
                             "or place a portable copy in Runtime/Tools/Blender. FBX does not require Blender.");
}
std::wstring Quote(const fs::path& path)
{
    // These are absolute file paths, not shell expressions. Windows filenames cannot contain quotes.
    return L"\"" + path.wstring() + L"\"";
}
std::string LogTail(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const auto size = file.tellg();
    file.seekg(size > std::streamoff(8192) ? size - std::streamoff(8192) : std::streampos(0));
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

void ModelImport::ConvertBlend(const fs::path& source, const fs::path& destination)
{
#if defined(ENGINE_ENABLE_ASSIMP)
    const auto blender = BlenderExecutable();
    const auto script = EnginePaths::ContentRoot() / "Tools/ImportBlend.py";
    if (!fs::is_regular_file(script)) throw std::runtime_error("EngineContent/Tools/ImportBlend.py is missing; update the engine distribution");
    const auto logPath = destination.parent_path() / "BlenderImport.log";
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle log{CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Handle input{CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr)};
    if (log.value == INVALID_HANDLE_VALUE || input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create Blender import log");
    std::wstring command = Quote(blender) + L" --background --factory-startup --disable-autoexec --python-exit-code 1 --python " +
        Quote(script) + L" -- " + Quote(source) + L" " + Quote(destination);
    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value; startup.hStdOutput = startup.hStdError = log.value;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(blender.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        source.parent_path().c_str(), &startup, &process))
        throw std::runtime_error("Cannot start Blender (Windows error " + std::to_string(GetLastError()) + ")");
    Handle processHandle{process.hProcess}, threadHandle{process.hThread};
    const auto wait = WaitForSingleObject(processHandle.value, 180000);
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(processHandle.value, 1);
        WaitForSingleObject(processHandle.value, INFINITE);
        throw std::runtime_error("Blender import timed out after 180 seconds. Export the model to FBX or GLB manually.");
    }
    DWORD result = 1;
    GetExitCodeProcess(processHandle.value, &result);
    if (result || !fs::is_regular_file(destination))
        throw std::runtime_error("Blender import failed:\n" + LogTail(logPath));
    if (!fs::file_size(destination) || fs::file_size(destination) > 256u * 1024 * 1024)
        throw std::runtime_error("Converted BLEND is empty or exceeds the model limit of 256 MiB");
#else
    (void)source; (void)destination;
    throw std::runtime_error("Model import requires an engine built with Assimp");
#endif
}
