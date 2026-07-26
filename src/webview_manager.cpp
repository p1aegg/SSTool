#include "webview_manager.h"
#include "tool_manager.h"
#include <shellapi.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <thread>

#pragma comment(lib, "winhttp.lib")

static constexpr UINT WM_SSTOOL_RUN_SCRIPT = WM_APP + 100;
static constexpr UINT WM_SSTOOL_NAVIGATE_HTML = WM_APP + 101;
static constexpr wchar_t MSG_WND_CLASS[] = L"SSToolWV2MsgWnd";

static constexpr wchar_t REMOTE_INDEX_URL[] =
    L"https://raw.githubusercontent.com/p1aegg/SSTool/refs/heads/main/web/index.html";

struct RemoteHtmlPayload {
    std::wstring html;
    bool success;
};

static std::wstring FetchTextOverHttps(const std::wstring& url, bool* outSuccess) {
    if (outSuccess) *outSuccess = false;

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        return {};
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    HINTERNET hSession = WinHttpOpen(L"SSTool/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     nullptr, nullptr, 0);
    if (!hSession) return {};

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirectPolicy, sizeof(redirectPolicy));

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(),
                                             nullptr, nullptr, nullptr,
                                             WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }

    bool sent = WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) != 0;
    bool received = sent && WinHttpReceiveResponse(hRequest, nullptr) != 0;

    std::string body;
    if (received) {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            nullptr, &statusCode, &statusSize, nullptr);

        if (statusCode == 200) {
            char buffer[65536];
            DWORD bytesRead = 0;
            while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                body.append(buffer, bytesRead);
            }
            if (outSuccess) *outSuccess = true;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (body.empty()) return {};

    int wlen = MultiByteToWideChar(CP_UTF8, 0, body.c_str(), (int)body.size(), nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, body.c_str(), (int)body.size(), &wide[0], wlen);
    return wide;
}

