#include "splash.h"
#include <cmath>
#include <cstdlib>

static constexpr int SPLASH_WIDTH = 640;
static constexpr int SPLASH_HEIGHT = 420;
static constexpr int TIMER_ID = 1;
static constexpr int TIMER_INTERVAL_MS = 33;

SplashScreen::SplashScreen() : m_hwnd(nullptr), m_gdiplusToken(0), m_closed(false), m_frame(0) {}

SplashScreen::~SplashScreen() {
    Close();
}

void SplashScreen::InitStars() {
    m_stars.clear();
    for (int i = 0; i < 150; i++) {
        Star s;
        s.x = (float)(rand() % 10000) / 100.0f;
        s.y = (float)(rand() % 10000) / 100.0f;
        s.size = 0.5f + (float)(rand() % 20) / 10.0f;
        s.phase = (float)(rand() % 628) / 100.0f;
        m_stars.push_back(s);
    }

    m_shootingStars.clear();
    for (int i = 0; i < 8; i++) {
        ShootingStar ss;
        ss.startX = (float)(rand() % 70);
        ss.startY = (float)(rand() % 60);
        ss.dx = 150.0f + (float)(rand() % 400);
        ss.dy = 80.0f + (float)(rand() % 250);
        ss.delay = i * 1.8f + (float)(rand() % 30) / 10.0f;
        ss.duration = 2.0f + (float)(rand() % 30) / 10.0f;
        m_shootingStars.push_back(ss);
    }

    m_nebulas.clear();
    {
        Nebula n;
        n.x = 15.0f; n.y = 25.0f; n.radius = 180.0f;
        n.color = Gdiplus::Color(18, 100, 50, 180);
        m_nebulas.push_back(n);
    }
    {
        Nebula n;
        n.x = 65.0f; n.y = 60.0f; n.radius = 150.0f;
        n.color = Gdiplus::Color(14, 180, 60, 100);
        m_nebulas.push_back(n);
    }
    {
        Nebula n;
        n.x = 45.0f; n.y = 40.0f; n.radius = 120.0f;
        n.color = Gdiplus::Color(12, 80, 40, 200);
        m_nebulas.push_back(n);
    }
    {
        Nebula n;
        n.x = 30.0f; n.y = 70.0f; n.radius = 100.0f;
        n.color = Gdiplus::Color(10, 60, 30, 220);
        m_nebulas.push_back(n);
    }
}

bool SplashScreen::Show(HINSTANCE hInstance) {
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gsi, nullptr);

    InitStars();

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_WAIT);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"SSToolSplash";
    wc.hIcon = nullptr;
    wc.hIconSm = nullptr;

    RegisterClassEx(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - SPLASH_WIDTH) / 2;
    int y = (screenH - SPLASH_HEIGHT) / 2;

    m_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"SSToolSplash", L"SSTool",
        WS_POPUP,
        x, y, SPLASH_WIDTH, SPLASH_HEIGHT,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
    SetTimer(m_hwnd, TIMER_ID, TIMER_INTERVAL_MS, nullptr);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void SplashScreen::Close() {
    if (m_hwnd) {
        KillTimer(m_hwnd, TIMER_ID);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_gdiplusToken) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}

LRESULT CALLBACK SplashScreen::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SplashScreen* self = nullptr;
    if (msg == WM_CREATE) {
        auto cs = (CREATESTRUCT*)lParam;
        self = (SplashScreen*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (SplashScreen*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_PAINT: return self->OnPaint();
        case WM_TIMER: return self->OnTimer();
        case WM_DESTROY: self->OnDestroy(); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT SplashScreen::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    if (!hdc) return 0;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, SPLASH_WIDTH, SPLASH_HEIGHT);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        DrawScene(g);
    }

    BitBlt(hdc, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(m_hwnd, &ps);
    return 0;
}

LRESULT SplashScreen::OnTimer() {
    m_frame++;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

void SplashScreen::OnDestroy() {
    m_hwnd = nullptr;
}

void SplashScreen::DrawScene(Gdiplus::Graphics& g) {
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 8, 8, 20));
    g.FillRectangle(&bgBrush, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT);

    DrawNebula(g);
    DrawStars(g);
    DrawShootingStars(g, m_frame);

    int cx = SPLASH_WIDTH / 2;
    int cy = SPLASH_HEIGHT / 2 - 16;
    DrawSpinner(g, cx, cy, m_frame);
    DrawLoadingText(g);
}

