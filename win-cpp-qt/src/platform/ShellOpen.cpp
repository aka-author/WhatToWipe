#include "platform/ShellOpen.h"

#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace wtw::platform {

ShellExploreResult exploreFolder(const QString& folderPath) {
    const QString path = QDir::toNativeSeparators(QDir::cleanPath(folderPath));
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isDir()) {
        return ShellExploreResult::FolderMissing;
    }
#ifdef _WIN32
    const std::wstring w = path.toStdWString();
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"explore";
    sei.lpFile = w.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        return ShellExploreResult::ExplorerFailed;
    }
    return ShellExploreResult::Success;
#else
    Q_UNUSED(path);
    return ShellExploreResult::ExplorerFailed;
#endif
}

ShellOpenFileResult openFile(const QString& filePath) {
    const QString path = QDir::toNativeSeparators(QDir::cleanPath(filePath));
    const QFileInfo fi(path);
    if (!fi.exists()) {
        return ShellOpenFileResult::FileMissing;
    }
    if (fi.isDir()) {
        return ShellOpenFileResult::IsDirectory;
    }
#ifdef _WIN32
    const std::wstring w = path.toStdWString();
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = w.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        return ShellOpenFileResult::LaunchFailed;
    }
    return ShellOpenFileResult::Success;
#else
    Q_UNUSED(path);
    return ShellOpenFileResult::LaunchFailed;
#endif
}

}  // namespace wtw::platform