static void PurgeOldScriptTemps() {
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring pattern = std::wstring(tempDir) + L"sstool_script_*.ps1";

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring full = std::wstring(tempDir) + fd.cFileName;
        DeleteFileW(full.c_str());
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

LRESULT CALLBACK WebView2Manager::MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SSTOOL_RUN_SCRIPT) {
        auto* self = reinterpret_cast<WebView2Manager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        auto* script = reinterpret_cast<std::wstring*>(lParam);
        if (self && script) {
            self->ExecuteScript(*script);
        }
        delete script;
        return 0;
    }
    if (msg == WM_SSTOOL_NAVIGATE_HTML) {
        auto* self = reinterpret_cast<WebView2Manager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        auto* payload = reinterpret_cast<RemoteHtmlPayload*>(lParam);
        if (self && payload) {
            self->HandleRemoteHtmlFetched(payload->html, payload->success);
        }
        delete payload;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WebView2Manager::PostScriptToUIThread(const std::wstring& script) {
    if (!m_msgWnd) return;
    auto* payload = new std::wstring(script);
    if (!::PostMessage(m_msgWnd, WM_SSTOOL_RUN_SCRIPT, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

WebView2Manager::WebView2Manager() {}

WebView2Manager::~WebView2Manager() {
    if (m_msgWnd) {
        DestroyWindow(m_msgWnd);
        m_msgWnd = nullptr;
    }
    if (m_controller) {
        m_controller->Close();
        m_controller.Reset();
    }
}

bool WebView2Manager::Initialize(HWND parentWnd, ReadyCallback onReady) {
    m_parentWnd = parentWnd;
    m_onReady = std::move(onReady);

    PurgeOldScriptTemps();

    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = MSG_WND_CLASS;
        RegisterClassEx(&wc);
        s_classRegistered = true;
    }
    m_msgWnd = CreateWindowEx(0, MSG_WND_CLASS, L"", 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_msgWnd) {
        SetWindowLongPtr(m_msgWnd, GWLP_USERDATA, (LONG_PTR)this);
    }

    auto envCompletedHandler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || !env) {
                if (m_onReady) m_onReady(false);
                return result;
            }
            m_env = env;

            RECT bounds;
            GetClientRect(m_parentWnd, &bounds);

            env->CreateCoreWebView2Controller(
                m_parentWnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(result) || !controller) {
                            if (m_onReady) m_onReady(false);
                            return result;
                        }

                        m_controller = controller;
                        m_controller->get_CoreWebView2(&m_webView);

                        RECT bounds;
                        GetClientRect(m_parentWnd, &bounds);
                        m_controller->put_Bounds(bounds);

                        auto webView = m_webView;

                        ComPtr<ICoreWebView2Controller2> ctrl2;
                        if (SUCCEEDED(m_controller.As(&ctrl2))) {
                            COREWEBVIEW2_COLOR bg = { 0xFF, 0x18, 0x18, 0x18 };
                            ctrl2->put_DefaultBackgroundColor(bg);
                        }

                        ComPtr<ICoreWebView2Settings> baseSettings;
                        if (SUCCEEDED(webView->get_Settings(&baseSettings))) {
                            baseSettings->put_AreDevToolsEnabled(FALSE);
                            ComPtr<ICoreWebView2Settings2> settings2;
                            if (SUCCEEDED(baseSettings.As(&settings2))) {
                                settings2->put_AreDefaultContextMenusEnabled(FALSE);
                            }
                        }

                        webView->add_WebMessageReceived(
                            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                    if (!args) return S_OK;
                                    LPWSTR msgRaw = nullptr;
                                    args->TryGetWebMessageAsString(&msgRaw);
                                    if (!msgRaw) return S_OK;
                                    HandleJSMessage(std::wstring(msgRaw), m_webView);
                                    CoTaskMemFree(msgRaw);
                                    return S_OK;
                                }
                            ).Get(),
                            nullptr
                        );

                        webView->add_NavigationCompleted(
                            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                    BOOL success = FALSE;
                                    if (args) args->get_IsSuccess(&success);
                                    if (m_onReady) {
                                        m_onReady(success != FALSE);
                                        m_onReady = nullptr;
                                    }
                                    return S_OK;
                                }
                            ).Get(),
                            nullptr
                        );

                        std::thread([this]() {
                            bool success = false;
                            std::wstring html = FetchTextOverHttps(REMOTE_INDEX_URL, &success);
                            auto* payload = new RemoteHtmlPayload{ std::move(html), success };
                            if (!m_msgWnd ||
                                !::PostMessage(m_msgWnd, WM_SSTOOL_NAVIGATE_HTML, 0,
                                               reinterpret_cast<LPARAM>(payload))) {
                                delete payload;
                            }
                        }).detach();

                        m_initialized = true;
                        return S_OK;
                    }
                ).Get()
            );

            return S_OK;
        }
    );

    wchar_t appData[MAX_PATH];
    DWORD appDataLen = GetEnvironmentVariableW(L"LOCALAPPDATA", appData, MAX_PATH);
    std::wstring userDataFolder = (appDataLen > 0 && appDataLen < MAX_PATH)
        ? (std::wstring(appData) + L"\\SSTool\\WebView2")
        : (std::wstring(L"C:\\temp\\SSTool\\WebView2"));

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        envCompletedHandler.Get()
    );

    return SUCCEEDED(hr);
}

void WebView2Manager::SetBounds(RECT bounds) {
    if (m_controller) {
        m_controller->put_Bounds(bounds);
    }
}

void WebView2Manager::Show() {
    if (m_controller) {
        m_controller->put_IsVisible(TRUE);
    }
}

void WebView2Manager::Hide() {
    if (m_controller) {
        m_controller->put_IsVisible(FALSE);
    }
}

void WebView2Manager::Navigate(const std::wstring& url) {
    if (m_webView) {
        m_webView->Navigate(url.c_str());
    }
}

