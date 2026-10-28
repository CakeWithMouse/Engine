#define NOMINMAX
#include "../Source/LaunchSplash.h"
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
