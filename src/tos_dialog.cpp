#include "tos_dialog.h"
#include <gdiplus.h>
#include <windowsx.h>

#pragma comment(lib, "gdiplus.lib")

bool TosDialog::s_accepted = false;

static constexpr int TOS_WIDTH = 520;
static constexpr int TOS_HEIGHT = 480;
static constexpr int IDC_ACCEPT = 1001;
static constexpr int IDC_CANCEL = 1002;

bool TosDialog::Show(HWND parentWnd, HINSTANCE hInstance) {
    s_accepted = false;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"SSToolTOS";
    RegisterClassEx(&wc);

    RECT parentRect;
    GetWindowRect(parentWnd, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - TOS_WIDTH) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - TOS_HEIGHT) / 2;

    HWND hwnd = CreateWindowEx(
        WS_EX_WINDOWEDGE,
        L"SSToolTOS", L"SSTool",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_POPUPWINDOW,
        x, y, TOS_WIDTH, TOS_HEIGHT,
        parentWnd, nullptr, hInstance, nullptr
    );

    if (!hwnd) return false;

    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HWND btnCancel = CreateWindowEx(0, L"BUTTON", L"  Cancel  ",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, TOS_HEIGHT - 65, 110, 34,
        hwnd, (HMENU)IDC_CANCEL, hInstance, nullptr);
    SendMessage(btnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND btnAccept = CreateWindowEx(0, L"BUTTON", L"  Accept & Continue  ",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        260, TOS_HEIGHT - 65, 150, 34,
        hwnd, (HMENU)IDC_ACCEPT, hInstance, nullptr);
    SendMessage(btnAccept, WM_SETFONT, (WPARAM)hFont, TRUE);

    EnableWindow(parentWnd, FALSE);

    CenterWindow(hwnd);

    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parentWnd, TRUE);
    DeleteObject(hFont);
    return s_accepted;
}

void TosDialog::CenterWindow(HWND hwnd) {
    HWND parent = GetParent(hwnd);
    if (!parent) parent = GetDesktopWindow();

    RECT parentRect, windowRect;
    GetWindowRect(parent, &parentRect);
    GetWindowRect(hwnd, &windowRect);

    int w = windowRect.right - windowRect.left;
    int h = windowRect.bottom - windowRect.top;
    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT CALLBACK TosDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: return OnPaint(hwnd);
        case WM_CTLCOLORBTN:
            SetBkColor((HDC)wParam, RGB(30, 30, 30));
            SetTextColor((HDC)wParam, RGB(200, 200, 200));
            return (LRESULT)CreateSolidBrush(RGB(30, 30, 30));
        case WM_COMMAND: OnCommand(hwnd, wParam); return 0;
        case WM_CLOSE:
            s_accepted = false;
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT TosDialog::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (!hdc) return 0;

    RECT client;
    GetClientRect(hwnd, &client);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 22, 22, 30));
        g.FillRectangle(&bgBrush, 0, 0, client.right, client.bottom);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font titleFont(&ff, 20, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 180, 140, 255));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF titleRect(0, 20, (float)client.right, 40);
        g.DrawString(L"SSTool", -1, &titleFont, titleRect, &sf, &titleBrush);

        Gdiplus::Font bodyFont(&ff, 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush bodyBrush(Gdiplus::Color(230, 200, 200, 210));
        sf.SetAlignment(Gdiplus::StringAlignmentNear);

        const wchar_t* body =
            L"By using SSTool you agree to the following:\n\n"
            L"\u2022  Tools are downloaded from their official GitHub repositories.\n"
            L"\u2022  Downloaded tools are saved in a local ./tools/ folder next to the executable.\n"
            L"\u2022  No data is collected, telemetry is not transmitted, and nothing leaves this machine.\n"
            L"\u2022  Each tool is maintained by its own respective author and is subject to its own license.\n"
            L"\u2022  The developer of SSTool takes no responsibility for the behavior of third-party tools.\n"
            L"\u2022  You accept full responsibility for your use of any downloaded tool.\n\n"
            L"SSTool itself is developed by Orbdiff (github.com/orbdiff).";

        Gdiplus::RectF bodyRect(30, 75, (float)client.right - 60, 260);
        g.DrawString(body, -1, &bodyFont, bodyRect, &sf, &bodyBrush);

        Gdiplus::Font warnFont(&ff, 12, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush warnBrush(Gdiplus::Color(255, 255, 80, 80));
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF warnRect(20, 340, (float)client.right - 40, 40);
        g.DrawString(L"To continue, you must agree with everything stated above.",
                     -1, &warnFont, warnRect, &sf, &warnBrush);
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
    return 0;
}

void TosDialog::OnCommand(HWND hwnd, WPARAM wParam) {
    int id = LOWORD(wParam);
    if (id == IDC_ACCEPT) {
        s_accepted = true;
        DestroyWindow(hwnd);
    } else if (id == IDC_CANCEL) {
        s_accepted = false;
        DestroyWindow(hwnd);
    }
}
