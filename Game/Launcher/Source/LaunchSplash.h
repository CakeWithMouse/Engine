#pragma once
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <thread>
#include <future>
#include <memory>
#include <string>
#include <stdexcept>

// A separate UI thread keeps the loading screen responsive during SDK/build work.
// The owner is disabled until the operation succeeds or unwinds with an error.
class LaunchSplash final {
    HWND window_ = nullptr, owner_ = nullptr;
    bool restoreOwner_ = false;
    std::thread thread_;
    std::wstring title_;
    std::unique_ptr<Gdiplus::Bitmap> cover_;
    unsigned tick_ = 0;
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<LaunchSplash*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<LaunchSplash*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window,message,w,l);
        if (message == WM_TIMER) { ++self->tick_; InvalidateRect(window,nullptr,FALSE); return 0; }
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_CLOSE) return 0; // Operation owns lifetime; closing cannot cancel a build safely.
        if (message == WM_APP + 1) { DestroyWindow(window); return 0; }
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(window,&paint);
            RECT rect{}; GetClientRect(window,&rect);
            Gdiplus::Bitmap frame(rect.right,rect.bottom,PixelFormat32bppARGB);
            Gdiplus::Graphics g(&frame);
            g.Clear(Gdiplus::Color(255,7,18,28));
            if (self->cover_) g.DrawImage(self->cover_.get(),0,0,rect.right,rect.bottom);
            Gdiplus::SolidBrush shade(Gdiplus::Color(145,0,7,15));
            g.FillRectangle(&shade,0,0,rect.right,rect.bottom);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            Gdiplus::FontFamily family(L"Segoe UI");
            Gdiplus::Font heading(&family,36,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
            Gdiplus::Font label(&family,20,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
            Gdiplus::SolidBrush white(Gdiplus::Color(255,240,248,255));
            g.DrawString(L"PainEngine",-1,&heading,Gdiplus::PointF(44,50),&white);
            g.DrawString(self->title_.c_str(),-1,&label,Gdiplus::RectF(44,305,700,55),nullptr,&white);
            Gdiplus::SolidBrush track(Gdiplus::Color(120,120,160,180)), cyan(Gdiplus::Color(255,30,215,239));
            g.FillRectangle(&track,44,389,712,4);
            const int step = static_cast<int>(self->tick_ % 160);
            g.FillRectangle(&cyan,44 + (step < 80 ? step : 160-step)*7,389,150,4);
            Gdiplus::Graphics target(dc); target.DrawImage(&frame,0,0);
            EndPaint(window,&paint); return 0;
        }
        return DefWindowProcW(window,message,w,l);
    }
public:
    explicit LaunchSplash(const wchar_t* title, bool enabled = true, HWND owner = nullptr) : owner_(owner), title_(title) {
        if (!enabled) return;
        restoreOwner_ = owner_ && IsWindowEnabled(owner_);
        if (restoreOwner_) EnableWindow(owner_,FALSE);
        std::promise<HWND> ready; auto future = ready.get_future();
        try {
            thread_ = std::thread([this, ready = std::move(ready)]() mutable {
                ULONG_PTR token = 0; Gdiplus::GdiplusStartupInput input;
                Gdiplus::GdiplusStartup(&token,&input,nullptr);
                IStream* stream = nullptr;
                const auto resource = FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(301),RT_RCDATA);
                if (resource) {
                    const auto size = SizeofResource(GetModuleHandleW(nullptr),resource);
                    const auto data = LockResource(LoadResource(GetModuleHandleW(nullptr),resource));
                    auto buffer = GlobalAlloc(GMEM_MOVEABLE,size);
                    if (buffer) {
                        auto memory = GlobalLock(buffer);
                        if (memory) { memcpy(memory,data,size); GlobalUnlock(buffer); }
                        if (memory && SUCCEEDED(CreateStreamOnHGlobal(buffer,TRUE,&stream))) {
                            cover_.reset(Gdiplus::Bitmap::FromStream(stream));
                        } else GlobalFree(buffer);
                    }
                }
                WNDCLASSW cls{}; cls.lpfnWndProc = Proc; cls.hInstance = GetModuleHandleW(nullptr);
                cls.lpszClassName = L"PainEngineLoading"; cls.hCursor = LoadCursorW(nullptr,IDC_ARROW);
                RegisterClassW(&cls);
                const auto window = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,L"PainEngine — загрузка",
                    WS_POPUP|WS_VISIBLE,(GetSystemMetrics(SM_CXSCREEN)-800)/2,(GetSystemMetrics(SM_CYSCREEN)-450)/2,
                    800,450,nullptr,nullptr,cls.hInstance,this);
                if (window) { SetTimer(window,1,25,nullptr); UpdateWindow(window); }
                ready.set_value(window);
                if (window) { MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
                cover_.reset(); if (stream) stream->Release(); if (token) Gdiplus::GdiplusShutdown(token);
            });
            window_ = future.get();
        } catch (...) { if (restoreOwner_) EnableWindow(owner_,TRUE); throw; }
    }
    ~LaunchSplash() {
        if (window_) PostMessageW(window_,WM_APP+1,0,0);
        if (thread_.joinable()) thread_.join();
        if (restoreOwner_) EnableWindow(owner_,TRUE);
    }
    LaunchSplash(const LaunchSplash&) = delete;
    LaunchSplash& operator=(const LaunchSplash&) = delete;
};

// New runtimes signal after their first viewport frame. Older pinned releases
// fall back to a visible non-console window. Never terminate a user launch on timeout.
class LaunchReadiness final {
    HANDLE event_ = nullptr;
    std::wstring previous_;
public:
    LaunchReadiness() {
        wchar_t previous[512]{}; GetEnvironmentVariableW(L"PAINENGINE_READY_EVENT",previous,512); previous_=previous;
        const auto name = L"Local\\PainEngine.Ready." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
        event_=CreateEventW(nullptr,TRUE,FALSE,name.c_str());
        if (event_) SetEnvironmentVariableW(L"PAINENGINE_READY_EVENT",name.c_str());
    }
    ~LaunchReadiness() {
        SetEnvironmentVariableW(L"PAINENGINE_READY_EVENT",previous_.empty()?nullptr:previous_.c_str());
        if(event_) CloseHandle(event_);
    }
    void Wait(HANDLE process, DWORD pid) {
        const auto deadline=GetTickCount64()+90000;
        while(GetTickCount64()<deadline) {
            if(event_ && WaitForSingleObject(event_,50)==WAIT_OBJECT_0) return;
            if(WaitForSingleObject(process,50)==WAIT_OBJECT_0) {
                DWORD code=0;GetExitCodeProcess(process,&code);
                throw std::runtime_error("Application exited during startup (code " + std::to_string(code) + ")");
            }
            struct Search { DWORD pid; bool found; } search{pid,false};
            EnumWindows([](HWND w,LPARAM p)->BOOL {
                auto& s=*reinterpret_cast<Search*>(p); DWORD id=0;GetWindowThreadProcessId(w,&id);
                wchar_t cls[128]{};GetClassNameW(w,cls,128);
                RECT rect{};GetClientRect(w,&rect);
                if(id==s.pid && IsWindowVisible(w) && wcscmp(cls,L"ConsoleWindowClass") && rect.right>200 && rect.bottom>150) s.found=true;
                return TRUE;
            },reinterpret_cast<LPARAM>(&search));
            // Compatibility fallback only after allowing an actual readiness signal.
            if(search.found && GetTickCount64()+85000<deadline) continue;
            if(search.found) return;
        }
        throw std::runtime_error("Startup is taking longer than expected. The process is still running; check its console.");
    }
};
