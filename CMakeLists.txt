cmake_minimum_required(VERSION 3.22)
project(SSTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /permissive- /Zc:__cplusplus /EHsc")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFESTUAC:level='requireAdministrator'")

# WebView2 SDK
include(FetchContent)
set(WEBVIEW2_VERSION "1.0.2739.15")
FetchContent_Declare(
    webview2
    URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(webview2)

# WebView2 NuGet package structure:
#   build/native/include/    -> headers
#   build/native/x64/        -> x64 libs
#   runtimes/win-x64/native/ -> runtime DLLs
set(WEBVIEW2_INCLUDE_DIR "${webview2_SOURCE_DIR}/build/native/include")
set(WEBVIEW2_LIB_DIR "${webview2_SOURCE_DIR}/build/native/x64")

message(STATUS "WebView2 include: ${WEBVIEW2_INCLUDE_DIR}")
message(STATUS "WebView2 lib: ${WEBVIEW2_LIB_DIR}")

file(TO_CMAKE_PATH "${WEBVIEW2_INCLUDE_DIR}" WEBVIEW2_INCLUDE_DIR)
file(TO_CMAKE_PATH "${WEBVIEW2_LIB_DIR}" WEBVIEW2_LIB_DIR)

# Sources
set(SOURCES
    src/main.cpp
    src/splash.cpp
    src/webview_manager.cpp
    src/tool_manager.cpp
)

set(HEADERS
    src/splash.h
    src/webview_manager.h
    src/tool_manager.h
    src/resource.h
)

add_executable(SSTool WIN32 ${SOURCES} ${HEADERS}
    resources/resource.rc
)

target_include_directories(SSTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WEBVIEW2_INCLUDE_DIR}
)

target_link_directories(SSTool PRIVATE
    ${WEBVIEW2_LIB_DIR}
)

target_link_libraries(SSTool PRIVATE
    WebView2LoaderStatic.lib
    gdiplus.lib
    urlmon.lib
    ole32.lib
    oleaut32.lib
    comctl32.lib
    shlwapi.lib
    dwmapi.lib
    user32.lib
)

target_compile_definitions(SSTool PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    _UNICODE
    UNICODE
)



# Install rules
install(TARGETS SSTool RUNTIME DESTINATION bin)
install(DIRECTORY web/ DESTINATION bin/web)
