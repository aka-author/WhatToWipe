#pragma once

#include "scan/ScanTypes.h"
#include <QString>

namespace wtw::util {

QString formatObjectSize(qint64 bytes);
QString formatObjectSize(quint64 bytes);
QString formatFolderSize(quint64 measuredSize, scan::SizeCompleteness completeness);
QString formatShareLine(double share, bool* showOut = nullptr);

}  // namespace wtw::util
