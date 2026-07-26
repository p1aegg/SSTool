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

## Tools & Scripts

The UI is organized into tabs, each holding a curated set of tools (or, for **Scripts**, PowerShell one-liners run in a visible console window). The list below reflects what's currently defined in `web/index.html`.

### OrbDiff — github.com/orbdiff
- **Prefetch View++** — Parses prefetch extracting file info
- **BAM Reveal** — Parses BAM forensic artefact
- **Amcache Parser** — Parses AMCache with YARA + signatures
- **Journal Parser** — Parses NTFS USNJournal entries
- **Check Deleted USN** — Compares USN timestamp vs boot time
- **Fileless** — Detect fileless via eventlog + memdump
- **JAR Parser** — Parses JAR prefetch, DcomLaunch strings, etc
- **PF Trace** — Rundll32/Regsvr32 prefetch analysis
- **InjGen** — Detects JNI/JVMTI memory injections
- **DPS Analyzer** — Analyzes DPS memory
- **USB Detector** — Detects USB device history
- **User Assist View** — Parses UserAssist artifact
- **Strings Parser** — Strings + YARA + signatures scanner
- **Bam Check Restart** — Detects the date of creation of the BAM

### Spokwn — github.com/spokwn
- **Activities Cache execution** — Gets execution of files using activitiescache.db
- **BAM Parser** — Parses the BAM forensic artefact
- **Bam Deleted Keys** — Gathers BAM deleted keys by comparing them with hive keys
- **Journal Trace** — Parses NTFS journal entries
- **Pca Svc Executed** — Fork of zack-src's service-execution
- **Espouken Tool** — A lot of tools
- **Process Parser** — Parses processes using xxstrings
- **Paths Parser** — Parses info about paths on a .txt

### Tonynoh — github.com/meowtonynoh
- **Meow Resolver** — Detector/resolver for common bypasses
- **Meow Doomsday Fucker** — Doomsday finder
- **Meow Client Fucker** — Scanner
- **Meow Imports Checker** — File checker

### DetectAC — detect.ac
- **Autoruns++** — Rebuilt Autoruns alternative with USN monitoring and signature verification
- **StringExplorer++** — Navigate an exe's full string data, entropy, and VirusTotal integration
- **WinPrefetchView++** — WinPrefetchView with bypass detections and YARA rule support
- **USBDeview++** — Aggregated USB device logs cross-referenced against DeviceHunt
- **SavedFilesViewer++** — Shows every file saved to disk with cross-referenced timestamps
- **SRUMExplorer++** — Maps file paths and services from SRUM with YARA + USN tracking
- **PowerShellParser++** — Scrapes PowerShell history artifacts with bypass detection
- **PathsParser++** — Paths parser GUI with YARA support and a USN journal viewer
- **MFTExplorer++** — Defined $MFT view with suspicious ADS identification
- **KernelLiveDump++** — Dumps Kernel/User-mode RAM with filterable string results
- **JournalTrace++** — USN Journal analysis with bypass detections and filtering
- **CrashedFileViewer++** — Unified view of Windows crash artifacts with USN highlighting
- **BrowsingHistoryView++** — Multi-browser history with suspicious domain flagging + VT links
- **BrowserDownloadsView++** — Multi-browser download history with USN modification highlighting
- **BamParser++** — BAM execution history with YARA engine and tamper detection
- **AmcacheParser++** — High-performance Amcache parser with YARA + VirusTotal

### Scripts
PowerShell one-liners, run via a visible `cmd.exe`/PowerShell console rather than downloaded as tools:
- **Collect System Info** — `Get-ComputerInfo`
- **Running Processes** — Lists running processes sorted by CPU
- **Services Checker** — Runs NiccBlahh/ServiceChecker
- **Zeezy Services** — Runs zeezyexe/services-checker
- **Mod Analyzer** — Runs p1aegg's mod analyzer
- **JAR Parser** — Runs l4rpsucks/Scripts JARParser
- **Fileless Bypass Detection** — Runs l4rpsucks/Scripts FilelessBypassDetection
- **Macro Scanner** — Runs zeezyexe/macro-scanner
- **Antivirus Disabler** — Disables Windows Defender/AV
- **ClassLoader Dump** — Dumps ClassLoader data
- **Yarp's Mod Analyzer** — Runs YarpLetapStan's mod analyzer v6.0
- **DoomsDay Detector** — Runs zedoonvm1's doomsday detector
- **Meow Mod Analyzer** — Runs MeowTonynoh's mod analyzer
- **Prefetch Integrity Analyzer** — Runs RedLotus prefetch integrity analyzer
- **RecordingKiller** — Kills recording/screen-share processes

### NirSoft — nirsoft.net
- **FullEventLogView** — View Windows event logs
- **NetworkUsageView** — Monitor network usage
- **BrowserDownloadsView** — View browser download history
- **AlternateStreamView** — View NTFS alternate data streams
- **USBDeview** — Manage USB devices
- **OpenSaveFilesView** — View Open/Save dialog history
- **ExecutedProgramsList** — List executed programs
- **TaskSchedulerView** — View scheduled tasks
- **JumpListsView** — View Windows Jump Lists
- **WinPrefetchView** — View Windows Prefetch files
- **RegScanner** — Advanced Registry scanner
- **ShellBagsView** — View ShellBags entries

### EricZimmerman — ericzimmerman.github.io
- **AmcacheParser** — Parse Amcache.hve
- **bstrings** — String extractor
- **EvtxECmd** — Event log parser
- **JumpListExplorer** — Jump List explorer
- **MFTECmd** — MFT parser
- **PECmd** — Prefetch parser
- **RegistryExplorer** — Registry viewer
- **ShellBagsExplorer** — ShellBags viewer
- **SrumECmd** — SRUM parser
- **TimelineExplorer** — Timeline explorer

### Others
- **Jarabel** — Locate .jar files on a computer
- **Luyten** — Open-source Java decompiler GUI for Procyon
- **VM Aware** — Advanced VM detection library and tool
- **NTFS Parser** — Forensics tool for NTFS
- **Hayabusa** — Threat hunting and fast forensics timeline generator
- **Everything** — Locate files and folders by name instantly
- **HxD** — Hex editor
- **P1AE Javaw** — Javaw scanner

### Dependencies
- **NET 9.0** — Installs the .NET 9.0 desktop runtime
- **NET 10.0** — Installs the .NET 10.0 desktop runtime
- **VSRedist** — Installs the Visual C++ Redistributable
