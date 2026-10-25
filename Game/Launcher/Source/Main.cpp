#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include "Catalog.h"
#include "../../Common/Deployment.h"
#include <iostream>
#include <shobjidl.h>
#include <shlobj.h>
#include <wrl/client.h>
#include "ProjectLibrary.h"

namespace
{
HWND list = nullptr;
HWND status = nullptr;
HWND editButton = nullptr;
constexpr INT_PTR ListId = 100;
constexpr INT_PTR LaunchId = 101;
constexpr INT_PTR EditId = 102;
constexpr INT_PTR CreateId = 103, OpenId = 104;
constexpr INT_PTR InstallEngineId = 105, UpgradeId = 106, PinEngineId = 107;
std::vector<std::filesystem::path> userProjects;

std::filesystem::path Pick(HWND owner, bool folder)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        throw std::runtime_error("Cannot open file chooser");
    DWORD options = 0; dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    if (!folder) { const COMDLG_FILTERSPEC filters[] = {{L"Engine project (Project.json)", L"Project.json"}}; dialog->SetFileTypes(1, filters); }
    if (dialog->Show(owner) == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) throw std::runtime_error("Cannot read selected path");
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) throw std::runtime_error("Not a filesystem path");
    std::filesystem::path result(path); CoTaskMemFree(path); return result;
}
std::wstring Text(HWND window, int id)
{
    const auto control = GetDlgItem(window, id);
    std::wstring text(GetWindowTextLengthW(control) + 1, L'\0');
    text.resize(GetWindowTextW(control, text.data(), static_cast<int>(text.size())));
    return text;
}
std::string Utf8(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Invalid project name");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
INT_PTR CALLBACK CreateDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG)
    {
        SetWindowLongPtrW(window, DWLP_USER, lParam);
        SetDlgItemTextW(window, 201, L"Мой проект");
        PWSTR documents = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents)))
        {
            const auto path = std::filesystem::path(documents) / L"Мой проект";
            SetDlgItemTextW(window, 202, path.c_str()); CoTaskMemFree(documents);
        }
        return TRUE;
    }
    if (message != WM_COMMAND) return FALSE;
    try
    {
        if (LOWORD(wParam) == IDCANCEL) { EndDialog(window, IDCANCEL); return TRUE; }
        if (LOWORD(wParam) == 203)
        {
            const auto parent = Pick(window, true);
            if (!parent.empty())
            {
                auto leaf = Text(window, 201);
                if (leaf.empty() || leaf == L"." || leaf == L".." || leaf.find_first_of(L"<>:\"/\\|?*") != std::wstring::npos) leaf = L"NewProject";
                SetDlgItemTextW(window, 202, (parent / leaf).c_str());
            }
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK)
        {
            const auto path = Text(window, 202);
            if (path.empty() || !std::filesystem::path(path).is_absolute()) throw std::runtime_error("Choose an absolute path for the new project folder");
            auto* output = reinterpret_cast<std::filesystem::path*>(GetWindowLongPtrW(window, DWLP_USER));
            *output = ProjectLibrary::Create(path, Utf8(Text(window, 201)), ExampleDeployment::ExecutableDirectory() / "Runtime/Templates/SceneProject/Game.dll");
            EndDialog(window, IDOK); return TRUE;
        }
    }
    catch (const std::exception& error) { MessageBoxW(window, ExampleDeployment::Wide(error.what()).c_str(), L"Проект не создан", MB_OK | MB_ICONERROR); }
    return FALSE;
}
std::filesystem::path SelectedProject()
{
    const auto index = SendMessageW(list, LB_GETCURSEL, 0, 0);
    return index >= static_cast<LRESULT>(Games.size()) ? userProjects.at(index - Games.size()) : std::filesystem::path{};
}
void ReloadProjects(const std::filesystem::path& selected = {})
{
    userProjects = ProjectLibrary::ReadRegistry();
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& game : Games) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(game.title));
    int selection = 0;
    for (size_t i = 0; i < userProjects.size(); ++i)
    {
        std::wstring title;
        try { title = L"Проект: " + ExampleDeployment::Wide(ReadRuntimeProject(userProjects[i]).name.c_str()); }
        catch (...) { title = L"Недоступен: " + userProjects[i].wstring(); }
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(title.c_str()));
        if (userProjects[i] == selected) selection = static_cast<int>(Games.size() + i);
    }
    SendMessageW(list, LB_SETCURSEL, selection, 0);
}
int LaunchProject(const std::filesystem::path& file, bool editor, bool smoke = false)
{
    const auto settings = ProjectLibrary::Validate(file);
    const auto runtime = ResolveEngineRelease(settings, ExampleDeployment::ExecutableDirectory() / "Runtime").runtime / "EngineRuntime.exe";
    if (!std::filesystem::is_regular_file(runtime)) throw std::runtime_error("EngineRuntime.exe is missing");
    const auto project = std::filesystem::absolute(file);
    std::wstring command = L"\"" + runtime.wstring() + L"\" --project \"" + project.wstring() + L"\" --mode " + (editor ? L"editor --scene-api" : L"game");
    if (smoke) command += L" --frames 3";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (!CreateProcessW(runtime.c_str(), command.data(), nullptr, nullptr, FALSE, smoke ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE,
        nullptr, runtime.parent_path().c_str(), &startup, &process)) throw std::runtime_error("Cannot start project, Windows error " + std::to_string(GetLastError()));
    CloseHandle(process.hThread);
    if (!smoke) { CloseHandle(process.hProcess); return 0; }
    if (WaitForSingleObject(process.hProcess, 60000) != WAIT_OBJECT_0)
    { TerminateProcess(process.hProcess, 124); CloseHandle(process.hProcess); throw std::runtime_error("Project smoke timed out"); }
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code); CloseHandle(process.hProcess); return static_cast<int>(code);
}