void WebView2Manager::PostMessage(const std::wstring& jsonMessage) {
    std::wstring escaped;
    for (wchar_t c : jsonMessage) {
        if (c == L'\\') escaped += L"\\\\";
        else if (c == L'\'') escaped += L"\\'";
        else escaped += c;
    }
    PostScriptToUIThread(L"window.handleCppMessage('" + escaped + L"')");
}

HRESULT WebView2Manager::ExecuteScript(const std::wstring& script) {
    if (m_webView) {
        return m_webView->ExecuteScript(script.c_str(), nullptr);
    }
    return E_FAIL;
}

void WebView2Manager::HandleRemoteHtmlFetched(const std::wstring& html, bool success) {
    if (!success || html.empty()) {
        if (m_onReady) {
            m_onReady(false);
            m_onReady = nullptr;
        }
        return;
    }
    if (m_webView) {
        m_webView->NavigateToString(html.c_str());
    }
}

static std::vector<std::wstring> ParseUrlArray(const std::wstring& json) {
    std::vector<std::wstring> urls;
    if (json.empty()) return urls;
    if (json[0] != L'[') { urls.push_back(json); return urls; }
    size_t pos = 0;
    while ((pos = json.find(L'"', pos)) != std::wstring::npos) {
        pos++;
        size_t end = json.find(L'"', pos);
        if (end == std::wstring::npos) break;
        urls.push_back(json.substr(pos, end - pos));
        pos = end + 1;
    }
    return urls;
}

