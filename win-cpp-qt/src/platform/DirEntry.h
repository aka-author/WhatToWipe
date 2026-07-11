#pragma once

#include <QString>
#include <windows.h>

namespace wtw::platform {

struct DirEntry {
    QString name;
    QString fullPath;
    DWORD attributes = 0;
    DWORD reparseTag = 0;
    quint64 size = 0;
    FILETIME creationTime{};
    FILETIME lastWriteTime{};
    bool isDirectory = false;
    bool isReparsePoint = false;
};

}  // namespace wtw::platform
