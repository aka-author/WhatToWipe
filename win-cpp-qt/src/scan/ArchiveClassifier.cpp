#include "scan/ArchiveClassifier.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtEndian>

namespace wtw::scan {

namespace {

struct ArcEntry {
    QString name;
    bool isDir = false;
};

QString normArchivePath(const QString& name) {
    QString s = name;
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    s = s.trimmed();
    while (s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    return s;
}

model::PackingType classifyArchiveLayout(const QVector<ArcEntry>& entries) {
    QVector<ArcEntry> names;
    names.reserve(entries.size());
    for (const ArcEntry& e : entries) {
        if (normArchivePath(e.name).isEmpty()) {
            continue;
        }
        names.push_back(e);
    }
    if (names.isEmpty()) {
        return model::PackingType::PackedClump;
    }

    QSet<QString> roots;
    for (const ArcEntry& e : names) {
        const QString n = normArchivePath(e.name);
        const int slash = n.indexOf(QLatin1Char('/'));
        const QString root = slash < 0 ? n : n.left(slash);
        if (!root.isEmpty()) {
            roots.insert(root);
        }
    }
    if (roots.size() != 1) {
        return model::PackingType::PackedClump;
    }
    const QString root = *roots.begin();
    if (root.isEmpty()) {
        return model::PackingType::PackedClump;
    }

    for (const ArcEntry& e : names) {
        const QString n = normArchivePath(e.name);
        if (n != root && !n.startsWith(root + QLatin1Char('/'))) {
            return model::PackingType::PackedClump;
        }
    }

    int rootFiles = 0;
    int underFiles = 0;
    for (const ArcEntry& e : names) {
        if (e.isDir) {
            continue;
        }
        const QString n = normArchivePath(e.name);
        if (n == root) {
            ++rootFiles;
        } else if (n.startsWith(root + QLatin1Char('/'))) {
            ++underFiles;
        }
    }

    if (rootFiles == 1 && underFiles == 0) {
        return model::PackingType::PackedFile;
    }
    if (underFiles > 0) {
        return model::PackingType::PackedFolder;
    }
    if (rootFiles == 0) {
        return model::PackingType::PackedFolder;
    }
    return model::PackingType::PackedClump;
}

bool readZipCentralDirectory(const QString& path, QVector<ArcEntry>* out) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const qint64 fileSize = file.size();
    if (fileSize < 22) {
        return false;
    }

    constexpr quint32 kEocdSig = 0x06054b50u;
    constexpr qint64 kMaxComment = 0xFFFF;
    const qint64 searchStart = qMax<qint64>(0, fileSize - (22 + kMaxComment));
    qint64 eocdOffset = -1;
    for (qint64 pos = fileSize - 22; pos >= searchStart; --pos) {
        file.seek(pos);
        quint32 sig = 0;
        if (file.read(reinterpret_cast<char*>(&sig), 4) != 4) {
            break;
        }
        if (qFromLittleEndian(sig) == kEocdSig) {
            eocdOffset = pos;
            break;
        }
    }
    if (eocdOffset < 0) {
        return false;
    }

    file.seek(eocdOffset + 16);
    quint32 centralOffset = 0;
    if (file.read(reinterpret_cast<char*>(&centralOffset), 4) != 4) {
        return false;
    }
    centralOffset = qFromLittleEndian(centralOffset);

    quint16 entryCount = 0;
    file.seek(eocdOffset + 10);
    if (file.read(reinterpret_cast<char*>(&entryCount), 2) != 2) {
        return false;
    }
    entryCount = qFromLittleEndian(entryCount);

    file.seek(centralOffset);
    constexpr quint32 kCfhSig = 0x02014b50u;
    for (int i = 0; i < entryCount; ++i) {
        quint32 sig = 0;
        if (file.read(reinterpret_cast<char*>(&sig), 4) != 4) {
            return false;
        }
        if (qFromLittleEndian(sig) != kCfhSig) {
            return false;
        }
        quint16 fileNameLen = 0;
        quint16 extraLen = 0;
        quint16 commentLen = 0;
        quint16 externalAttr = 0;
        file.seek(file.pos() + 18);
        if (file.read(reinterpret_cast<char*>(&externalAttr), 2) != 2) {
            return false;
        }
        externalAttr = qFromLittleEndian(externalAttr);
        if (file.read(reinterpret_cast<char*>(&fileNameLen), 2) != 2) {
            return false;
        }
        fileNameLen = qFromLittleEndian(fileNameLen);
        if (file.read(reinterpret_cast<char*>(&extraLen), 2) != 2) {
            return false;
        }
        extraLen = qFromLittleEndian(extraLen);
        if (file.read(reinterpret_cast<char*>(&commentLen), 2) != 2) {
            return false;
        }
        commentLen = qFromLittleEndian(commentLen);
        file.seek(file.pos() + 12);
        const QByteArray nameBytes = file.read(fileNameLen);
        file.seek(file.pos() + extraLen + commentLen);
        ArcEntry entry;
        entry.name = QString::fromUtf8(nameBytes);
        entry.isDir = (externalAttr & 0x10) != 0 || entry.name.endsWith(QLatin1Char('/'));
        out->push_back(entry);
    }
    return true;
}

}  // namespace

model::PackingType classifyArchiveFile(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("zip")) {
        QVector<ArcEntry> entries;
        if (!readZipCentralDirectory(path, &entries)) {
            return model::PackingType::PackedClump;
        }
        return classifyArchiveLayout(entries);
    }
    if (ext == QStringLiteral("rar")) {
        return model::PackingType::PackedClump;
    }
    return model::PackingType::Native;
}

}  // namespace wtw::scan
