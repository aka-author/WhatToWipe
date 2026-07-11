#include "scan/ScanWorker.h"

#include "scan/ArchiveClassifier.h"
#include "scan/SubtreeMerge.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <algorithm>
#include <chrono>
#include <future>

namespace wtw::scan {

ScanWorker::ScanWorker(QString rootPath, QObject* parent)
    : QObject(parent), m_rootPath(std::move(rootPath)) {}

void ScanWorker::requestCancel() { m_cancel.store(true); }

void ScanWorker::maybeEmitProgress(const QString& path) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastProgressMs == 0 || now - m_lastProgressMs >= 500) {
        m_lastProgressMs = now;
        emit progress(path);
    }
}

bool ScanWorker::isDirectoryReparsePoint(const QFileInfo& fi) const {
    if (!fi.isDir()) {
        return false;
    }
#ifdef _WIN32
    return fi.isJunction() || fi.isSymbolicLink();
#else
    return fi.isSymbolicLink();
#endif
}

QVector<QFileInfo> ScanWorker::readDirBounded(const QString& path, bool* timedOut) const {
    if (timedOut) {
        *timedOut = false;
    }
    const auto task = [path]() {
        QVector<QFileInfo> out;
        const QDir dir(path);
        const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries,
                                                        QDir::DirsFirst | QDir::Name);
        out.reserve(entries.size());
        for (const QFileInfo& fi : entries) {
            out.push_back(fi);
        }
        return out;
    };

    auto future = std::async(std::launch::async, task);
    QElapsedTimer timer;
    timer.start();
    while (future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
        if (m_cancel.load()) {
            return {};
        }
        if (timer.elapsed() >= 30000) {
            if (timedOut) {
                *timedOut = true;
            }
            return {};
        }
    }
    return future.get();
}

model::FolderDescriptor ScanWorker::scanDir(const QString& path, int* errorCount, bool* cancelled) {
    if (m_cancel.load()) {
        if (cancelled) {
            *cancelled = true;
        }
        return {};
    }

    maybeEmitProgress(path);

    model::FolderDescriptor node;
    node.fullPath = QDir::cleanPath(path);
    node.name = QFileInfo(path).fileName();
    if (node.name.isEmpty()) {
        node.name = node.fullPath;
    }

    bool timedOut = false;
    const QVector<QFileInfo> entries = readDirBounded(path, &timedOut);
    if (timedOut) {
        if (errorCount) {
            ++(*errorCount);
        }
        node.skipReason = QStringLiteral("timed out reading directory");
        return node;
    }

    qint64 total = 0;
    std::optional<QDateTime> oldest;
    std::optional<QDateTime> newest;

    auto touchTime = [&](const QDateTime& dt) {
        if (!dt.isValid()) {
            return;
        }
        if (!oldest || dt < *oldest) {
            oldest = dt;
        }
        if (!newest || dt > *newest) {
            newest = dt;
        }
    };

    for (const QFileInfo& fi : entries) {
        if (m_cancel.load()) {
            if (cancelled) {
                *cancelled = true;
            }
            return {};
        }

        const QString full = QDir::cleanPath(fi.absoluteFilePath());
        if (fi.isDir()) {
            if (isDirectoryReparsePoint(fi)) {
                model::FolderDescriptor reparse;
                reparse.name = fi.fileName();
                reparse.fullPath = full;
                reparse.size = fi.size();
                reparse.reparseSkipped = true;
                reparse.skipReason = QStringLiteral("directory reparse point not traversed");
                reparse.treeRole = model::TreeRole::EmptyFolder;
                total += reparse.size;
                node.children.push_back(std::move(reparse));
                continue;
            }
            model::FolderDescriptor child = scanDir(full, errorCount, cancelled);
            if (cancelled && *cancelled) {
                return {};
            }
            total += child.size;
            node.children.push_back(std::move(child));
            continue;
        }

        const qint64 sz = fi.size();
        total += sz;
        model::FileDescriptor file;
        file.name = fi.fileName();
        file.fullPath = full;
        file.size = sz;
        file.type = model::ObjectType::File;
        const QDateTime mod = fi.lastModified();
        file.oldestFile = mod;
        file.newestFile = mod;
        touchTime(mod);

        const QString ext = fi.suffix().toLower();
        if (ext == QStringLiteral("zip") || ext == QStringLiteral("rar")) {
            file.packing = classifyArchiveFile(full);
        }
        node.files.push_back(std::move(file));
    }

    std::sort(node.files.begin(), node.files.end(),
              [](const model::FileDescriptor& a, const model::FileDescriptor& b) {
                  return a.size > b.size;
              });
    std::sort(node.children.begin(), node.children.end(),
              [](const model::FolderDescriptor& a, const model::FolderDescriptor& b) {
                  return a.size > b.size;
              });

    node.size = total;
    node.oldestFile = oldest;
    node.newestFile = newest;
    recomputeAggregates(node);
    maybeEmitProgress(path);
    return node;
}

void ScanWorker::run() {
    int errorCount = 0;
    bool cancelled = false;
    model::FolderDescriptor tree = scanDir(m_rootPath, &errorCount, &cancelled);

    const bool rootUnavailable =
        !cancelled && tree.children.empty() && tree.files.empty() && !tree.skipReason.isEmpty();
    const bool technicalFailure =
        !cancelled && tree.skipReason.contains(QStringLiteral("timed out")) && tree.size == 0;

    emit finished(tree, errorCount, cancelled, rootUnavailable, technicalFailure);
}

}  // namespace wtw::scan
