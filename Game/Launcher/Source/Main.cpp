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
#include "ProjectVersion.h"
#include "LaunchSplash.h"
#include "../../../Engine/Runtime/ProjectExport.h"

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
constexpr INT_PTR EnableCodeId = 108, BuildCodeId = 109;
constexpr INT_PTR ExportId = 112;
std::vector<std::filesystem::path> userProjects;

void RunProjectCodeTool(const std::filesystem::path& project, const wchar_t* action)
{
    const auto release = ResolveEngineRelease(ReadRuntimeProject(project), ExampleDeployment::ExecutableDirectory() / "Runtime");
    const auto script = release.runtime / "SDK/ProjectTools.ps1";
    if (!std::filesystem::is_regular_file(script)) throw std::runtime_error("This engine has no C++ SDK. Upgrade a project copy first.");
    auto command = std::wstring(L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"") + script.wstring() +
        L"\" -Action " + action + L" -Project \"" + project.wstring() + L"\" -Runtime \"" + release.runtime.wstring() + L"\"";
    const auto logPath = project.parent_path() / "CodeBuild.log";
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE log = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create CodeBuild.log");
    STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = log;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const auto started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, project.parent_path().c_str(), &startup, &process);
    CloseHandle(log);
    if (!started) throw std::runtime_error("Cannot start C++ project tools");
    const auto wait = WaitForSingleObject(process.hProcess, 600000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000); }
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if (code || wait != WAIT_OBJECT_0) throw std::runtime_error("C++ build failed. Existing project preserved. See CodeBuild.log in the project folder.");
}

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
            std::string buildWarning;
            {
                LaunchSplash splash(L"Создание проекта…", true, window);
                *output = ProjectLibrary::Create(path, Utf8(Text(window, 201)), ExampleDeployment::ExecutableDirectory() / "Runtime/Templates/SceneProject/Game.dll");
                if (IsDlgButtonChecked(window,204) == BST_CHECKED) {
                    try { RunProjectCodeTool(*output,L"Enable"); }
                    catch (const std::exception& error) { buildWarning = error.what(); }
                }
            }
            if (!buildWarning.empty()) MessageBoxW(window, ExampleDeployment::Wide(buildWarning.c_str()).c_str(),
                L"Проект создан; C++ не собран. Открывается прежний модуль", MB_OK | MB_ICONWARNING);
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
void UpgradeProject(const std::filesystem::path& file, const std::filesystem::path& destination);
void UpdateStatus();
struct UpgradePrompt { std::wstring text; bool previousAvailable; };
INT_PTR CALLBACK UpgradeDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(message==WM_INITDIALOG) {
        const auto& prompt=*reinterpret_cast<const UpgradePrompt*>(lParam);
        SetDlgItemTextW(window,211,prompt.text.c_str());
        EnableWindow(GetDlgItem(window,IDNO),prompt.previousAvailable);
        return TRUE;
    }
    if(message==WM_COMMAND && (LOWORD(wParam)==IDYES || LOWORD(wParam)==IDNO || LOWORD(wParam)==IDCANCEL)) {
        EndDialog(window,LOWORD(wParam));return TRUE;
    }
    if(message==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
    return FALSE;
}
int LaunchProject(const std::filesystem::path& file, bool editor, bool smoke = false, bool currentEngine = false, bool promptInSmoke = false)
{
    auto project = std::filesystem::absolute(file);
    auto settings = ProjectLibrary::Validate(project);
    const auto current = ExampleDeployment::ExecutableDirectory() / "Runtime";
    const auto release = ReadEngineRelease(current);
    if(!currentEngine && (!smoke || promptInSmoke) && ProjectVersion::IsNewer(release.version,settings.engineVersion)) {
        bool previousAvailable=false;
        try { ResolveEngineRelease(settings,current);previousAvailable=true; } catch(const std::exception&) {}
        const auto destination=ProjectVersion::CopyDestination(project,release.version);
        UpgradePrompt prompt{L"Проект: "+ExampleDeployment::Wide(settings.name)+L"\r\nВерсия проекта: "+ExampleDeployment::Wide(settings.engineVersion)+
            L"\r\nНовая версия движка: "+ExampleDeployment::Wide(release.version)+
            L"\r\n\r\nОбновить проект для запуска на новой версии? Будет создана и проверена копия. Исходный проект сохранится.\r\n\r\nПапка копии:\r\n"+destination.wstring(),previousAvailable};
        if(!previousAvailable)prompt.text+=L"\r\n\r\nПрежняя версия движка не установлена.";
        const auto choice=DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(210),GetActiveWindow(),UpgradeDialogProc,reinterpret_cast<LPARAM>(&prompt));
        if(choice==-1)throw std::runtime_error("Cannot show project upgrade dialog");
        if(choice!=IDYES && choice!=IDNO){if(status)SetWindowTextW(status,L"Запуск отменён.");return 0;}
        if(choice==IDYES) {
            { LaunchSplash upgrading(L"Обновление копии проекта…",!smoke,GetActiveWindow()); UpgradeProject(project,destination); }
            project=destination/project.filename();settings=ProjectLibrary::Validate(project);
            ProjectLibrary::Remember(project);if(list){ReloadProjects(project);UpdateStatus();}
            currentEngine=true;
        }
    }
    LaunchSplash splash(editor ? L"Запуск редактора…" : L"Запуск игры…", !smoke, GetActiveWindow());
    LaunchReadiness readiness;
    const auto runtime = (currentEngine ? release : ResolveEngineRelease(settings, current)).runtime / "EngineRuntime.exe";
    if (!std::filesystem::is_regular_file(runtime)) throw std::runtime_error("EngineRuntime.exe is missing");
    std::wstring command = L"\"" + runtime.wstring() + L"\" --project \"" + project.wstring() + L"\" --mode " + (editor ? L"editor --scene-api" : L"game");
    if (smoke) command += L" --frames 3";
    if (currentEngine) command += L" --compatible-engine";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (!CreateProcessW(runtime.c_str(), command.data(), nullptr, nullptr, FALSE, smoke ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE,
        nullptr, runtime.parent_path().c_str(), &startup, &process)) throw std::runtime_error("Cannot start project, Windows error " + std::to_string(GetLastError()));
    CloseHandle(process.hThread);
    if (!smoke) { try { readiness.Wait(process.hProcess,process.dwProcessId); } catch (...) { CloseHandle(process.hProcess); throw; } CloseHandle(process.hProcess); if(status)SetWindowTextW(status,L"Приложение запущено. Лаунчер можно закрыть."); return 0; }
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
    LaunchSplash splash(editor ? L"Запуск редактора…" : L"Запуск примера…", !smoke, GetActiveWindow());
    LaunchReadiness readiness;
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
    if (!smoke) { try { readiness.Wait(process.hProcess,process.dwProcessId); } catch (...) { CloseHandle(process.hProcess); throw; } CloseHandle(process.hProcess); return 0; }
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
            const auto current=ReadEngineRelease(ExampleDeployment::ExecutableDirectory()/"Runtime");
            if(ProjectVersion::IsNewer(current.version,project.engineVersion)) {
                SetWindowTextW(status,(L"Версия проекта: "+ExampleDeployment::Wide(project.engineVersion)+L". Доступна новая: "+ExampleDeployment::Wide(current.version)+L".\r\nПри запуске будет предложено обновление копии.").c_str());
                EnableWindow(editButton,TRUE);return;
            }
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
    {
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
        auto* bar = CreateMenu();
        auto* projectMenu = CreatePopupMenu();
        AppendMenuW(projectMenu,MF_STRING,CreateId,L"Создать проект…");
        AppendMenuW(projectMenu,MF_STRING,OpenId,L"Открыть проект…");
        AppendMenuW(projectMenu,MF_STRING,ExportId,L"Экспорт игры на текущем движке…");
        auto* codeMenu = CreatePopupMenu();
        AppendMenuW(codeMenu,MF_STRING,EnableCodeId,L"Добавить C++…");
        AppendMenuW(codeMenu,MF_STRING,BuildCodeId,L"Собрать C++ проект");
        AppendMenuW(projectMenu,MF_POPUP,reinterpret_cast<UINT_PTR>(codeMenu),L"C++");
        auto* versions = CreatePopupMenu();
        AppendMenuW(versions,MF_STRING,InstallEngineId,L"Добавить версию…");
        AppendMenuW(versions,MF_STRING,PinEngineId,L"Закрепить версию проекта…");
        AppendMenuW(versions,MF_STRING,UpgradeId,L"Обновить копию проекта…");
        AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(projectMenu),L"Проект");
        AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(versions),L"Версии движка");
        SetMenu(window,bar);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            18, 335, 670, 70, window, nullptr, nullptr, nullptr);
        UpdateStatus();
        return 0;
    }
    case WM_ACTIVATE:
        if (list && LOWORD(wParam) != WA_INACTIVE) UpdateStatus();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ExportId) {
            try {
                const auto project=SelectedProject(); if(project.empty())throw std::runtime_error("Select your project first");
                const auto parent=Pick(window,true); if(parent.empty())return 0;
                const auto target=parent/(project.parent_path().filename().wstring()+L"-Export");
                LaunchSplash splash(L"Проверка и экспорт игры…",true,window);
                ProjectExport::CheckProcess(ExampleDeployment::ExecutableDirectory()/"Runtime/EngineRuntime.exe",
                    L"--export-project \""+project.wstring()+L"\" --destination \""+target.wstring()+L"\"",project.parent_path()/"Export.log");
                SetWindowTextW(status,(L"Готовая игра: "+target.wstring()+L". Запуск: Start.cmd").c_str());
            } catch(const std::exception& e){MessageBoxW(window,ExampleDeployment::Wide(e.what()).c_str(),L"Экспорт",MB_OK|MB_ICONERROR);}
            return 0;
        }
        if (LOWORD(wParam) == EnableCodeId || LOWORD(wParam) == BuildCodeId)
        {
            try {
                const auto project = SelectedProject();
                if (project.empty()) throw std::runtime_error("Select your project first");
                SetWindowTextW(status,L"C++: сборка проекта. Лог: CodeBuild.log");
                LaunchSplash splash(L"Сборка C++ проекта…", true, window);
                RunProjectCodeTool(project, LOWORD(wParam) == EnableCodeId ? L"Enable" : L"Build");
                SetWindowTextW(status,L"Сборка проверена. Откройте редактор заново.");
            } catch (const std::exception& error) { MessageBoxW(window,ExampleDeployment::Wide(error.what()).c_str(),L"C++",MB_OK|MB_ICONERROR); }
            return 0;
        }
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
                if (project.empty()) { Launch(Selected(), false, LOWORD(wParam) == EditId);SetWindowTextW(status, L"Приложение запущено. Лаунчер можно закрыть."); }
                else LaunchProject(project, LOWORD(wParam) != LaunchId);
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
            if(argc==3 && (option==L"--smoke-project-prompt" || option==L"--smoke-project-editor-prompt")) {
                const std::filesystem::path file=argv[2];LocalFree(argv);argv=nullptr;
                return LaunchProject(file,option==L"--smoke-project-editor-prompt",true,false,true);
            }
            if (argc == 3 && (option == L"--smoke-project-current" || option == L"--smoke-project-editor-current"))
            {
                const std::filesystem::path file = argv[2]; LocalFree(argv); argv = nullptr;
                return LaunchProject(file, option == L"--smoke-project-editor-current", true, true);
            }
            if ((option == L"--enable-code" || option == L"--build-project") && argc == 3)
            {
                RunProjectCodeTool(std::filesystem::absolute(argv[2]),option == L"--enable-code" ? L"Enable" : L"Build");
                LocalFree(argv); return 0;
            }
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
