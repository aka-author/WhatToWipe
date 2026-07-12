#include "platform/WinWindowIcon.h"

#include <ui/AppIcon.h>

#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QWidget>
#include <QtGui/QRgb>

#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wtw::platform {

#ifdef _WIN32
namespace {

HICON hiconFromImage(const QImage& source) {
    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return nullptr;
    }

    const int w = image.width();
    const int h = image.height();

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    if (!screen) {
        return nullptr;
    }

    void* colorBits = nullptr;
    HBITMAP colorBitmap =
        CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &colorBits, nullptr, 0);
    if (!colorBitmap || !colorBits) {
        ReleaseDC(nullptr, screen);
        return nullptr;
    }
    std::memcpy(colorBits, image.constBits(), static_cast<size_t>(image.sizeInBytes()));

    const int maskStride = ((w + 31) / 32) * 4;
    std::vector<BYTE> maskBits(static_cast<size_t>(maskStride * h), 0xFF);
    for (int y = 0; y < h; ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(row[x]) < 128) {
                maskBits[static_cast<size_t>(y * maskStride + (x / 8))] &=
                    static_cast<BYTE>(~(1u << (7 - (x % 8))));
            }
        }
    }

    HBITMAP maskBitmap = CreateBitmap(w, h, 1, 1, maskBits.data());

    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = colorBitmap;
    info.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&info);

    DeleteObject(colorBitmap);
    DeleteObject(maskBitmap);
    ReleaseDC(nullptr, screen);
    return icon;
}

HICON hiconFromIcon(const QIcon& icon, int logicalSize, qreal devicePixelRatio) {
    const QPixmap pixmap = icon.pixmap(QSize(logicalSize, logicalSize), devicePixelRatio);
    if (pixmap.isNull()) {
        return nullptr;
    }
    return hiconFromImage(pixmap.toImage());
}

UINT windowDpi(HWND hwnd) {
    if (auto getDpi = reinterpret_cast<UINT(WINAPI*)(HWND)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"))) {
        return getDpi(hwnd);
    }
    return static_cast<UINT>(GetDpiForSystem());
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
    const QIcon icon = wtw::ui::appWindowIcon();
    if (icon.isNull()) {
        return;
    }

    const qreal dpr = widget->devicePixelRatioF();
    const UINT dpi = windowDpi(hwnd);

    // Title bar uses the small slot; Windows 11 taskbar uses ~24 logical px.
    const int titleLogical = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int taskbarLogical = 24;

    if (HICON small = hiconFromIcon(icon, titleLogical, dpr)) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
    }
    if (HICON big = hiconFromIcon(icon, taskbarLogical, dpr)) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    }
#else
    (void)widget;
#endif
}

}  // namespace wtw::platform
