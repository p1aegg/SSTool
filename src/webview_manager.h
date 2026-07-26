#pragma once
#include <windows.h>
#include <objidl.h>
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <string>
#include <functional>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;

class ToolManager;

class WebView2Manager {
public:
    using ReadyCallback = std::function<void(bool success)>;

    WebView2Manager();
    ~WebView2Manager();

    bool Initialize(HWND parentWnd, ReadyCallback onReady);
    void SetBounds(RECT bounds);
    void Show();
    void Hide();
    void Navigate(const std::wstring& url);
    void PostMessage(const std::wstring& jsonMessage);
    HRESULT ExecuteScript(const std::wstring& script);

    void PostScriptToUIThread(const std::wstring& script);

    void SetToolManager(ToolManager* mgr) { m_toolManager = mgr; }
    ComPtr<ICoreWebView2> GetWebView() const { return m_webView; }

private:
    void HandleJSMessage(const std::wstring& msg, ComPtr<ICoreWebView2> webView);
    void HandleRemoteHtmlFetched(const std::wstring& html, bool success);
    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    ComPtr<ICoreWebView2Environment> m_env;
    ComPtr<ICoreWebView2Controller> m_controller;
    ComPtr<ICoreWebView2> m_webView;
    HWND m_parentWnd = nullptr;
    HWND m_msgWnd = nullptr;
    ToolManager* m_toolManager = nullptr;
    ReadyCallback m_onReady;
    bool m_initialized = false;
};

inline std::wstring EscapeJson(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}