#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include "Catalog.h"
#include "../../Common/Deployment.h"
#include <iostream>

namespace
{
HWND list = nullptr;
HWND status = nullptr;
HWND editButton = nullptr;
constexpr INT_PTR ListId = 100;
constexpr INT_PTR LaunchId = 101;
constexpr INT_PTR EditId = 102;

std::filesystem::path GameDirectory(const GameEntry& entry)
{
    return ExampleDeployment::ExecutableDirectory() / entry.directory;
}

std::filesystem::path Validate(const GameEntry& entry, bool editor = false)
{
    if (editor && !entry.runtime) throw std::runtime_error("Editing is not supported for legacy games");
    if (entry.runtime)
    {
        const auto runtime = ExampleDeployment::ExecutableDirectory() / "Runtime";
        const auto project = runtime / "Projects" / entry.directory;
        for (const auto& file : {runtime / "EngineRuntime.exe", project / "Project.json",
            editor ? runtime / "Editor.dll" : project / "Game.dll"})
            if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Runtime file is missing: " + file.u8string());
        return runtime / "EngineRuntime.exe";
    }
    const auto directory = GameDirectory(entry);
    const auto executable = directory / (std::wstring(entry.directory) + L".exe");
    if (!std::filesystem::is_regular_file(executable))
        throw std::runtime_error("Game is not built: " + executable.u8string());
    ExampleDeployment::ValidateContent(directory);
    return executable;
}

int Launch(const GameEntry& entry, bool smoke, bool editor = false)
{
    const auto executable = Validate(entry, editor);
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    if (entry.runtime)
    {
        const auto project = executable.parent_path() / "Projects" / entry.directory / "Project.json";
        command += L" --project \"" + project.wstring() + L"\" --mode " + (editor ? L"editor" : L"game");
        if (smoke) command += L" --frames 3";
    }
    else if (smoke) command += L" --smoke";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Inherit the caller's CWD: games must locate content independently of it.
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        entry.runtime && !smoke ? CREATE_NEW_CONSOLE : 0, nullptr, nullptr, &startup, &process))
        throw std::runtime_error("CreateProcess failed, Windows error " + std::to_string(GetLastError()));
    CloseHandle(process.hThread);
    if (!smoke) { CloseHandle(process.hProcess); return 0; }
    const DWORD wait = WaitForSingleObject(process.hProcess, 60000);
    if (wait != WAIT_OBJECT_0)
    {
        // Only the process created by this explicit diagnostic run is terminated.
        TerminateProcess(process.hProcess, 124);
        CloseHandle(process.hProcess);
        throw std::runtime_error("Game smoke run timed out or wait failed");
    }
    DWORD result = 1;
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hProcess);
    return static_cast<int>(result);
}

const GameEntry& Selected()
{
    const LRESULT selected = SendMessageW(list, LB_GETCURSEL, 0, 0);
    return Games.at(selected == LB_ERR ? 0 : static_cast<size_t>(selected));
}

void UpdateStatus()
{
    const auto& entry = Selected();
    EnableWindow(editButton, entry.runtime);
    try
    {
        Validate(entry);
        SetWindowTextW(status, entry.runtime ? L"Один EngineRuntime: игра или редактор. Пока консольные заглушки. Enter — кадр, q — выход." :
            L"Готово к запуску. Старый игровой exe; редактирование пока недоступно.");
    }
    catch (const std::exception& error)
    {
        const auto message = ExampleDeployment::Wide(error.what()) + L"\r\n" + entry.buildHint;
        SetWindowTextW(status, message.c_str());
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Выберите игру", WS_CHILD | WS_VISIBLE, 18, 15, 600, 24, window, nullptr, nullptr, nullptr);
        list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL,
            18, 45, 670, 145, window, reinterpret_cast<HMENU>(ListId), nullptr, nullptr);
        for (const auto& game : Games) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(game.title));
        SendMessageW(list, LB_SETCURSEL, 0, 0);
        CreateWindowW(L"BUTTON", L"Играть", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            18, 205, 155, 35, window, reinterpret_cast<HMENU>(LaunchId), nullptr, nullptr);
        editButton = CreateWindowW(L"BUTTON", L"Редактировать", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            190, 205, 180, 35, window, reinterpret_cast<HMENU>(EditId), nullptr, nullptr);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            18, 255, 670, 120, window, nullptr, nullptr, nullptr);
        UpdateStatus();
        return 0;
    case WM_ACTIVATE:
        if (list && LOWORD(wParam) != WA_INACTIVE) UpdateStatus();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_SELCHANGE) { UpdateStatus(); return 0; }
        if (LOWORD(wParam) == LaunchId || LOWORD(wParam) == EditId || (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_DBLCLK))
        {
            try
            {
                Launch(Selected(), false, LOWORD(wParam) == EditId);
                SetWindowTextW(status, L"Приложение запущено. Лаунчер можно закрыть.");
            }
            catch (const std::exception& error)
            {
                const auto text = ExampleDeployment::Wide(error.what()) + L"\r\n" + Selected().buildHint;
                MessageBoxW(window, text.c_str(), L"Не удалось запустить игру", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    bool diagnostic = argc > 1;
    try
    {
        if (diagnostic)
        {
            if (argc != 3 || (std::wstring(argv[1]) != L"--check" && std::wstring(argv[1]) != L"--smoke" &&
                std::wstring(argv[1]) != L"--check-editor" && std::wstring(argv[1]) != L"--smoke-editor"))
            { LocalFree(argv); return 2; }
            const bool smoke = std::wstring(argv[1]) == L"--smoke" || std::wstring(argv[1]) == L"--smoke-editor";
            const bool editor = std::wstring(argv[1]) == L"--check-editor" || std::wstring(argv[1]) == L"--smoke-editor";
            const std::wstring name = argv[2];
            LocalFree(argv); argv = nullptr;
            for (const auto& entry : Games)
                if (name == entry.directory) { Validate(entry, editor); return smoke ? Launch(entry, true, editor) : 0; }
            return 2;
        }
        LocalFree(argv); argv = nullptr;
        WNDCLASSW wc{};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"EngineLauncherWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassW(&wc)) throw std::runtime_error("Cannot register launcher window");
        HWND window = CreateWindowW(wc.lpszClassName, L"Engine Launcher", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 730, 430, nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("Cannot create launcher window");
        ShowWindow(window, show);
        MSG msg{};
        BOOL result;
        while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0)
            if (!IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        return result == -1 ? 1 : static_cast<int>(msg.wParam);
    }
    catch (const std::exception& error)
    {
        if (argv) LocalFree(argv);
        if (!diagnostic) MessageBoxW(nullptr, ExampleDeployment::Wide(error.what()).c_str(), L"Engine Launcher", MB_OK | MB_ICONERROR);
        OutputDebugStringW(ExampleDeployment::Wide(error.what()).c_str());
        return 1;
    }
}
