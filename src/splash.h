#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <vector>

class SplashScreen {
public:
    SplashScreen();
    ~SplashScreen();

    bool Show(HINSTANCE hInstance);
    void Close();
    HWND GetWindow() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT OnPaint();
    LRESULT OnTimer();
    void OnDestroy();

    void InitStars();
    void DrawScene(Gdiplus::Graphics& g);
    void DrawStars(Gdiplus::Graphics& g);
    void DrawNebula(Gdiplus::Graphics& g);
    void DrawShootingStars(Gdiplus::Graphics& g, int frame);
    void DrawSpinner(Gdiplus::Graphics& g, int cx, int cy, int frame);
    void DrawLoadingText(Gdiplus::Graphics& g);

    HWND m_hwnd = nullptr;
    ULONG_PTR m_gdiplusToken = 0;
    bool m_closed = false;
    int m_frame = 0;

    struct Star {
        float x, y, size;
        float phase;
    };
    std::vector<Star> m_stars;

    struct ShootingStar {
        float startX, startY;
        float dx, dy;
        float delay;
        float duration;
    };
    std::vector<ShootingStar> m_shootingStars;

    struct Nebula {
        float x, y, radius;
        Gdiplus::Color color;
    };
    std::vector<Nebula> m_nebulas;
};
