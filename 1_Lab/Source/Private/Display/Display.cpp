#include <windows.h>
#include <windowsx.h>
#include "../../Public/Display/Display.h"
#include "../../Public/MainGame/BaseGameClass/Game.h"
#include "../../Public/InputDevice/InputDevice.h"
#include <iostream>

namespace
{
    Game* GetGameFromHwnd(HWND hwnd)
    {
        return reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
}

Display::Display(int ScreenW, int ScreenH)
    : ScreenW(ScreenW), ScreenH(ScreenH)
{
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    InitWinClass();

    windowRect = { 0, 0, static_cast<LONG>(ScreenW), static_cast<LONG>(ScreenH) };

    // Registering an already registered class fails harmlessly (e.g. a second Game instance).
    RegisterClassEx(&wc);
    InitHwndClass();
}

void Display::InitWinClass()
{
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = applicationName;
    wc.cbSize = sizeof(WNDCLASSEX);
}

void Display::InitHwndClass()
{
    const DWORD dwStyle = WS_OVERLAPPEDWINDOW;

    // Grow the outer rectangle so that the client area has exactly the requested size.
    RECT outerRect = windowRect;
    AdjustWindowRectEx(&outerRect, dwStyle, FALSE, WS_EX_APPWINDOW);
    const int outerWidth = outerRect.right - outerRect.left;
    const int outerHeight = outerRect.bottom - outerRect.top;
    const int posX = (GetSystemMetrics(SM_CXSCREEN) - outerWidth) / 2;
    const int posY = (GetSystemMetrics(SM_CYSCREEN) - outerHeight) / 2;

    hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        applicationName,
        applicationName,
        dwStyle,
        posX, posY,
        outerWidth,
        outerHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
    {
        DWORD error = GetLastError();
        std::cout << "Failed to create window! Error code: " << error << std::endl;
        return;
    }

    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);
    ScreenW = clientRect.right - clientRect.left;
    ScreenH = clientRect.bottom - clientRect.top;
    windowRect = clientRect;
}

LRESULT Display::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Game* game = GetGameFromHwnd(hwnd);

    switch (msg)
    {
    case WM_INPUT:
    {
        InputDevice* inputDevice = game ? game->GetInputDevice() : nullptr;
        if (!inputDevice)
        {
            break;
        }

        UINT dwSize = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize == 0)
        {
            break;
        }

        // Reused between messages instead of allocating for every Raw Input event.
        static std::vector<BYTE> rawBuffer;
        if (rawBuffer.size() < dwSize)
        {
            rawBuffer.resize(dwSize);
        }

        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawBuffer.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        {
            break;
        }

        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(rawBuffer.data());
        if (raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            KeyboardInputEventArgs args;
            args.MakeCode = raw->data.keyboard.MakeCode;
            args.Flags = raw->data.keyboard.Flags;
            args.VKey = raw->data.keyboard.VKey;
            args.Message = raw->data.keyboard.Message;

            inputDevice->OnKeyDown(args);
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            RawMouseEventArgs args;
            args.Mode = raw->data.mouse.usFlags;
            args.ButtonFlags = raw->data.mouse.usButtonFlags;
            args.ExtraInformation = static_cast<int>(raw->data.mouse.ulExtraInformation);
            args.Buttons = static_cast<int>(raw->data.mouse.ulRawButtons);
            args.WheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
            args.X = raw->data.mouse.lLastX;
            args.Y = raw->data.mouse.lLastY;

            inputDevice->OnMouseMove(args);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_KILLFOCUS:
    {
        // Key-up events are not delivered to an unfocused window: forget everything that is held.
        if (game)
        {
            game->OnFocusLost();
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_SIZE:
    {
        if (game)
        {
            game->OnWindowResized(LOWORD(lParam), HIWORD(lParam), wParam == SIZE_MINIMIZED);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_DESTROY:
    {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

Display::~Display()
{
    if (hWnd && IsWindow(hWnd))
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        DestroyWindow(hWnd);
    }
    hWnd = nullptr;
}

void Display::SetGamePointer(Game* gamePtr)
{
    GamePointer = gamePtr;
    if (hWnd)
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(gamePtr));
    }
}

void Display::SetClientSize(int width, int height)
{
    ScreenW = width;
    ScreenH = height;
    windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}
