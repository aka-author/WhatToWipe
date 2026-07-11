#include "util/Format.h"

#include <QDir>

#include <cmath>

namespace wtw::util {

namespace {

QString formatObjectSizeImpl(quint64 n) {
    if (n == 0) {
        return QStringLiteral("0.0 KB");
    }
    constexpr quint64 KB = 1024;
    constexpr quint64 MB = KB * 1024;
    constexpr quint64 GB = MB * 1024;
    constexpr quint64 TB = GB * 1024;

    double v = 0.0;
    const char* suf = "KB";
    if (n >= TB) {
        v = static_cast<double>(n) / static_cast<double>(TB);
        suf = "TB";
    } else if (n >= GB) {
        v = static_cast<double>(n) / static_cast<double>(GB);
        suf = "GB";
    } else if (n >= MB) {
        v = static_cast<double>(n) / static_cast<double>(MB);
        suf = "MB";
    } else {
        v = static_cast<double>(n) / static_cast<double>(KB);
    }
    return QString::asprintf("%.1f %s", v, suf);
}

}  // namespace

QString formatObjectSize(qint64 bytes) {
    if (bytes < 0) {
        return formatObjectSizeImpl(0);
    }
    return formatObjectSizeImpl(static_cast<quint64>(bytes));
}

QString formatObjectSize(quint64 bytes) { return formatObjectSizeImpl(bytes); }

QString formatPathForStatusBar(const QString& path) {
    if (path.isEmpty()) {
        return path;
    }
#ifdef _WIN32
    return QDir::toNativeSeparators(QDir::cleanPath(path));
#else
    return QDir::cleanPath(path);
#endif
}

QString formatShareLine(double share, bool* showOut) {
    if (showOut) {
        *showOut = false;
    }
    if (!std::isfinite(share) || share < 0) {
        share = 0;
    }
    const double pct = share * 100.0;
    QString s1 = QString::number(pct, 'f', 1);
    if (s1 == QStringLiteral("0.0")) {
        const QString s2 = QString::number(pct, 'f', 2);
        if (s2 == QStringLiteral("0.00")) {
            return QString();
        }
        if (showOut) {
            *showOut = true;
        }
        return s2 + QLatin1Char('%');
    }
    if (s1.endsWith(QStringLiteral(".0"))) {
        s1.chop(2);
    }
    if (showOut) {
        *showOut = true;
    }
    return s1 + QLatin1Char('%');
}

}  // namespace wtw::util
