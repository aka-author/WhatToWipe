#pragma once

#include "model/FolderDescriptor.h"
#include <QString>
#include <optional>

namespace wtw::scan {

std::optional<model::FolderDescriptor> mergeSubtree(const model::FolderDescriptor& oldTree,
                                                    const QString& scanRootPath,
                                                    const model::FolderDescriptor& newSubtree);

void recomputeAggregates(model::FolderDescriptor& node);

}  // namespace wtw::scan
