#include "app/UpdatePublish.h"

#include "scan/SubtreeMerge.h"

#include <QDir>
#include <functional>

namespace wtw::app {

const model::FolderDescriptor* findFolderByContextPath(const model::FolderDescriptor& publishedTree,
                                                       const QString& targetPath, const QString& contextPath) {
    if (contextPath.isEmpty() ||
        QDir::cleanPath(contextPath).compare(QDir::cleanPath(targetPath), Qt::CaseInsensitive) == 0) {
        return &publishedTree;
    }
    const QString wanted = QDir::cleanPath(contextPath);
    if (QDir::cleanPath(publishedTree.fullPath).compare(wanted, Qt::CaseInsensitive) == 0) {
        return &publishedTree;
    }
    std::function<const model::FolderDescriptor*(const model::FolderDescriptor&)> find =
        [&](const model::FolderDescriptor& node) -> const model::FolderDescriptor* {
        if (QDir::cleanPath(node.fullPath).compare(wanted, Qt::CaseInsensitive) == 0) {
            return &node;
        }
        for (const auto& child : node.children) {
            if (const model::FolderDescriptor* hit = find(child)) {
                return hit;
            }
        }
        return nullptr;
    };
    return find(publishedTree);
}

QString resolveRestoredUpdateContext(const model::FolderDescriptor& restoredTree, const QString& targetPath,
                                     const QString& liveContextPath, const QString& snapshotContextPath) {
    const QString target = QDir::cleanPath(targetPath);
    const QString live = liveContextPath.isEmpty() ? target : QDir::cleanPath(liveContextPath);
    const QString snapshot = snapshotContextPath.isEmpty() ? target : QDir::cleanPath(snapshotContextPath);

    if (findFolderByContextPath(restoredTree, target, live)) {
        return live;
    }
    if (findFolderByContextPath(restoredTree, target, snapshot)) {
        return snapshot;
    }
    if (findFolderByContextPath(restoredTree, target, target)) {
        return target;
    }
    return snapshot;
}

void restorePendingUpdateSession(Session& session) {
    if (!session.pendingUpdateSnapshot) {
        return;
    }
    const UpdateSnapshot snapshot = *session.pendingUpdateSnapshot;
    const QString liveContext = session.contextPath;
    session.publishedTree = cloneFolder(snapshot.tree);
    session.treemapComplete = true;
    session.contextPath =
        resolveRestoredUpdateContext(session.publishedTree, session.targetPath, liveContext, snapshot.contextPath);
}

PreparedUpdatePublication prepareUpdatePublication(const UpdateSnapshot& snapshot, const QString& scanRoot,
                                                   const model::FolderDescriptor& newSubtree,
                                                   const QString& liveContextPath, const QString& targetPath,
                                                   quint64 currentDescriptorVersion, quint64 driveTotal) {
    PreparedUpdatePublication prepared;
    auto merged = scan::mergeSubtree(snapshot.tree, scanRoot, newSubtree);
    if (!merged) {
        prepared.status = UpdatePublicationStatus::MergeFailed;
        return prepared;
    }

    model::annotateShares(*merged, driveTotal);
    const QString resolvedContext =
        liveContextPath.isEmpty() ? snapshot.contextPath : QDir::cleanPath(liveContextPath);
    const QString resolvedTarget =
        targetPath.isEmpty() ? QDir::cleanPath(snapshot.tree.fullPath) : QDir::cleanPath(targetPath);
    if (!findFolderByContextPath(*merged, resolvedTarget, resolvedContext)) {
        prepared.status = UpdatePublicationStatus::ContextMissing;
        prepared.error004Target = resolvedContext;
        return prepared;
    }

    prepared.status = UpdatePublicationStatus::Ready;
    prepared.tree = std::move(*merged);
    prepared.contextPath = resolvedContext;
    prepared.descriptorVersion = currentDescriptorVersion + 1;
    return prepared;
}

void publishPreparedUpdate(Session& session, PreparedUpdatePublication prepared) {
    session.publishedTree = std::move(prepared.tree);
    session.contextPath = std::move(prepared.contextPath);
    session.descriptorVersion = prepared.descriptorVersion;
    session.treemapComplete = true;
    session.pendingUpdateSnapshot.reset();
}

}  // namespace wtw::app
