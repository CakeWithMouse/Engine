#include <windows.h>
#include "../../Public/Display/Display.h"
#include "../../Public/MainGame/BaseGameClass/Game.h"
#include "../../Public/InputDevice/InputDevice.h"
#include <iostream>

//Game* Display::GamePointer = nullptr;
namespace
{
    Game* GetGameFromHwnd(HWND hwnd)
    {
        return reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    /*LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
    {
        Game* game = GetGameFromHwnd(hwnd);
        
        switch (umessage)
        {
        case WM_KEYDOWN:
            {
                std::cout << "Key: " << static_cast<unsigned int>(wparam) << std::endl;
                if (static_cast<unsigned int>(wparam) == 27) PostQuitMessage(0);
                return 0;
            }
        case WM_DESTROY:
            {
                PostQuitMessage(0);
                return 0;
            }
        default:
            {
                return DefWindowProc(hwnd, umessage, wparam, lparam);
            }
        }
    }*/
}

Display::Display(int ScreenW, int ScreenH)
    : ScreenW(ScreenW), ScreenH(ScreenH)
{
    // Инициализируем структуру класса окна
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    InitWinClass();
    
    // Устанавливаем прямоугольник окна
    windowRect = { 0, 0, static_cast<LONG>(ScreenW), static_cast<LONG>(ScreenH) };
    
    // ВАЖНО: Регистрируем класс окна
    RegisterClassEx(&wc);
    
    // ВАЖНО: Создаем окно!
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
    const DWORD dwStyle = WS_OVERLAPPEDWINDOW;  // Используем стандартный стиль для простоты
    const int posX = (GetSystemMetrics(SM_CXSCREEN) - ScreenW) / 2;
    const int posY = (GetSystemMetrics(SM_CYSCREEN) - ScreenH) / 2;
    
    hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        applicationName,
        applicationName,
        dwStyle,
        posX, posY,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
    
    if (!hWnd)
    {
        DWORD error = GetLastError();
        std::cout << "Failed to create window! Error code: " << error << std::endl;
    }
}

LRESULT Display::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Получаем указатель на Game из данных окна
    Game* game = GetGameFromHwnd(hwnd);
    
    switch (msg)
    {
    case WM_INPUT:
    {
        if (!game) break;
        
        InputDevice* inputDevice = game->GetInputDevice();
        if (!inputDevice) break;
        
        UINT dwSize = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        
        if (dwSize == 0) break;
        
        LPBYTE lpb = new BYTE[dwSize];
        if (!lpb) break;
        
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        {
            delete[] lpb;
            break;
        }
        
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb);
        bool MouseUse = false;
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
            MouseUse = true;
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
            /*if (!MouseUse)
            {
                inputDevice->MouseOffset = glm::vec2(0.f,0.f);
                //MouseOffset		= glm::vec2(args.X, args.Y); 
            }*/
        
        delete[] lpb;
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    case WM_KEYDOWN:
    {
        // Для отладки
        // std::cout << "Key: " << static_cast<unsigned int>(wParam) << std::endl;
        
        // Escape для выхода
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    
    case WM_SIZE:
    {
        // Уведомляем об изменении размера окна
        if (game && game->GetDisplay())
        {
            // Можно добавить обработку изменения размера
            RECT rect;
            GetClientRect(hwnd, &rect);
            // Обновляем viewport и projection matrix
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    default:
    {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Деструктор для очистки
Display::~Display()
{
    if (hWnd)
    {
        DestroyWindow(hWnd);
        hWnd = nullptr;
    }
}

void Display::SetGamePointer(Game* gamePtr)
{
    GamePointer = gamePtr;
    if (hWnd && gamePtr)
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(gamePtr));
    }
}