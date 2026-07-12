#pragma once

#include <QString>

namespace wtw::util {

QString formatObjectSize(qint64 bytes);
QString formatObjectSize(quint64 bytes);
QString formatPathForStatusBar(const QString& path);
QString formatShareLine(double share, bool* showOut = nullptr);

}  // namespace wtw::util
