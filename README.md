# SSTool — Forensic Tooling Suite

A modern Windows desktop launcher for digital forensics tools, built with C++20 and WebView2.

## Requirements

- Windows 10 / Windows 11 (x64)
- [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (pre-installed on Windows 11)
- Visual Studio 2022 (with C++ tools)
- CMake 3.22+

## Building

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The output `SSTool.exe` will be in `build/Release/` along with the `web/` folder.

## Project Structure

```
sstools/
├── CMakeLists.txt
├── src/
│   ├── main.cpp          Entry point and window management
│   ├── splash.h/.cpp     Animated space-themed loading screen (GDI+)
│   ├── webview_manager.h/.cpp  WebView2 integration and JS/C++ bridge
│   ├── tool_manager.h/.cpp     Tool download, launch, and management
│   └── resource.h        Resource identifiers
├── web/
│   └── index.html        Self-contained UI (CSS + vanilla JS)
├── resources/
│   ├── app.manifest      DPI-aware manifest
│   └── resource.rc       Version info and resources
├── .gitignore
└── README.md
```

## Architecture

1. **Splash Screen** (native GDI+): Animated loading screen with spinner and "Loading UI…" text while WebView2 initializes.

2. **WebView2 UI**: Self-contained HTML file (`web/index.html`) loaded via virtual host mapping. Tabbed navigation with a tool grid, categorized by tool author.

3. **JS/C++ Bridge**: `AddHostObjectToScript` exposes bridge methods for download, launch, script execution, and window controls. C++ sends progress/completion events via `PostWebMessageAsJson`.

4. **Tool Manager**: Downloads tools from GitHub releases using WinHTTP, manages the `./tools/{category}/` folder structure, and launches executables via `ShellExecuteEx`.

## Safety

- All downloads require user confirmation before execution.
- No Windows Defender exclusion modifications.
- No silent execution — all operations are transparent.
- Each tool is launched with a visible window.