void UpgradeProject(const std::filesystem::path& file, const std::filesystem::path& destination)
{
    const auto runtime = ExampleDeployment::ExecutableDirectory() / "Runtime/EngineRuntime.exe";
    std::wstring command = L"\"" + runtime.wstring() + L"\" --upgrade-project \"" + file.wstring() + L"\" --destination \"" + destination.wstring() + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (!CreateProcessW(runtime.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,runtime.parent_path().c_str(),&startup,&process))
        throw std::runtime_error("Cannot start project upgrade");
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess,&code); CloseHandle(process.hProcess);
    if (code) throw std::runtime_error("Project copy could not be upgraded. Original preserved. Check unknown components, missing assets or custom DLL compatibility. Run EngineRuntime --upgrade-project for detailed diagnostics.");
}

std::filesystem::path GameDirectory(const GameEntry& entry)
{
    return ExampleDeployment::ExecutableDirectory() / entry.directory;
}

std::filesystem::path Validate(const GameEntry& entry, bool editor = false)
{
    if (editor && !entry.preview) throw std::runtime_error("Preview is unavailable for this project (Katamari requires Assimp and a scene adapter)");
    if (entry.runtime || editor)
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
    if (entry.runtime || editor)
    {
        const auto project = executable.parent_path() / "Projects" / entry.directory / "Project.json";
        command += L" --project \"" + project.wstring() + L"\" --mode " + (editor ? L"editor" : L"game");
        if (smoke) command += L" --frames 3";
    }
    else if (smoke) command += L" --smoke";
    if (entry.extraArgs && *entry.extraArgs)
    {
        command += L" ";
        command += entry.extraArgs;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Inherit the caller's CWD: games must locate content independently of it.
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        (entry.runtime || editor) && !smoke ? CREATE_NEW_CONSOLE : 0, nullptr, nullptr, &startup, &process))
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
    const auto projectFile = SelectedProject();
    if (!projectFile.empty())
    {
        try
        {
            const auto project = ProjectLibrary::Validate(projectFile);
            const auto release = ResolveEngineRelease(project, ExampleDeployment::ExecutableDirectory() / "Runtime");
            SetWindowTextW(status, (L"Движок: " + ExampleDeployment::Wide(release.version) + L" " + ExampleDeployment::Wide(release.configuration) +
                (project.engineVersion.empty() ? L" (старый проект, режим совместимости)" : L" (закреплён)") + L"\r\n" + projectFile.wstring()).c_str());
            EnableWindow(editButton, TRUE);
        }
        catch (const std::exception& error) { SetWindowTextW(status, ExampleDeployment::Wide(error.what()).c_str()); EnableWindow(editButton, FALSE); }
        return;
    }
    const auto& entry = Selected();
    std::wstring message;
    try
    {
        Validate(entry);
        message = L"Игра готова к запуску.";
    }
    catch (const std::exception& error)
    {
        message = ExampleDeployment::Wide(error.what()) + L" " + entry.buildHint;
    }
    bool previewReady = false;
    if (entry.preview)
    {
        try
        {
            Validate(entry, true);
            previewReady = true;
            message += L"\r\nРедактировать: preview сцены в Qt. Изменение объектов пока недоступно.";
        }
        catch (const std::exception& error)
        {
            message += L"\r\nPreview: " + ExampleDeployment::Wide(error.what());
        }
    }
    else message += L"\r\nPreview для этого режима пока не подключён.";
    EnableWindow(editButton, previewReady);
    SetWindowTextW(status, message.c_str());
}

