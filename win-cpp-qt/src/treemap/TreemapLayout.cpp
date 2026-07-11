#include "treemap/TreemapLayout.h"

#include <algorithm>

namespace wtw::treemap {

namespace {

qint64 max64(qint64 a, qint64 b) { return a > b ? a : b; }

quint64 weightForItem(const model::TreemapItem& item) {
    return item.size == 0 ? 1 : item.size;
}

model::BlockLayout blockFromItem(const model::TreemapItem& it, const QRect& area) {
    model::BlockLayout b;
    b.item = it;
    b.rect = area;
    return b;
}

std::vector<model::BlockLayout> squarifyRecursive(const std::vector<model::TreemapItem>& items,
                                                  const QRect& area) {
    if (items.empty()) {
        return {};
    }
    if (items.size() == 1) {
        return {blockFromItem(items.front(), area)};
    }
    if (area.width() <= 0 || area.height() <= 0) {
        return {blockFromItem(items.front(), area)};
    }

    quint64 total = 0;
    for (const auto& it : items) {
        total += weightForItem(it);
    }
    if (total == 0) {
        total = items.size();
    }

    quint64 leftSum = 0;
    int split = 0;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (i == static_cast<int>(items.size()) - 1) {
            break;
        }
        leftSum += weightForItem(items[static_cast<size_t>(i)]);
        split = i + 1;
        if (leftSum * 2 >= total) {
            break;
        }
    }
    if (split <= 0) {
        split = 1;
    }
    if (split >= static_cast<int>(items.size())) {
        split = static_cast<int>(items.size()) - 1;
    }

    std::vector<model::TreemapItem> aItems(items.begin(), items.begin() + split);
    std::vector<model::TreemapItem> bItems(items.begin() + split, items.end());

    quint64 aSum = 0;
    for (const auto& it : aItems) {
        aSum += weightForItem(it);
    }

    QRect aRect;
    QRect bRect;
    bool tryVertical = area.width() >= area.height();
    if (area.width() <= 1 && area.height() > 1) {
        tryVertical = false;
    }
    if (area.height() <= 1 && area.width() > 1) {
        tryVertical = true;
    }

    if (tryVertical) {
        int wA = static_cast<int>((static_cast<quint64>(area.width()) * aSum) / total);
        if (wA < 1) {
            wA = 1;
        }
        if (wA >= area.width()) {
            wA = area.width() - 1;
        }
        aRect = QRect(area.left(), area.top(), wA, area.height());
        bRect = QRect(area.left() + wA, area.top(), area.width() - wA, area.height());
    } else {
        int hA = static_cast<int>((static_cast<quint64>(area.height()) * aSum) / total);
        if (hA < 1) {
            hA = 1;
        }
        if (hA >= area.height()) {
            hA = area.height() - 1;
        }
        aRect = QRect(area.left(), area.top(), area.width(), hA);
        bRect = QRect(area.left(), area.top() + hA, area.width(), area.height() - hA);
    }

    std::vector<model::BlockLayout> out;
    auto aBlocks = squarifyRecursive(aItems, aRect);
    auto bBlocks = squarifyRecursive(bItems, bRect);
    out.insert(out.end(), aBlocks.begin(), aBlocks.end());
    out.insert(out.end(), bBlocks.begin(), bBlocks.end());
    return out;
}

}  // namespace

std::vector<model::BlockLayout> squarify(const std::vector<model::TreemapItem>& items, const QRect& area,
                                         int minTileW, int minTileH) {
    Q_UNUSED(minTileW);
    Q_UNUSED(minTileH);
    return squarifyRecursive(items, area);
}

}  // namespace wtw::treemap
