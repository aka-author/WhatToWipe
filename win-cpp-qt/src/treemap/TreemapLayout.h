#pragma once

#include "model/FolderDescriptor.h"
#include <QRect>
#include <vector>

namespace wtw::treemap {

std::vector<model::BlockLayout> squarify(const std::vector<model::TreemapItem>& items, const QRect& area,
                                         int minTileW = 0, int minTileH = 0);

}  // namespace wtw::treemap
