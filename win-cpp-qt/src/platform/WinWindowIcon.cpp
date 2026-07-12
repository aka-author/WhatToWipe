#include "platform/WinWindowIcon.h"

#include <QWidget>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <unordered_map>

namespace wtw::platform {
namespace {

struct WindowIcons {
    HICON small = nullptr;
    HICON big = nullptr;
};

std::unordered_map<HWND, WindowIcons> g_icons;

HICON loadIconForSize(HINSTANCE instance, int cx, int cy) {
    return static_cast<HICON>(LoadImageW(instance,
                                         MAKEINTRESOURCEW(1),
                                         IMAGE_ICON,
                                         cx,
                                         cy,
                                         LR_DEFAULTCOLOR));
}

void destroyIcons(WindowIcons& icons) {
    if (icons.small) {
        DestroyIcon(icons.small);
        icons.small = nullptr;
    }
    if (icons.big) {
        DestroyIcon(icons.big);
        icons.big = nullptr;
    }
}

}  // namespace

void applyWindowIcons(QWidget* widget) {
    if (!widget) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    const UINT dpi = GetDpiForWindow(hwnd);
    const int smallCx = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int smallCy = GetSystemMetricsForDpi(SM_CYSMICON, dpi);
    const int bigCx = GetSystemMetricsForDpi(SM_CXICON, dpi);
    const int bigCy = GetSystemMetricsForDpi(SM_CYICON, dpi);

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WindowIcons replacement;
    replacement.small = loadIconForSize(instance, smallCx, smallCy);
    replacement.big = loadIconForSize(instance, bigCx, bigCy);

    if (!replacement.small || !replacement.big) {
        destroyIcons(replacement);
        return;
    }

    const auto found = g_icons.find(hwnd);
    if (found != g_icons.end()) {
        destroyIcons(found->second);
    }
    g_icons[hwnd] = replacement;

    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(replacement.small));
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(replacement.big));
    RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
}

void releaseWindowIcons(QWidget* widget) {
    if (!widget) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    const auto found = g_icons.find(hwnd);
    if (found == g_icons.end()) {
        return;
    }

    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, 0);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, 0);
    destroyIcons(found->second);
    g_icons.erase(found);
}

}  // namespace wtw::platform
