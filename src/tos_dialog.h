#pragma once
#include <windows.h>

class TosDialog {
public:
    static bool Show(HWND parentWnd, HINSTANCE hInstance);

private:
    TosDialog() = delete;
    ~TosDialog() = delete;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT OnPaint(HWND hwnd);
    static void OnCommand(HWND hwnd, WPARAM wParam);
    static void CenterWindow(HWND hwnd);

    static bool s_accepted;
};
