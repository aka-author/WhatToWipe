#include "platform/WinWindowIcon.h"

#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wtw::platform {

#ifdef _WIN32
namespace {

UINT queryWindowDpi(HWND hwnd) {
    if (auto getDpiForWindow = reinterpret_cast<UINT(WINAPI*)(HWND)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"))) {
        const UINT dpi = getDpiForWindow(hwnd);
        if (dpi != 0) {
            return dpi;
        }
    }
    return GetDpiForSystem();
}

int scaleForDpi(int logical96Px, UINT dpi) {
    return MulDiv(logical96Px, static_cast<int>(dpi), 96);
}

HICON loadEmbeddedIcon(int pixels) {
    return static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(1),
        IMAGE_ICON,
        pixels,
        pixels,
        LR_DEFAULTCOLOR));
}

}  // namespace
#endif

void applyWin32WindowIcons(QWidget* widget) {
#ifdef _WIN32
    if (!widget) {
        return;
    }
    const WId wid = widget->winId();
    if (!wid) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(wid);
    const UINT dpi = queryWindowDpi(hwnd);

    // Windows 10/11 taskbar and Alt+Tab use 24x24 at 96 DPI, not SM_CXICON (32).
    // MSFT: LoadImage for 24x24 into ICON_BIG.
    const int titlePx = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int taskbarPx = scaleForDpi(24, dpi);

    if (HICON titleIcon = loadEmbeddedIcon(titlePx)) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(titleIcon));
    }
    if (HICON taskbarIcon = loadEmbeddedIcon(taskbarPx)) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(taskbarIcon));
    }
#else
    (void)widget;
#endif
}

}  // namespace wtw::platform
