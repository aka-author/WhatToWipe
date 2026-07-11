#include "scan/SubtreeMerge.h"

#include "util/CheckedMath.h"

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

bool childMakesParentPartial(const model::FolderDescriptor& child) {
    return child.sizeCompleteness == SizeCompleteness::Partial ||
           child.traversalState == TraversalState::Unreadable;
}

}  // namespace

void recomputeAggregates(model::FolderDescriptor& node) {
    quint64 measured = 0;
    bool partial = node.traversalState == TraversalState::Unreadable;
    int nestedFolders = 0;
    int nestedFiles = 0;
    std::optional<QDateTime> oldest;
    std::optional<QDateTime> newest;

    for (const auto& file : node.files) {
        measured = util::checkedAdd(measured, file.size);
        ++nestedFiles;
        oldest = minTime(oldest, file.oldestFile);
        newest = maxTime(newest, file.newestFile);
    }

    for (auto& child : node.children) {
        recomputeAggregates(child);
        measured = util::checkedAdd(measured, child.measuredSize);
        nestedFolders += 1 + child.nestedFolderCount;
        nestedFiles += child.nestedFileCount;
        oldest = minTime(oldest, child.oldestFile);
        newest = maxTime(newest, child.newestFile);
        if (childMakesParentPartial(child)) {
            partial = true;
        }
    }

    node.measuredSize = measured;
    node.sizeCompleteness = partial ? SizeCompleteness::Partial : SizeCompleteness::Complete;
    node.nestedFolderCount = nestedFolders;
    node.nestedFileCount = nestedFiles;
    node.oldestFile = oldest;
    node.newestFile = newest;

    if (node.traversalState == TraversalState::Unreadable ||
        node.traversalState == TraversalState::ReparseTargetNotTraversed) {
        node.treeRole = model::TreeRole::NodeFolder;
    } else if (node.children.empty() && node.files.empty()) {
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
