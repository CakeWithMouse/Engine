#pragma once
#include <windows.h>
#include <vector>

class Game;

class Display
{
public:
    /** ScreenW/ScreenH is the client area (the drawable size), not the outer window size. */
    Display(int ScreenW = 1000, int ScreenH = 500);
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    FORCEINLINE HWND GetHwnd() const { return hWnd; }
    FORCEINLINE WNDCLASSEX GetWinClass() const { return wc; }
    FORCEINLINE RECT GetWinRect() const { return windowRect; }
    FORCEINLINE int GetHeight() const { return ScreenH; }
    FORCEINLINE int GetWidth() const { return ScreenW; }

    void SetGamePointer(Game* gamePtr);
    /** Called by Game once the swap chain has actually been resized. */
    void SetClientSize(int width, int height);

private:
    void InitWinClass();
    void InitHwndClass();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    LPCWSTR applicationName = L"My3DApp";
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    HWND hWnd = nullptr;
    WNDCLASSEX wc = {};
    RECT windowRect = {};

    int ScreenW = 1000;
    int ScreenH = 500;

    Game* GamePointer = nullptr;
};