void SplashScreen::DrawNebula(Gdiplus::Graphics& g) {
    for (const auto& n : m_nebulas) {
        float px = n.x / 100.0f * SPLASH_WIDTH;
        float py = n.y / 100.0f * SPLASH_HEIGHT;
        Gdiplus::SolidBrush brush(n.color);
        g.FillEllipse(&brush, px - n.radius, py - n.radius, n.radius * 2, n.radius * 2);
    }
}

void SplashScreen::DrawStars(Gdiplus::Graphics& g) {
    for (const auto& s : m_stars) {
        float px = s.x / 100.0f * SPLASH_WIDTH;
        float py = s.y / 100.0f * SPLASH_HEIGHT;
        float twinkle = 0.3f + 0.7f * (0.5f + 0.5f * (float)sin((double)m_frame * 0.05f + (double)s.phase));
        int alpha = (int)(255 * twinkle);
        Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, 220, 220, 255));
        g.FillEllipse(&brush, px - s.size / 2, py - s.size / 2, s.size, s.size);
    }
}

void SplashScreen::DrawShootingStars(Gdiplus::Graphics& g, int frame) {
    float time = frame * TIMER_INTERVAL_MS / 1000.0f;
    for (const auto& ss : m_shootingStars) {
        float elapsed = time - ss.delay;
        if (elapsed < 0 || elapsed > ss.duration) continue;
        float t = elapsed / ss.duration;

        float startPx = ss.startX / 100.0f * SPLASH_WIDTH;
        float startPy = ss.startY / 100.0f * SPLASH_HEIGHT;
        float endPx = startPx + ss.dx;
        float endPy = startPy + ss.dy;

        float cx = startPx + (endPx - startPx) * t;
        float cy = startPy + (endPy - startPy) * t;

        float alpha = t < 0.7f ? t / 0.7f : (1.0f - t) / 0.3f;
        alpha = min(1.0f, max(0.0f, alpha));
        int tailLen = 40;

        float tx = cx - (endPx - startPx) / ss.duration * 0.016f * tailLen;
        float ty = cy - (endPy - startPy) / ss.duration * 0.016f * tailLen;

        Gdiplus::Pen pen(Gdiplus::Color((int)(255 * alpha), 255, 255, 255), 1.5f);
        g.DrawLine(&pen, (int)cx, (int)cy, (int)tx, (int)ty);

        Gdiplus::SolidBrush headBrush(Gdiplus::Color((int)(255 * alpha), 255, 255, 255));
        g.FillEllipse(&headBrush, (INT)(cx - 2), (INT)(cy - 2), 4, 4);
    }
}

void SplashScreen::DrawSpinner(Gdiplus::Graphics& g, int cx, int cy, int frame) {
    const int numDots = 12;
    const int radius = 14;
    const int dotSize = 4;

    for (int i = 0; i < numDots; i++) {
        double angle = (i * 2.0 * 3.14159 / numDots) - (frame * 0.12);
        int dx = cx + (int)(radius * cos(angle));
        int dy = cy + (int)(radius * sin(angle));

        int idx = (i + frame) % numDots;
        float opacity = 1.0f - (float)idx / (float)numDots;
        int alpha = (int)(200 * opacity);

        Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, 180, 160, 255));
        g.FillEllipse(&brush, dx - dotSize / 2, dy - dotSize / 2, dotSize, dotSize);
    }
}

void SplashScreen::DrawLoadingText(Gdiplus::Graphics& g) {
    Gdiplus::FontFamily ff(L"Segoe UI");

    Gdiplus::Font font(&ff, 13, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(200, 200, 200, 230));

    Gdiplus::StringFormat sfCenter;
    sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
    sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    int cx = SPLASH_WIDTH / 2;
    int cy = SPLASH_HEIGHT / 2 - 16;

    Gdiplus::RectF textRect((float)(cx - 100), (float)(cy + 26), 200.0f, 30.0f);
    g.DrawString(L"Loading UI\u2026", -1, &font, textRect, &sfCenter, &textBrush);

    Gdiplus::Font smallFont(&ff, 11, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush subtitleBrush(Gdiplus::Color(220, 180, 140, 255));

    Gdiplus::StringFormat sfLeft;
    sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
    sfLeft.SetLineAlignment(Gdiplus::StringAlignmentNear);

    Gdiplus::RectF subRect(18.0f, 16.0f, 320.0f, 24.0f);
    g.DrawString(L"SSTool - p1ae (Fork)", -1, &smallFont, subRect, &sfLeft, &subtitleBrush);
}