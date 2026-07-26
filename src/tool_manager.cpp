#include "tool_manager.h"
#include <shlwapi.h>
#include <shellapi.h>
#include <winhttp.h>
#include <objbase.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

ToolManager::ToolManager() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    m_toolsPath = exePath;
    m_toolsPath += L"\\tools";
}

ToolManager::~ToolManager() {
    m_cancelCurrent = true;
}

bool ToolManager::EnsureToolsDirectory(const std::wstring& subDir) {
    if (!CreateDirectoryW(m_toolsPath.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    if (!subDir.empty()) {
        std::wstring subPath = m_toolsPath + L"\\" + subDir;
        if (!CreateDirectoryW(subPath.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    return true;
}

std::wstring ToolManager::GetToolsPath() const {
    return m_toolsPath;
}

bool ToolManager::IsToolDownloaded(const std::wstring& exeName, const std::wstring& category) const {
    std::wstring fullPath = m_toolsPath + L"\\" + category + L"\\" + exeName;
    return PathFileExistsW(fullPath.c_str()) == TRUE;
}

bool ToolManager::DownloadFile(const std::wstring& url, const std::wstring& destPath,
                                ProgressCallback onProgress) {
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        return false;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    HINTERNET hSession = WinHttpOpen(L"SSTool/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     nullptr, nullptr, 0);
    if (!hSession) return false;

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirectPolicy, sizeof(redirectPolicy));

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(),
                                             nullptr, nullptr, nullptr,
                                             WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    bool sent = WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) != 0;
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    bool received = WinHttpReceiveResponse(hRequest, nullptr) != 0;
    if (!received) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        nullptr, &statusCode, &statusSize, nullptr);

    if (statusCode != 200) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    wchar_t contentLength[32] = {};
    DWORD clSize = sizeof(contentLength);
    DWORD totalSize = 0;
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                            nullptr, contentLength, &clSize, nullptr)) {
        totalSize = _wtoi(contentLength);
    }

    std::wstring tmpPath = destPath + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }
    bool result = false;
    DWORD downloaded = 0;
    DWORD bytesRead = 0;
    BYTE buffer[65536];
    bool cancelled = false;
    int lastReportedPct = -1;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (m_cancelCurrent) { cancelled = true; break; }

        DWORD written;
        WriteFile(hFile, buffer, bytesRead, &written, nullptr);
        downloaded += bytesRead;

        if (onProgress) {
            if (totalSize > 0) {
                int percent = (int)((double)downloaded / totalSize * 100);
                if (percent != lastReportedPct) {
                    lastReportedPct = percent;
                    onProgress(L"", percent, L"Downloading...", (int)totalSize);
                }
            } else {
                onProgress(L"", downloaded, L"Downloading...", 0);
            }
        }
    }

if (cancelled) {
        CloseHandle(hFile);
        DeleteFileW(tmpPath.c_str());
    } else {
        CloseHandle(hFile);
        DeleteFileW(destPath.c_str());
        if (MoveFileW(tmpPath.c_str(), destPath.c_str())) {
            result = true;
        } else {
            DeleteFileW(tmpPath.c_str());
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static std::wstring ExtractFilenameFromUrl(const std::wstring& url) {
    size_t slash = url.find_last_of(L'/');
    if (slash != std::wstring::npos && slash + 1 < url.length())
        return url.substr(slash + 1);
    return url;
}

void ToolManager::DownloadTool(const std::wstring& name, const std::vector<std::wstring>& urls,
                                const std::wstring& category,
                                ProgressCallback onProgress, CompleteCallback onComplete) {
    m_cancelCurrent = false;
    EnsureToolsDirectory(category);

    std::wstring ext = L".exe";
    if (!urls.empty()) {
        size_t dot = urls[0].find_last_of(L'.');
        size_t slash = urls[0].find_last_of(L'/');
        if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
            ext = urls[0].substr(dot);
    }
    std::wstring destPath = m_toolsPath + L"\\" + category + L"\\" + name + ext;

    std::thread([this, name, urls, category, destPath, onProgress, onComplete]() {
        HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        bool allSuccess = true;
        int totalFiles = (int)urls.size();

        for (int i = 0; i < totalFiles; i++) {
            if (m_cancelCurrent) { allSuccess = false; break; }

            std::wstring destFile = (i == 0)
                ? destPath
                : (m_toolsPath + L"\\" + category + L"\\" + ExtractFilenameFromUrl(urls[i]));

            if (onProgress) {
                std::wstring status = L"[" + std::to_wstring(i + 1) + L"/" + std::to_wstring(totalFiles) + L"]";
                onProgress(name, 0, status, 0);
            }

            auto fileProgress = [name, i, totalFiles, onProgress](const std::wstring&, int value,
                                                                    const std::wstring&, int totalSize) {
                if (onProgress) {
                    std::wstring s = L"[" + std::to_wstring(i + 1) + L"/" + std::to_wstring(totalFiles) + L"]";
                    onProgress(name, value, s, totalSize);
                }
            };

            bool success = DownloadFile(urls[i], destFile, fileProgress);
            if (!success) { allSuccess = false; break; }
        }

        if (m_cancelCurrent) {
            if (onComplete) onComplete(name, false, L"");
            if (SUCCEEDED(coInit)) CoUninitialize();
            return;
        }

        if (onComplete) onComplete(name, allSuccess, allSuccess ? destPath : L"");

        if (SUCCEEDED(coInit)) CoUninitialize();
    }).detach();
}

bool ToolManager::AddDefenderExclusion() {
    std::wstring args = L"-NoProfile -Command \"";
    args += L"try { Add-MpPreference -ExclusionPath '" + m_toolsPath + L"' -ErrorAction Stop; exit 0 } ";
    args += L"catch { exit 1 }\"";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"open";
    sei.lpFile = L"powershell.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteExW(&sei) == TRUE;
}

bool ToolManager::LaunchTool(const std::wstring& exePath) {
    std::wstring workingDir;
    size_t lastSlash = exePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        workingDir = exePath.substr(0, lastSlash);
    }

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = exePath.c_str();
    sei.lpDirectory = workingDir.empty() ? nullptr : workingDir.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) == TRUE;
}