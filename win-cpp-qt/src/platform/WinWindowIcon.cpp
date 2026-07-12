#include "platform/WinWindowIcon.h"

#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wtw::platform {

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
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const LPCWSTR iconId = MAKEINTRESOURCEW(1);

    const int smallW = GetSystemMetrics(SM_CXSMICON);
    const int smallH = GetSystemMetrics(SM_CYSMICON);
    const int bigW = GetSystemMetrics(SM_CXICON);
    const int bigH = GetSystemMetrics(SM_CYICON);

    HICON smallIcon = static_cast<HICON>(
        LoadImageW(instance, iconId, IMAGE_ICON, smallW, smallH, LR_DEFAULTCOLOR));
    HICON bigIcon = static_cast<HICON>(
        LoadImageW(instance, iconId, IMAGE_ICON, bigW, bigH, LR_DEFAULTCOLOR));

    if (smallIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
    if (bigIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    }
#else
    (void)widget;
#endif
}

}  // namespace wtw::platform
