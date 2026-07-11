#pragma once

#include <QString>
#include <optional>

namespace wtw::platform {

struct VolumeValidation {
    bool ok = false;
    QString root;
    QString label;
    quint64 totalBytes = 0;
    quint64 freeBytes = 0;
    QString error;
};

std::optional<QString> rootForPath(const QString& path);
QString driveLabel(const QString& root);
bool diskSpace(const QString& path, quint64* totalOut, quint64* freeOut);
VolumeValidation validateLocalVolume(const QString& path);

}  // namespace wtw::platform
