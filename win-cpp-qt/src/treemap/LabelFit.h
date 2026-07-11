#pragma once

#include "config/TreemapConfig.h"
#include "model/FolderDescriptor.h"
#include <QFont>
#include <QRect>
#include <QString>

namespace wtw::treemap {

// Port of win-go/internal/ui/run_windows.go label resolution.
enum class LabelMode {
    Hidden,
    HorizWithDetails,
    HorizNoDetails,
    HorizWithDetailsShort,
    HorizNoDetailsShort,
};

struct LabelChoice {
    LabelMode mode = LabelMode::Hidden;
    QString heading;
    int pt = 8;
    bool withDetails = false;
};

struct LabelLayout {
    QRect inner;
    QFont nameFont;
    QFont metaFont;
    int nameLH = 0;
    int metaLH = 0;
    int gap = 0;
    QString shareText;
    bool showShare = false;
    int contentW = 0;
    int contentH = 0;
};

LabelChoice resolveTileLabel(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi);
bool computeLabelLayout(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                        const QString& heading, int namePt, bool withDetails, LabelLayout* out);
bool tileLabelFits(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                   const QString& heading, int headingPt, bool withDetails);
QString labelPlaceholder(const config::TreemapSettings& cfg);
QString labelDummy(const config::TreemapSettings& cfg);
bool tileNeedsTooltip(const LabelChoice& choice, const config::TreemapSettings& cfg);

}  // namespace wtw::treemap
