#include "platform/VolumeInfo.h"

#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace wtw::platform {

std::optional<QString> rootForPath(const QString& path) {
    const QString cleaned = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (cleaned.isEmpty()) {
        return std::nullopt;
    }
#ifdef _WIN32
    if (cleaned.startsWith(QStringLiteral("\\\\"))) {
        const QStringList parts = cleaned.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            return QStringLiteral("\\\\%1\\%2\\").arg(parts[0], parts[1]);
        }
        return cleaned + QLatin1Char('\\');
    }
    const int colon = cleaned.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        return cleaned.left(colon + 1) + QLatin1Char('\\');
    }
#endif
    return QDir::rootPath();
}

QString driveLabel(const QString& root) {
    const QString cleaned = QDir::cleanPath(root);
#ifdef _WIN32
    if (cleaned.startsWith(QStringLiteral("\\\\"))) {
        return QDir::toNativeSeparators(cleaned);
    }
    const int colon = cleaned.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        return cleaned.left(colon + 1);
    }
#endif
    return QStringLiteral("?");
}

bool diskSpace(const QString& path, quint64* totalOut, quint64* freeOut) {
    if (totalOut) {
        *totalOut = 0;
    }
    if (freeOut) {
        *freeOut = 0;
    }
    const auto root = rootForPath(path);
    if (!root) {
        return false;
    }
#ifdef _WIN32
    ULARGE_INTEGER freeBytes{};
    ULARGE_INTEGER totalBytes{};
    const std::wstring w = root->toStdWString();
    if (!GetDiskFreeSpaceExW(w.c_str(), &freeBytes, &totalBytes, nullptr)) {
        return false;
    }
    if (totalOut) {
        *totalOut = static_cast<quint64>(totalBytes.QuadPart);
    }
    if (freeOut) {
        *freeOut = static_cast<quint64>(freeBytes.QuadPart);
    }
    return true;
#else
    Q_UNUSED(path);
    return false;
#endif
}

VolumeValidation validateLocalVolume(const QString& path) {
    VolumeValidation out;
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isDir()) {
        out.error = QStringLiteral("path does not exist or is not a directory");
        return out;
    }

    const auto root = rootForPath(path);
    if (!root) {
        out.error = QStringLiteral("could not resolve volume root");
        return out;
    }
    out.root = *root;
    out.label = driveLabel(out.root);

#ifdef _WIN32
    const std::wstring w = out.root.toStdWString();
    const UINT driveType = GetDriveTypeW(w.c_str());
    if (driveType == DRIVE_REMOTE) {
        out.error = QStringLiteral("network volume is not supported");
        return out;
    }
#endif

    quint64 total = 0;
    quint64 free = 0;
    if (!diskSpace(path, &total, &free)) {
        out.error = QStringLiteral("could not read volume capacity");
        return out;
    }
    out.totalBytes = total;
    out.freeBytes = free;
    out.ok = true;
    return out;
}

}  // namespace wtw::platform
