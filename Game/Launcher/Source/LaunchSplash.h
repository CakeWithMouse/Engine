#pragma once
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <thread>
#include <future>
#include <memory>
#include <string>
#include <stdexcept>

#ifndef IDI_ICON
#define IDI_ICON 101
#endif

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
            const int w = rect.right;
            const int h = rect.bottom;
            Gdiplus::Bitmap frame(w,h,PixelFormat32bppARGB);
            Gdiplus::Graphics g(&frame);
            g.Clear(Gdiplus::Color(255,10,4,8));
            if (self->cover_) {
                const float imgW = static_cast<float>(self->cover_->GetWidth());
                const float imgH = static_cast<float>(self->cover_->GetHeight());
                if (imgW > 0.0f && imgH > 0.0f) {
                    const float winAspect = static_cast<float>(w) / static_cast<float>(h);
                    const float imgAspect = imgW / imgH;
                    float srcX = 0.0f, srcY = 0.0f, srcW = imgW, srcH = imgH;
                    if (imgAspect > winAspect) {
                        srcW = imgH * winAspect;
                        srcX = (imgW - srcW) * 0.5f;
                    } else {
                        srcH = imgW / winAspect;
                        srcY = (imgH - srcH) * 0.5f;
                    }
                    g.DrawImage(self->cover_.get(), Gdiplus::RectF(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)),
                        srcX, srcY, srcW, srcH, Gdiplus::UnitPixel);
                }
            }
            Gdiplus::SolidBrush shade(Gdiplus::Color(65,0,0,0));
            g.FillRectangle(&shade,0,0,w,h);

            Gdiplus::SolidBrush textBackdrop(Gdiplus::Color(90, 8, 3, 5));
            g.FillRectangle(&textBackdrop, 0, h - 160, 560, 160);

            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            Gdiplus::FontFamily family(L"Segoe UI");
            Gdiplus::Font heading(&family,32,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
            Gdiplus::Font label(&family,18,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
            Gdiplus::SolidBrush white(Gdiplus::Color(255,245,248,255));
            Gdiplus::SolidBrush textMuted(Gdiplus::Color(255,215,222,230));

            g.DrawString(L"PainEngine",-1,&heading,Gdiplus::PointF(48.0f,42.0f),&white);

            const float barX = 48.0f;
            const float barW = 460.0f;
            const float barH = 4.0f;
            const float barY = static_cast<float>(h - 58);

            g.DrawString(self->title_.c_str(),-1,&label,Gdiplus::RectF(barX,barY - 32.0f,barW,28.0f),nullptr,&textMuted);

            Gdiplus::SolidBrush track(Gdiplus::Color(140,80,60,70)), red(Gdiplus::Color(255,220,38,38));
            g.FillRectangle(&track,barX,barY,barW,barH);
            const float runnerW = 110.0f;
            const float travel = barW - runnerW;
            const int period = 140;
            const int step = static_cast<int>(self->tick_ % period);
            const float runnerX = barX + ((step < period / 2) ? (step * travel / (period / 2)) : ((period - step) * travel / (period / 2)));
            g.FillRectangle(&red,runnerX,barY,runnerW,barH);

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
                WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls);
                cls.lpfnWndProc = Proc; cls.hInstance = GetModuleHandleW(nullptr);
                cls.lpszClassName = L"PainEngineLoading"; cls.hCursor = LoadCursorW(nullptr,IDC_ARROW);
                cls.hIcon = LoadIconW(cls.hInstance, MAKEINTRESOURCEW(IDI_ICON));
                cls.hIconSm = static_cast<HICON>(LoadImageW(cls.hInstance, MAKEINTRESOURCEW(IDI_ICON), IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
                if (!cls.hIcon) cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
                RegisterClassExW(&cls);
                const int splashW = 960;
                const int splashH = 540;
                const auto window = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,L"PainEngine — загрузка",
                    WS_POPUP|WS_VISIBLE,(GetSystemMetrics(SM_CXSCREEN)-splashW)/2,(GetSystemMetrics(SM_CYSCREEN)-splashH)/2,
                    splashW,splashH,nullptr,nullptr,cls.hInstance,this);
                if (window) {
                    if (cls.hIcon) SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(cls.hIcon));
                    if (cls.hIconSm) SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(cls.hIconSm));
                    SetTimer(window,1,25,nullptr); UpdateWindow(window);
                }
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
