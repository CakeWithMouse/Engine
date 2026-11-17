#define NOMINMAX
#include <Windows.h>
#include <filesystem>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int)
{
    const bool interactive = !*arguments;
    const auto fail = [interactive](const std::wstring& message, DWORD code) {
        if (interactive) MessageBoxW(nullptr, message.c_str(), L"Game.exe", MB_OK | MB_ICONERROR);
        return static_cast<int>(code);
    };

    wchar_t executable[32768];
    const DWORD length = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
    if (!length || length == ARRAYSIZE(executable))
        return fail(L"Cannot locate the game folder.", 1);

    const auto root = std::filesystem::path(executable).parent_path();
    const auto runtime = root / L"Runtime/EngineRuntime.exe";
    const auto project = root / L"Game/Project.json";
    std::wstring command = L"\"" + runtime.wstring() + L"\" --project \"" + project.wstring() + L"\" --mode game " + arguments;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
    const bool redirect = output && output != INVALID_HANDLE_VALUE && error && error != INVALID_HANDLE_VALUE;
    if (redirect) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = output;
        startup.hStdError = error;
    }
    if (!CreateProcessW(runtime.c_str(), command.data(), nullptr, nullptr, redirect,
            CREATE_NO_WINDOW, nullptr, runtime.parent_path().c_str(), &startup, &process))
        return fail(L"Cannot start the game. Check that the Runtime and Game folders are beside Game.exe.", GetLastError());

    CloseHandle(process.hThread);
    DWORD code = 1;
    if (WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0)
        GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    if (code) return fail(L"The game exited with code " + std::to_wstring(code) + L".", code);
    return 0;
}