LRESULT WindowProcImpl(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Примеры и ваши проекты", WS_CHILD | WS_VISIBLE, 18, 15, 600, 24, window, nullptr, nullptr, nullptr);
        list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL,
            18, 45, 670, 220, window, reinterpret_cast<HMENU>(ListId), nullptr, nullptr);
        ReloadProjects();
        CreateWindowW(L"BUTTON", L"Играть", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            18, 280, 130, 35, window, reinterpret_cast<HMENU>(LaunchId), nullptr, nullptr);
        editButton = CreateWindowW(L"BUTTON", L"Редактировать", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            160, 280, 160, 35, window, reinterpret_cast<HMENU>(EditId), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Создать проект", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            332, 280, 170, 35, window, reinterpret_cast<HMENU>(CreateId), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Открыть проект…", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            514, 280, 174, 35, window, reinterpret_cast<HMENU>(OpenId), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Добавить версию…", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            18, 325, 210, 30, window, reinterpret_cast<HMENU>(InstallEngineId), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Закрепить версию…", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            240, 325, 210, 30, window, reinterpret_cast<HMENU>(PinEngineId), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Обновить копию…", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            462, 325, 226, 30, window, reinterpret_cast<HMENU>(UpgradeId), nullptr, nullptr);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            18, 365, 670, 90, window, nullptr, nullptr, nullptr);
        UpdateStatus();
        return 0;
    case WM_ACTIVATE:
        if (list && LOWORD(wParam) != WA_INACTIVE) UpdateStatus();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == InstallEngineId || LOWORD(wParam) == PinEngineId || LOWORD(wParam) == UpgradeId)
        {
            try
            {
                const auto project = SelectedProject();
                if (LOWORD(wParam) != InstallEngineId && project.empty()) throw std::runtime_error("Select your project first");
                if (LOWORD(wParam) == UpgradeId)
                {
                    const auto parent = Pick(window,true);
                    if (parent.empty()) return 0;
                    const auto release = ReadEngineRelease(ExampleDeployment::ExecutableDirectory() / "Runtime");
                    auto destination = parent / (project.parent_path().filename().wstring() + L"-" + ExampleDeployment::Wide(release.version));
                    InstallEngineRelease(release.runtime);
                    UpgradeProject(project,destination);
                    const auto copy = destination / project.filename();
                    ProjectLibrary::Remember(copy); ReloadProjects(copy);
                }
                else
                {
                    auto folder = Pick(window,true);
                    if (folder.empty()) return 0;
                    if (std::filesystem::exists(folder / "Runtime/EngineRuntime.exe")) folder /= "Runtime";
                    const auto release = InstallEngineRelease(folder);
                    if (LOWORD(wParam) == PinEngineId)
                    {
                        const auto lock = project.parent_path() / "Engine.lock.json";
                        if (std::filesystem::exists(lock))
                            std::filesystem::copy_file(lock,lock.wstring() + L".backup-" + std::to_wstring(GetTickCount64()),std::filesystem::copy_options::none);
                        ProjectLibrary::Write(lock,{{"format",1},{"engineVersion",release.version},{"engineConfiguration",release.configuration}});
                    }
                }
                UpdateStatus();
            }
            catch (const std::exception& error) { MessageBoxW(window,ExampleDeployment::Wide(error.what()).c_str(),L"Версии движка",MB_OK | MB_ICONERROR); }
            return 0;
        }
        if (LOWORD(wParam) == CreateId || LOWORD(wParam) == OpenId)
        {
            try
            {
                std::filesystem::path project;
                if (LOWORD(wParam) == CreateId)
                {
                    if (DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(200), window, CreateDialogProc, reinterpret_cast<LPARAM>(&project)) != IDOK) return 0;
                }
                else project = Pick(window, false);
                if (project.empty()) return 0;
                ProjectLibrary::Validate(project);
                ProjectLibrary::Remember(project); ReloadProjects(project); UpdateStatus();
                LaunchProject(project, true);
            }
            catch (const std::exception& error) { MessageBoxW(window, ExampleDeployment::Wide(error.what()).c_str(), L"Ошибка проекта", MB_OK | MB_ICONERROR); }
            return 0;
        }
        if (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_SELCHANGE) { UpdateStatus(); return 0; }
        if (LOWORD(wParam) == LaunchId || LOWORD(wParam) == EditId || (LOWORD(wParam) == ListId && HIWORD(wParam) == LBN_DBLCLK))
        {
            try
            {
                const auto project = SelectedProject();
                if (project.empty()) Launch(Selected(), false, LOWORD(wParam) == EditId);
                else LaunchProject(project, LOWORD(wParam) != LaunchId);
                SetWindowTextW(status, L"Приложение запущено. Лаунчер можно закрыть.");
            }
            catch (const std::exception& error)
            {
                const auto text = ExampleDeployment::Wide(error.what()) + (SelectedProject().empty() ? L"\r\n" + std::wstring(Selected().buildHint) : L"");
                MessageBoxW(window, text.c_str(), L"Не удалось запустить игру", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    try { return WindowProcImpl(window, message, wParam, lParam); }
    catch (const std::exception& error)
    {
        MessageBoxW(window, ExampleDeployment::Wide(error.what()).c_str(), L"Engine Launcher", MB_OK | MB_ICONERROR);
        return message == WM_CREATE ? -1 : 0;
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComGuard { ~ComGuard() { CoUninitialize(); } } comGuard;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    bool diagnostic = argc > 1;
    try
    {
        if (diagnostic)
        {
            const std::wstring option = argv[1];
            if (option == L"--install-engine" && argc == 3)
            {
                auto folder = std::filesystem::absolute(argv[2]);
                if (std::filesystem::exists(folder / "Runtime/EngineRuntime.exe")) folder /= "Runtime";
                InstallEngineRelease(folder); LocalFree(argv); return 0;
            }
            if (option == L"--upgrade-project" && argc == 4)
            {
                UpgradeProject(std::filesystem::absolute(argv[2]),std::filesystem::absolute(argv[3]));
                LocalFree(argv); return 0;
            }
            if (option == L"--create-project" && argc == 4)
            {
                ProjectLibrary::Create(argv[2], Utf8(argv[3]), ExampleDeployment::ExecutableDirectory() / "Runtime/Templates/SceneProject/Game.dll");
                LocalFree(argv); return 0;
            }
            if (argc == 3 && (option == L"--check-project" || option == L"--smoke-project" || option == L"--smoke-project-editor"))
            {
                const std::filesystem::path file = argv[2]; LocalFree(argv); argv = nullptr;
                ProjectLibrary::Validate(file);
                return option == L"--check-project" ? 0 : LaunchProject(file, option == L"--smoke-project-editor", true);
            }
            if (argc != 3 || (std::wstring(argv[1]) != L"--check" && std::wstring(argv[1]) != L"--smoke" &&
                std::wstring(argv[1]) != L"--check-editor" && std::wstring(argv[1]) != L"--smoke-editor"))
            { LocalFree(argv); return 2; }
            const bool smoke = std::wstring(argv[1]) == L"--smoke" || std::wstring(argv[1]) == L"--smoke-editor";
            const bool editor = std::wstring(argv[1]) == L"--check-editor" || std::wstring(argv[1]) == L"--smoke-editor";
            const std::wstring name = argv[2];
            LocalFree(argv); argv = nullptr;
            for (const auto& entry : Games)
            {
                const bool hasExtra = (entry.extraArgs && *entry.extraArgs);
                if (name == entry.title ||
                    (hasExtra && (name == L"BattleAnim" || name == L"Battle-Anim" || name == L"Battle (Animated Crowd)" || name == (std::wstring(entry.directory) + L" " + entry.extraArgs))) ||
                    (!hasExtra && name == entry.directory))
                {
                    Validate(entry, editor);
                    return smoke ? Launch(entry, true, editor) : 0;
                }
            }
            return 2;
        }
        LocalFree(argv); argv = nullptr;
        InstallEngineRelease(ExampleDeployment::ExecutableDirectory() / "Runtime");
        WNDCLASSW wc{};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"EngineLauncherWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassW(&wc)) throw std::runtime_error("Cannot register launcher window");
        HWND window = CreateWindowW(wc.lpszClassName, L"Engine Launcher", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 730, 510, nullptr, nullptr, instance, nullptr);
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
