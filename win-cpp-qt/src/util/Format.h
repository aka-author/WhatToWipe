#pragma once

#include <QString>

namespace wtw::util {

QString formatObjectSize(qint64 bytes);
QString formatShareLine(double share, bool* showOut = nullptr);

}  // namespace wtw::util
