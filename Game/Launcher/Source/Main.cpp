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
constexpr INT_PTR ListId = 100;
constexpr INT_PTR LaunchId = 101;

std::filesystem::path GameDirectory(const GameEntry& entry)
{
    return ExampleDeployment::ExecutableDirectory() / entry.directory;
}

std::filesystem::path Validate(const GameEntry& entry)
{
    const auto directory = GameDirectory(entry);
    const auto executable = directory / (std::wstring(entry.directory) + L".exe");
    if (!std::filesystem::is_regular_file(executable))
        throw std::runtime_error("Game is not built: " + executable.u8string());
    ExampleDeployment::ValidateContent(directory);
    return executable;
}

int Launch(const GameEntry& entry, bool smoke)
{
    const auto executable = Validate(entry);
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    if (smoke) command += L" --smoke";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Inherit the caller's CWD: games must locate content independently of it.
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &startup, &process))
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
    try
    {
        Validate(entry);
        SetWindowTextW(status, L"Готово к запуску. Игра откроется отдельным процессом.");
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
        CreateWindowW(L"BUTTON", L"Запустить", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            18, 205, 155, 35, window, reinterpret_cast<HMENU>(LaunchId), nullptr, nullptr);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            18, 255, 670, 120, window, nullptr, nullptr, nullptr);
        UpdateStatus();
        return 0;
    case WM_ACTIVATE:
        if (list && LOWORD(wParam) != WA_INACTIVE) UpdateStatus();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_SELCHANGE) { UpdateStatus(); return 0; }
        if (LOWORD(wParam) == LaunchId || (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_DBLCLK))
        {
            try
            {
                Launch(Selected(), false);
                SetWindowTextW(status, L"Игра запущена отдельным процессом. Лаунчер можно закрыть.");
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
            if (argc != 3 || (std::wstring(argv[1]) != L"--check" && std::wstring(argv[1]) != L"--smoke"))
            { LocalFree(argv); return 2; }
            const bool smoke = std::wstring(argv[1]) == L"--smoke";
            const std::wstring name = argv[2];
            LocalFree(argv); argv = nullptr;
            for (const auto& entry : Games)
                if (name == entry.directory) { Validate(entry); return smoke ? Launch(entry, true) : 0; }
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
