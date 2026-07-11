#include "model/FolderDescriptor.h"

namespace wtw::model {

static TreeRole computeTreeRole(const FolderDescriptor& f) {
    if (f.children.empty() && f.files.empty()) return TreeRole::EmptyFolder;
    if (f.children.empty()) return TreeRole::LeafFolder;
    return TreeRole::NodeFolder;
}

void annotateShares(FolderDescriptor& node, quint64 driveTotal) {
    if (driveTotal == 0) node.volumeShare = 0;
    else node.volumeShare = static_cast<double>(node.size) / static_cast<double>(driveTotal);
    node.treeRole = computeTreeRole(node);
    for (auto& c : node.children) annotateShares(c, driveTotal);
    for (auto& f : node.files) {
        if (driveTotal == 0) f.volumeShare = 0;
        else f.volumeShare = static_cast<double>(f.size) / static_cast<double>(driveTotal);
    }
}

}  // namespace wtw::model
