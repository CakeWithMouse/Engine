#pragma once
#include <Windows.h>
#include <string>

namespace LauncherCli
{
inline void InitializeStreams()
{
    const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    const auto error = GetStdHandle(STD_ERROR_HANDLE);
    const auto outputType = GetFileType(output), errorType = GetFileType(error);
    // Attach only to an existing terminal. Attaching can replace redirected handles.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (outputType == FILE_TYPE_DISK || outputType == FILE_TYPE_PIPE) SetStdHandle(STD_OUTPUT_HANDLE, output);
        if (errorType == FILE_TYPE_DISK || errorType == FILE_TYPE_PIPE) SetStdHandle(STD_ERROR_HANDLE, error);
    }
}
inline void Write(bool error, const std::string& text)
{
    const auto handle = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (!handle || handle == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0, written = 0;
    if (GetConsoleMode(handle, &mode)) {
        const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
        std::wstring wide(count, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), wide.data(), count);
        WriteConsoleW(handle, wide.data(), DWORD(wide.size()), &written, nullptr);
    } else {
        size_t offset = 0;
        while (offset < text.size()) {
            if (!WriteFile(handle, text.data()+offset, DWORD(text.size()-offset), &written, nullptr) || !written) break;
            offset += written;
        }
    }
}
struct Command { const wchar_t* name; int argc; const char* usage; };
inline constexpr Command Commands[] = {
    {L"--help",2,"--help"}, {L"-h",2,"-h"}, {L"/?",2,"/?"},
    {L"--create-project",4,"--create-project <NEW-folder> <name>"},
    {L"--enable-code",3,"--enable-code <Project.json>"},
    {L"--build-project",3,"--build-project <Project.json>"},
    {L"--install-engine",3,"--install-engine <Runtime-folder>"},
    {L"--upgrade-project",4,"--upgrade-project <Project.json> <NEW-copy-folder>"},
    {L"--check-project",3,"--check-project <Project.json>"},
    {L"--smoke-project",3,"--smoke-project <Project.json>"},
    {L"--smoke-project-editor",3,"--smoke-project-editor <Project.json>"},
    {L"--smoke-project-current",3,"--smoke-project-current <Project.json>"},
    {L"--smoke-project-editor-current",3,"--smoke-project-editor-current <Project.json>"},
    {L"--smoke-project-prompt",3,"--smoke-project-prompt <Project.json>"},
    {L"--smoke-project-editor-prompt",3,"--smoke-project-editor-prompt <Project.json>"},
    {L"--check",3,"--check <example>"}, {L"--smoke",3,"--smoke <example>"},
    {L"--check-editor",3,"--check-editor <example>"}, {L"--smoke-editor",3,"--smoke-editor <example>"}
};
inline void Help()
{
    Write(false, "PainEngine Launcher\nUsage: EngineLauncher.exe <command> [arguments]\nWithout arguments: open the launcher GUI.\n\nCommands:\n");
    for (const auto& command : Commands) Write(false, std::string("  ")+command.usage+"\n");
    Write(false,
        "\nCreate a C++ project (the parent folder must exist):\n"
        "  EngineLauncher.exe --create-project \"E:\\Games\\MyCity\" \"My City\"\n"
        "  EngineLauncher.exe --enable-code \"E:\\Games\\MyCity\\Project.json\"\n"
        "  EngineLauncher.exe --build-project \"E:\\Games\\MyCity\\Project.json\"\n"
        "  EngineLauncher.exe --smoke-project \"E:\\Games\\MyCity\\Project.json\"\n"
        "The project folder must be NEW, even if an existing folder is empty.\n"
        "C++ requires Visual Studio C++ tools, Windows SDK and the pinned engine's SDK.\n"
        "Build diagnostics: CodeBuild.log in the project folder.\n"
        "Default project smoke uses the pinned engine; -current uses this launcher's Runtime.\n"
        "Smoke runs 3 frames. Only -prompt smoke options display the upgrade dialog.\n"
        "Exit codes: 0 success, 1 operation failed, 2 invalid command/arguments/example.\n"
        "Output: stdout for help; stderr for errors; redirected text is UTF-8.\n"
        "PowerShell: use Start-Process -Wait -PassThru (GUI executable); read ExitCode.\n"
        "Guide: Engine/docs/LAUNCHER-CLI.md\n");
}
}