void WebView2Manager::HandleJSMessage(const std::wstring& msg, ComPtr<ICoreWebView2> webView) {
    auto extractMethod = [&]() -> std::wstring {
        auto m = msg.find(L"\"method\":\"");
        if (m == std::wstring::npos) return {};
        m += 10;
        std::wstring val;
        while (m < msg.length() && msg[m] != L'"') {
            if (msg[m] == L'\\' && m + 1 < msg.length()) { val += msg[m + 1]; m += 2; }
            else { val += msg[m]; m++; }
        }
        return val;
    };

    auto extractArg = [&](int index) -> std::wstring {
        std::wstring key = L"\"args\":[";
        auto a = msg.find(key);
        if (a == std::wstring::npos) return {};
        a += key.length();
        int found = 0;
        while (a < msg.length() && found <= index) {
            if (msg[a] == L'"') {
                a++;
                if (found == index) {
                    std::wstring val;
                    while (a < msg.length() && msg[a] != L'"') {
                        if (msg[a] == L'\\' && a + 1 < msg.length()) { val += msg[a + 1]; a += 2; }
                        else { val += msg[a]; a++; }
                    }
                    return val;
                }
                found++;
                while (a < msg.length() && msg[a] != L'"') a++;
                a++;
            } else {
                a++;
            }
        }
        return {};
    };

    std::wstring method = extractMethod();
    if (method.empty()) return;

    if (method == L"downloadTool") {
        std::wstring name = extractArg(0);
        std::wstring urlsJson = extractArg(1);
        std::wstring category = extractArg(2);
        if (name.empty() || urlsJson.empty()) return;
        if (category.empty()) category = L"unknown";

        std::vector<std::wstring> urls = ParseUrlArray(urlsJson);
        if (urls.empty()) return;

        if (m_toolManager) {
            m_toolManager->DownloadTool(name, urls, category,
                [this, name](const std::wstring&, int value, const std::wstring& status, int totalSize) {
                    std::wstring json = L"{\"type\":\"progress\",\"name\":\"" + EscapeJson(name) + L"\",\"value\":" + std::to_wstring(value) + L",\"totalSize\":" + std::to_wstring(totalSize) + L",\"status\":\"" + EscapeJson(status) + L"\"}";
                    std::wstring escaped; for (wchar_t c : json) { if (c == L'\\') escaped += L"\\\\"; else if (c == L'\'') escaped += L"\\'"; else escaped += c; }
                    PostScriptToUIThread(L"window.handleCppMessage('" + escaped + L"')");
                },
                [this, name](const std::wstring&, bool success, const std::wstring& exePath) {
                    std::wstring json = L"{\"type\":\"downloadComplete\",\"name\":\"" + EscapeJson(name) + L"\",\"success\":" + (success ? L"true" : L"false") + L",\"exePath\":\"" + EscapeJson(exePath) + L"\"}";
                    std::wstring escaped; for (wchar_t c : json) { if (c == L'\\') escaped += L"\\\\"; else if (c == L'\'') escaped += L"\\'"; else escaped += c; }
                    PostScriptToUIThread(L"window.handleCppMessage('" + escaped + L"')");
                }
            );
        }
    } else if (method == L"launchTool") {
        std::wstring exePath = extractArg(0);
        if (!exePath.empty() && m_toolManager) {
            m_toolManager->LaunchTool(exePath);
        }
    } else if (method == L"closeApp") {
        PostQuitMessage(0);
    } else if (method == L"logMessage") {
        std::wstring text = extractArg(0);
        OutputDebugStringW(text.c_str());
        OutputDebugStringW(L"\n");
    } else if (method == L"openScript") {
        std::wstring script = extractArg(0);
        if (!script.empty()) {
            wchar_t tempDir[MAX_PATH];
            GetTempPathW(MAX_PATH, tempDir);
            std::wstring ps1Path = std::wstring(tempDir) + L"sstool_script_" +
                                    std::to_wstring(GetTickCount64()) + L".ps1";

            HANDLE hFile = CreateFileW(ps1Path.c_str(), GENERIC_WRITE, 0, nullptr,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile == INVALID_HANDLE_VALUE) return;

            const WORD bom = 0xFEFF;
            DWORD written = 0;
            WriteFile(hFile, &bom, sizeof(bom), &written, nullptr);
            WriteFile(hFile, script.c_str(),
                      (DWORD)(script.size() * sizeof(wchar_t)), &written, nullptr);
            CloseHandle(hFile);

            std::wstring cmdLine =
                L"cmd.exe /k title Command Prompt & "
                L"powershell -ExecutionPolicy Bypass -File \"" + ps1Path + L"\" & "
                L"title Command Prompt";

            std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
            cmdLineBuf.push_back(L'\0');

            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi = {};
            BOOL created = CreateProcessW(
                nullptr, cmdLineBuf.data(), nullptr, nullptr, FALSE,
                CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);

            if (created) {
                CloseHandle(pi.hThread);
                std::thread([procHandle = pi.hProcess, ps1Path]() {
                    WaitForSingleObject(procHandle, INFINITE);
                    CloseHandle(procHandle);
                    DeleteFileW(ps1Path.c_str());
                }).detach();
            } else {
                ShellExecuteW(nullptr, L"open", L"cmd.exe",
                    (L"/k title Command Prompt & powershell -ExecutionPolicy Bypass -File \"" +
                     ps1Path + L"\" & title Command Prompt").c_str(),
                    nullptr, SW_SHOWNORMAL);
            }
        }
    } else if (method == L"minimize") {
        if (m_parentWnd) ShowWindow(m_parentWnd, SW_MINIMIZE);
    } else if (method == L"maximize") {
        if (m_parentWnd) {
            WINDOWPLACEMENT wp = { sizeof(wp) };
            GetWindowPlacement(m_parentWnd, &wp);
            ShowWindow(m_parentWnd, wp.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
        }
    } else if (method == L"startWindowDrag") {
        if (m_parentWnd) {
            ReleaseCapture();
            POINT pt;
            GetCursorPos(&pt);
            SendMessage(m_parentWnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
        }
    } else if (method == L"cancelDownload") {
        if (m_toolManager) m_toolManager->CancelCurrentDownload();
    }
}
    // ... existing method/arg extraction ...