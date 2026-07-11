#include "scan/SubtreeMerge.h"

#include <QDir>
#include <algorithm>
#include <functional>

namespace wtw::scan {

namespace {

std::optional<QDateTime> minTime(std::optional<QDateTime> a, std::optional<QDateTime> b) {
    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }
    return (*a < *b) ? a : b;
}

std::optional<QDateTime> maxTime(std::optional<QDateTime> a, std::optional<QDateTime> b) {
    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }
    return (*a > *b) ? a : b;
}

}  // namespace

void recomputeAggregates(model::FolderDescriptor& node) {
    qint64 total = 0;
    int nestedFolders = 0;
    int nestedFiles = 0;
    std::optional<QDateTime> oldest;
    std::optional<QDateTime> newest;

    for (const auto& f : node.files) {
        total += f.size;
        ++nestedFiles;
        oldest = minTime(oldest, f.oldestFile);
        newest = maxTime(newest, f.newestFile);
    }
    for (auto& child : node.children) {
        recomputeAggregates(child);
        total += child.size;
        nestedFolders += 1 + child.nestedFolderCount;
        nestedFiles += child.nestedFileCount;
        oldest = minTime(oldest, child.oldestFile);
        newest = maxTime(newest, child.newestFile);
    }

    node.size = total;
    node.nestedFolderCount = nestedFolders;
    node.nestedFileCount = nestedFiles;
    node.oldestFile = oldest;
    node.newestFile = newest;
    if (node.children.empty() && node.files.empty()) {
        node.treeRole = model::TreeRole::EmptyFolder;
    } else if (node.children.empty()) {
        node.treeRole = model::TreeRole::LeafFolder;
    } else {
        node.treeRole = model::TreeRole::NodeFolder;
    }
}

std::optional<model::FolderDescriptor> mergeSubtree(const model::FolderDescriptor& oldTree,
                                                    const QString& scanRootPath,
                                                    const model::FolderDescriptor& newSubtree) {
    const QString root = QDir::cleanPath(scanRootPath);
    const QString oldRoot = QDir::cleanPath(oldTree.fullPath);
    if (oldRoot.compare(root, Qt::CaseInsensitive) == 0) {
        model::FolderDescriptor merged = newSubtree;
        recomputeAggregates(merged);
        return merged;
    }

    model::FolderDescriptor result = oldTree;
    bool found = false;

    std::function<bool(model::FolderDescriptor&)> walk = [&](model::FolderDescriptor& node) -> bool {
        if (QDir::cleanPath(node.fullPath).compare(root, Qt::CaseInsensitive) == 0) {
            node = newSubtree;
            found = true;
            return true;
        }
        for (auto& child : node.children) {
            if (walk(child)) {
                return true;
            }
        }
        return false;
    };

    if (!walk(result)) {
        return std::nullopt;
    }
    recomputeAggregates(result);
    return result;
}

}  // namespace wtw::scan
