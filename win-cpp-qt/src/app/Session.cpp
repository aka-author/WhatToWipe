#include "app/Session.h"

#include <QDir>
#include <QFileInfo>
#include <functional>

namespace wtw::app {

model::FolderDescriptor cloneFolder(const model::FolderDescriptor& src) {
    model::FolderDescriptor out = src;
    out.children.clear();
    out.files = src.files;
    out.children.reserve(src.children.size());
    for (const auto& child : src.children) {
        out.children.push_back(cloneFolder(child));
    }
    return out;
}

void Session::resetToInitial() {
    targetPath.clear();
    contextPath.clear();
    volBarRoot.clear();
    volLabel.clear();
    driveTotal = 0;
    driveFree = 0;
    publishedTree = {};
    treemapComplete = false;
    scanning = false;
    scanId = 0;
    ++sessionId;
    descriptorVersion = 0;
    pendingUpdateSnapshot.reset();
}

const model::FolderDescriptor* Session::resolveContextFolder() const {
    if (!treemapComplete || targetPath.isEmpty()) {
        return nullptr;
    }
    if (contextPath.isEmpty() ||
        QDir::cleanPath(contextPath).compare(QDir::cleanPath(targetPath), Qt::CaseInsensitive) == 0) {
        return &publishedTree;
    }
    const QString wanted = QDir::cleanPath(contextPath);
    const model::FolderDescriptor* cur = &publishedTree;
    if (QDir::cleanPath(cur->fullPath).compare(wanted, Qt::CaseInsensitive) == 0) {
        return cur;
    }
    // Walk tree by matching full paths under target.
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

void Session::setContextPath(const QString& path) { contextPath = QDir::cleanPath(path); }

void Session::pushContext(const QString& folderPath) { contextPath = QDir::cleanPath(folderPath); }

bool Session::canGoUp() const {
    if (!treemapComplete || contextPath.isEmpty()) {
        return false;
    }
    return QDir::cleanPath(contextPath).compare(QDir::cleanPath(targetPath), Qt::CaseInsensitive) != 0;
}

void Session::goUp() {
    if (!canGoUp()) {
        return;
    }
    const QFileInfo fi(contextPath);
    const QString parent = fi.path();
    if (parent.isEmpty()) {
        contextPath = targetPath;
        return;
    }
    if (QDir::cleanPath(parent).startsWith(QDir::cleanPath(targetPath), Qt::CaseInsensitive) ||
        QDir::cleanPath(targetPath).startsWith(QDir::cleanPath(parent), Qt::CaseInsensitive)) {
        contextPath = QDir::cleanPath(parent);
    } else {
        contextPath = targetPath;
    }
}

}  // namespace wtw::app
