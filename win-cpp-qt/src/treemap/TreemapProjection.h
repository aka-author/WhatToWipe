#pragma once

#include "config/TreemapConfig.h"
#include "model/FolderDescriptor.h"
#include <vector>

namespace wtw::treemap {

// Port of win-go/internal/scan/treemap_build.go BuildTreemapItems.
std::vector<model::TreemapItem> buildTreemapItems(const model::FolderDescriptor* contextFolder,
                                                  quint64 driveTotal,
                                                  const config::TreemapSettings& cfg);

}  // namespace wtw::treemap
