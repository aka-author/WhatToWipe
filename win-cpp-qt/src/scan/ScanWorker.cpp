#include "scan/ScanWorker.h"

#include "platform/WinDirEnum.h"
#include "scan/ArchiveClassifier.h"
#include "scan/SubtreeMerge.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace wtw::scan {

namespace {

ScanOutcome rootOutcomeForStatus(DirectoryReadStatus status) {
    switch (status) {
    case DirectoryReadStatus::AccessDenied:
    case DirectoryReadStatus::SharingViolation:
    case DirectoryReadStatus::NotFound:
        return ScanOutcome::RootUnavailable;
    default:
        return ScanOutcome::TechnicalFailure;
    }
}

}  // namespace

ScanWorker::ScanWorker(QString rootPath, ScanIdentity identity, QObject* parent)
    : QObject(parent), m_rootPath(std::move(rootPath)), m_identity(identity) {}

void ScanWorker::requestCancel() { m_cancel.store(true); }

void ScanWorker::maybeEmitProgress(const QString& path) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastProgressMs == 0 || now - m_lastProgressMs >= 500) {
        m_lastProgressMs = now;
        emit progress(m_identity, path);
    }
}

bool ScanWorker::isDirectoryReparsePoint(const platform::DirEntry& entry) {
    return entry.isDirectory && entry.isReparsePoint;
}

QDateTime ScanWorker::fileTimeFromEntry(const platform::DirEntry& entry, bool creation) {
    const FILETIME& ft = creation ? entry.creationTime : entry.lastWriteTime;
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    if (uli.QuadPart == 0) {
        return {};
    }
    static const QDateTime epoch(QDate(1601, 1, 1), QTime(0, 0, 0), Qt::UTC);
    return epoch.addMSecs(static_cast<qint64>(uli.QuadPart / 10000));
}

model::FolderDescriptor ScanWorker::scanDir(const QString& path, ScanDiagnostics* diagnostics, bool* cancelled,
                                            bool isRoot, DirectoryReadStatus* rootReadStatus) {
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
    node.traversalState = TraversalState::Complete;
    node.sizeCompleteness = SizeCompleteness::Complete;
    node.measuredSize = 0;

    const auto isCancelled = [this]() { return m_cancel.load(); };
    const DirectoryReadResult readResult = platform::enumerateDirectory(path, isCancelled);
    const DirectoryReadStatus status = readResult.status();

    if (status == DirectoryReadStatus::Cancelled) {
        if (cancelled) {
            *cancelled = true;
        }
        return {};
    }

    if (status == DirectoryReadStatus::Ok) {
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

        for (const platform::DirEntry& entry : readResult.entries()) {
            if (m_cancel.load()) {
                if (cancelled) {
                    *cancelled = true;
                }
                return {};
            }

            if (entry.isDirectory) {
                if (isDirectoryReparsePoint(entry)) {
                    model::FolderDescriptor reparse;
                    reparse.name = entry.name;
                    reparse.fullPath = entry.fullPath;
                    reparse.measuredSize = 0;
                    reparse.sizeCompleteness = SizeCompleteness::Complete;
                    reparse.traversalState = TraversalState::ReparseTargetNotTraversed;
                    reparse.treeRole = model::TreeRole::NodeFolder;
                    if (diagnostics) {
                        ++diagnostics->reparseNotTraversedCount;
                    }
                    node.children.push_back(std::move(reparse));
                    continue;
                }

                model::FolderDescriptor child =
                    scanDir(entry.fullPath, diagnostics, cancelled, false, rootReadStatus);
                if (cancelled && *cancelled) {
                    return {};
                }
                node.children.push_back(std::move(child));
                continue;
            }

            model::FileDescriptor file;
            file.name = entry.name;
            file.fullPath = entry.fullPath;
            file.size = entry.size;
            file.type = model::ObjectType::File;
            const QDateTime mod = fileTimeFromEntry(entry, false);
            file.oldestFile = mod;
            file.newestFile = mod;
            touchTime(mod);

            const QString ext = QFileInfo(entry.name).suffix().toLower();
            if (ext == QStringLiteral("zip") || ext == QStringLiteral("rar")) {
                file.packing = classifyArchiveFile(entry.fullPath);
            }
            node.files.push_back(std::move(file));
        }

        node.oldestFile = oldest;
        node.newestFile = newest;
    } else {
        if (isRoot && rootReadStatus) {
            *rootReadStatus = status;
        }
        node.traversalState = TraversalState::Unreadable;
        node.treeRole = model::TreeRole::NodeFolder;
        node.measuredSize = 0;
        node.sizeCompleteness = SizeCompleteness::Partial;
        if (diagnostics) {
            ++diagnostics->unreadableDirectoryCount;
        }
        return node;
    }

    std::sort(node.files.begin(), node.files.end(),
              [](const model::FileDescriptor& a, const model::FileDescriptor& b) { return a.size > b.size; });
    std::sort(node.children.begin(), node.children.end(), [](const model::FolderDescriptor& a,
                                                             const model::FolderDescriptor& b) {
        return a.measuredSize > b.measuredSize;
    });

    recomputeAggregates(node);
    maybeEmitProgress(path);
    return node;
}

void ScanWorker::run() {
    ScanDiagnostics diagnostics;
    bool cancelled = false;
    DirectoryReadStatus rootReadStatus = DirectoryReadStatus::Invalid;

    try {
        model::FolderDescriptor tree = scanDir(m_rootPath, &diagnostics, &cancelled, true, &rootReadStatus);

        if (cancelled) {
            emit finished(ScanResult::cancelled(m_identity, diagnostics));
            return;
        }

        if (rootReadStatus != DirectoryReadStatus::Invalid && rootReadStatus != DirectoryReadStatus::Ok) {
            const ScanOutcome outcome = rootOutcomeForStatus(rootReadStatus);
            if (outcome == ScanOutcome::RootUnavailable) {
                emit finished(ScanResult::rootUnavailable(m_identity, diagnostics));
            } else {
                emit finished(ScanResult::technicalFailure(m_identity, diagnostics));
            }
            return;
        }

        tree.fullPath = QDir::cleanPath(m_rootPath);
        tree.name = QFileInfo(m_rootPath).fileName();
        if (tree.name.isEmpty()) {
            tree.name = tree.fullPath;
        }
        recomputeAggregates(tree);
        emit finished(ScanResult::success(m_identity, std::move(tree), diagnostics));
    } catch (const std::exception&) {
        emit finished(ScanResult::technicalFailure(m_identity, diagnostics));
    }
}

}  // namespace wtw::scan
