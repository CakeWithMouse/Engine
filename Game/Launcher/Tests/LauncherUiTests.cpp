#define NOMINMAX
#include "../Source/LaunchSplash.h"
#include "../Source/ProjectLibrary.h"
#include <filesystem>
#include <iostream>
#include <vector>

int wmain(int argc,wchar_t** argv) {
    if(argc==2 && std::wstring(argv[1])==L"--ready") {
        wchar_t name[512]{};GetEnvironmentVariableW(L"PAINENGINE_READY_EVENT",name,512);
        HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);if(!event)return 2;
        SetEvent(event);CloseHandle(event);Sleep(100);return 0;
    }
    if(argc==2 && std::wstring(argv[1])==L"--fail")return 17;
    int checks=0;
    auto check=[&](bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;};
    try {
        check(argc==2,"usage: LauncherUiTests screenshot.png");
        const auto fixture = std::filesystem::temp_directory_path() /
            (L"LauncherCatalog-" + std::to_wstring(GetCurrentProcessId()));
        check(std::filesystem::create_directory(fixture), "exclusive catalog fixture");
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{fixture};
        check(ProjectLibrary::CollectProjects(fixture, {}).empty(), "absent bundle folders allowed");
        for (const auto* relative : {L"Examples/City", L"Examples/Bomber", L"Tutorials/Lab", L"Examples/Incomplete"})
            std::filesystem::create_directories(fixture / relative);
        for (const auto* relative : {L"Examples/City/Project.json", L"Examples/Bomber/Project.json", L"Tutorials/Lab/Project.json"})
            std::ofstream(fixture / relative) << "{}";
        const auto personal = fixture / "Personal/Project.json";
        const auto projects = ProjectLibrary::CollectProjects(fixture,
            {fixture / "Examples/City/Project.json", personal, fixture / "EXAMPLES/CITY/Project.json"});
        check(projects.size() == 4, "bundle discovery merges registry without duplicates");
        check(projects[0] == fixture / "Examples/Bomber/Project.json" && projects[2] == fixture / "Tutorials/Lab/Project.json",
            "examples and tutorials have deterministic ordering");
        check(projects.back() == personal, "personal entries retained after bundled projects");
        const auto moved = fixture / L"Перенесённая сборка";
        std::filesystem::create_directory(moved);
        std::filesystem::rename(fixture / "Examples", moved / "Examples");
        const auto relocated = ProjectLibrary::CollectProjects(moved, {});
        check(relocated.size() == 2 && relocated[0] == moved / "Examples/Bomber/Project.json", "relocated bundle needs no registry");
        EngineRelease current{"2026.11.07.6", "Release", moved / "Runtime"};
        RuntimeProject bundled;
        bundled.file = relocated[0]; bundled.engineVersion = current.version; bundled.engineConfiguration = current.configuration;
        check(ProjectLibrary::UsesBundledRuntime(bundled, current), "bundled project prefers portable runtime over installed copy");
        bundled.file = moved / "Tutorials/Lab/Project.json";
        check(ProjectLibrary::UsesBundledRuntime(bundled, current), "tutorial prefers portable runtime");
        bundled.engineVersion = "2026.10.31.3";
        check(!ProjectLibrary::UsesBundledRuntime(bundled, current), "different version retains engine pin");
        bundled.engineVersion = current.version; bundled.engineConfiguration = "Debug";
        check(!ProjectLibrary::UsesBundledRuntime(bundled, current), "different configuration retains engine pin");
        bundled.engineConfiguration = current.configuration; bundled.file = personal;
        check(!ProjectLibrary::UsesBundledRuntime(bundled, current), "personal project retains installed engine selection");
        const auto owner=CreateWindowW(L"STATIC",L"Test owner",WS_OVERLAPPEDWINDOW,0,0,800,450,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(owner!=nullptr,"owner created");
        {
            LaunchSplash splash(L"Создание проекта…",true,owner);
            auto window=FindWindowW(L"PainEngineLoading",nullptr);
            check(window && IsWindowVisible(window),"preloader visible");
            check(!IsWindowEnabled(owner),"owner disabled during work");
            RECT rect{};GetClientRect(window,&rect);auto dc=GetDC(window);
            auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,rect.right,rect.bottom);
            auto previous=SelectObject(memory,bitmap);
            check(PrintWindow(window,memory,PW_CLIENTONLY)!=0,"paint preloader preview");
            SelectObject(memory,previous);
            ULONG_PTR token=0;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);
            {
                Gdiplus::Bitmap image(bitmap,nullptr);UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
                std::vector<unsigned char> buffer(size);auto* codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
                Gdiplus::GetImageEncoders(count,size,codecs);bool saved=false;
                for(UINT i=0;i<count;++i)if(wcscmp(codecs[i].MimeType,L"image/png")==0)saved=image.Save(argv[1],&codecs[i].Clsid,nullptr)==Gdiplus::Ok;
                check(saved,"save preview");
            }
            Gdiplus::GdiplusShutdown(token);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);
        }
        check(!FindWindowW(L"PainEngineLoading",nullptr) && IsWindowEnabled(owner),"success closes splash and restores owner");
        try { LaunchSplash splash(L"Запуск проекта…",true,owner);throw std::runtime_error("test"); } catch(...) {}
        check(!FindWindowW(L"PainEngineLoading",nullptr) && IsWindowEnabled(owner),"failure closes splash and restores owner");
        DestroyWindow(owner);
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        for(bool fail:{false,true}) {
            LaunchReadiness ready;std::wstring command=L"\""+std::wstring(executable)+L"\" "+(fail?L"--fail":L"--ready");
            STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
            check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=0,"spawn readiness probe");
            bool failed=false;try{ready.Wait(process.hProcess,process.dwProcessId);}catch(...){failed=true;}
            check(failed==fail,"readiness handshake and startup failure");
            WaitForSingleObject(process.hProcess,1000);CloseHandle(process.hThread);CloseHandle(process.hProcess);
        }
        std::cout<<"Launcher UI: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
