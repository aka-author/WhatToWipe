#include "model/FolderDescriptor.h"

namespace wtw::model {

static TreeRole computeTreeRole(const FolderDescriptor& folder) {
    if (folder.traversalState == scan::TraversalState::Unreadable ||
        folder.traversalState == scan::TraversalState::ReparseTargetNotTraversed) {
        return TreeRole::NodeFolder;
    }
    if (folder.children.empty() && folder.files.empty()) {
        return TreeRole::EmptyFolder;
    }
    if (folder.children.empty()) {
        return TreeRole::LeafFolder;
    }
    return TreeRole::NodeFolder;
}

void annotateShares(FolderDescriptor& node, quint64 driveTotal) {
    if (driveTotal == 0) {
        node.volumeShare = 0;
    } else {
        node.volumeShare = static_cast<double>(node.measuredSize) / static_cast<double>(driveTotal);
    }
    node.treeRole = computeTreeRole(node);
    for (auto& child : node.children) {
        annotateShares(child, driveTotal);
    }
    for (auto& file : node.files) {
        if (driveTotal == 0) {
            file.volumeShare = 0;
        } else {
            file.volumeShare = static_cast<double>(file.size) / static_cast<double>(driveTotal);
        }
    }
}

}  // namespace wtw::model
