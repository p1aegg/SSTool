#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <string>
#include <memory>

#include "splash.h"
#include "webview_manager.h"
#include "tool_manager.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

static constexpr wchar_t MAIN_WND_CLASS[] = L"SSToolMain";
static constexpr int MIN_WIDTH = 640;
static constexpr int MIN_HEIGHT = 500;

static std::unique_ptr<SplashScreen> g_splash;
static std::unique_ptr<WebView2Manager> g_webview;
static std::unique_ptr<ToolManager> g_toolMgr;
static HWND g_mainHwnd = nullptr;
static bool g_webViewReady = false;

static ULONG_PTR g_gdiplusToken = 0;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_mainHwnd = hwnd;
            g_toolMgr = std::make_unique<ToolManager>();
            g_webview = std::make_unique<WebView2Manager>();
            g_webview->SetToolManager(g_toolMgr.get());

            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
            DWORD cornerPreference = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                   &cornerPreference, sizeof(cornerPreference));

            g_webview->Initialize(hwnd, [hwnd](bool success) {
                g_webViewReady = true;

                if (g_splash) {
                    g_splash->Close();
                    g_splash.reset();
                }

                if (!success) {
                    MessageBoxW(hwnd,
                        L"Failed to load SSTool.\n\n"
                        L"This can happen if:\n"
                        L"  - The WebView2 Runtime isn't installed:\n"
                        L"    https://developer.microsoft.com/en-us/microsoft-edge/webview2/\n"
                        L"  - There is no internet connection (the UI is loaded from GitHub)",
                        L"SSTool - Error", MB_ICONERROR | MB_OK);
                    DestroyWindow(hwnd);
                    return;
                }

                g_webview->Show();
                ShowWindow(hwnd, SW_SHOW);
                UpdateWindow(hwnd);
            });

            break;
        }

        case WM_SIZE: {
            if (g_webview) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                g_webview->SetBounds(bounds);
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = MIN_WIDTH;
            mmi->ptMinTrackSize.y = MIN_HEIGHT;
            break;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            break;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool CreateMainWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = MAIN_WND_CLASS;

    if (!RegisterClassEx(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int w = 1100;
    int h = 720;
    int x = (screenW - w) / 2;
    int y = (screenH - h) / 2;

    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        MAIN_WND_CLASS,
        L"SSTool - p1ae",
        WS_POPUP | WS_CLIPCHILDREN,
        x, y, w, h,
        nullptr, nullptr, hInstance, nullptr
    );

    return hwnd != nullptr;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);

    g_splash = std::make_unique<SplashScreen>();
    g_splash->Show(hInstance);

    if (!CreateMainWindow(hInstance)) {
        g_splash->Close();
        g_splash.reset();
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_webview.reset();
    g_toolMgr.reset();
    g_splash.reset();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return 0;
}