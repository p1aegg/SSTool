#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include <thread>

struct ToolInfo {
    std::wstring name;
    std::wstring displayName;
    std::wstring description;
    std::wstring downloadUrl;
    std::wstring exeName;
    std::wstring category;
    std::wstring detail;
};

class ToolManager {
public:
    using ProgressCallback = std::function<void(const std::wstring& toolName, int value, const std::wstring& status, int totalSize)>;
    using CompleteCallback = std::function<void(const std::wstring& toolName, bool success, const std::wstring& exePath)>;

    ToolManager();
    ~ToolManager();

    std::wstring GetToolsPath() const;

    void DownloadTool(const std::wstring& name, const std::vector<std::wstring>& urls,
                      const std::wstring& category,
                      ProgressCallback onProgress, CompleteCallback onComplete);

    void CancelCurrentDownload() { m_cancelCurrent = true; }
    bool LaunchTool(const std::wstring& exePath);
    bool IsToolDownloaded(const std::wstring& exeName, const std::wstring& category) const;
    bool AddDefenderExclusion();

private:
    bool EnsureToolsDirectory(const std::wstring& subDir = L"");
    bool DownloadFile(const std::wstring& url, const std::wstring& destPath,
                      ProgressCallback onProgress);

    std::wstring m_toolsPath;
    std::atomic<bool> m_cancelCurrent{false};
};