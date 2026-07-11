#pragma once

#include <QString>

namespace wtw::platform {

enum class ShellExploreResult {
    Success,
    FolderMissing,
    ExplorerFailed,
};

enum class ShellOpenFileResult {
    Success,
    FileMissing,
    LaunchFailed,
    IsDirectory,
};

ShellExploreResult exploreFolder(const QString& folderPath);
ShellOpenFileResult openFile(const QString& filePath);

}  // namespace wtw::platform
