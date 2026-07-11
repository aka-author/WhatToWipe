#include "platform/AppVersion.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace wtw::platform {

namespace {

QString versionFromVersionInfoJson(const QString& exeDir) {
    const QStringList candidates = {
        exeDir + QStringLiteral("/versioninfo.json"),
        exeDir + QStringLiteral("/../versioninfo.json"),
    };
    for (const QString& path : candidates) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QString ver = root.value(QStringLiteral("StringFileInfo"))
                                .toObject()
                                .value(QStringLiteral("FileVersion"))
                                .toString()
                                .trimmed();
        if (!ver.isEmpty()) {
            return ver;
        }
    }
    return {};
}

#ifdef _WIN32
QString translationSubBlock(QByteArray& buf) {
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    };
    UINT len = 0;
    LANGANDCODEPAGE* ptr = nullptr;
    if (!VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&ptr), &len)) {
        return {};
    }
    if (len < sizeof(LANGANDCODEPAGE) || !ptr) {
        return {};
    }
    return QStringLiteral("%1%2")
        .arg(ptr->wLanguage, 4, 16, QChar('0'))
        .arg(ptr->wCodePage, 4, 16, QChar('0'));
}

QString fileVersionFromPe(const QString& exePath) {
    const std::wstring w = exePath.toStdWString();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(w.c_str(), &handle);
    if (size == 0) {
        return {};
    }
    QByteArray buf(static_cast<int>(size), Qt::Uninitialized);
    if (!GetFileVersionInfoW(w.c_str(), 0, size, buf.data())) {
        return {};
    }
    const QString trans = translationSubBlock(buf);
    if (trans.isEmpty()) {
        return {};
    }
    const QString subBlock = QStringLiteral("\\StringFileInfo\\%1\\FileVersion").arg(trans);
    LPVOID value = nullptr;
    UINT valueLen = 0;
    if (!VerQueryValueW(buf.data(), reinterpret_cast<LPCWSTR>(subBlock.utf16()), &value, &valueLen)) {
        return {};
    }
    if (!value || valueLen < 2) {
        return {};
    }
    const auto* chars = static_cast<const wchar_t*>(value);
    return QString::fromWCharArray(chars).trimmed();
}
#endif

}  // namespace

QString fileVersionDotted() {
    const QString fallback = QStringLiteral("0.0.0.0");
    const QString exePath = QCoreApplication::applicationFilePath();
#ifdef _WIN32
    if (!exePath.isEmpty()) {
        const QString peVer = fileVersionFromPe(QFileInfo(exePath).absoluteFilePath());
        if (!peVer.isEmpty()) {
            return peVer;
        }
    }
#endif
    if (!exePath.isEmpty()) {
        const QString jsonVer = versionFromVersionInfoJson(QFileInfo(exePath).absolutePath());
        if (!jsonVer.isEmpty()) {
            return jsonVer;
        }
    }
    return fallback;
}

}  // namespace wtw::platform
