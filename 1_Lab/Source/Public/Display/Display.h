#pragma once
#include <windows.h>  // Включаем полный Windows.h вместо libloaderapi.h

class Game;

class Display
{
public:
    Display(int ScreenW = 1000, int ScreenH = 500);
    ~Display();  // Добавляем деструктор

    // Геттеры
    FORCEINLINE HWND GetHwnd() const { return hWnd; }
    FORCEINLINE WNDCLASSEX GetWinClass() const { return wc; }
    FORCEINLINE RECT GetWinRect() const { return windowRect; }
    FORCEINLINE int GetHeight() const { return ScreenH; }
    FORCEINLINE int GetWidth() const { return ScreenW; }

    void SetGamePointer(Game* gamePtr);
private:
    void InitWinClass();
    void InitHwndClass();
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    LPCWSTR applicationName = L"My3DApp";
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    
    HWND hWnd = nullptr;  // Инициализируем nullptr
    WNDCLASSEX wc = {};   // Инициализируем нулями
    RECT windowRect = {};  // Инициализируем нулями

    int ScreenW = 600;
    int ScreenH = 800;

    Game* GamePointer = nullptr;
};