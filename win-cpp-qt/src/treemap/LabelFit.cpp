#include "treemap/LabelFit.h"

#include "util/Format.h"

#include <QFontMetrics>

namespace wtw::treemap {

namespace {

int mulDiv(int value, int numerator, int denominator) {
    return static_cast<int>((static_cast<qint64>(value) * numerator + denominator / 2) / denominator);
}

double ratioOr(double v, double def) {
    if (v <= 0) {
        return def;
    }
    return v;
}

void tilePaddingPx(const config::TreemapSettings& cfg, int dpi, int* left, int* top, int* right, int* bottom) {
    auto nz = [](int v, int def) {
        if (v <= 0) {
            return def;
        }
        return v;
    };
    *left = mulDiv(nz(cfg.tilePaddingLeftPt, 5), dpi, 72);
    *top = mulDiv(nz(cfg.tilePaddingTopPt, 5), dpi, 72);
    *right = mulDiv(nz(cfg.tilePaddingRightPt, 5), dpi, 72);
    *bottom = mulDiv(nz(cfg.tilePaddingBottomPt, 5), dpi, 72);
}

QString shortenedHeadingByKeep(const QString& s, const QString& placeholder, int keep) {
    const int n = s.size();
    if (keep <= 0) {
        return placeholder;
    }
    if (keep >= n) {
        return s;
    }
    const int leftKeep = keep / 2;
    const int rightKeep = keep - leftKeep;
    if (leftKeep + rightKeep > n) {
        return s;
    }
    return s.left(leftKeep) + placeholder + s.right(rightKeep);
}

bool findBestFontSizeForMode(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                             const QString& heading, int minPt, int maxPt, bool withDetails, int* outPt) {
    if (minPt <= 0 || maxPt <= 0 || minPt > maxPt) {
        return false;
    }
    if (!tileLabelFits(block, cfg, dpi, heading, minPt, withDetails)) {
        return false;
    }
    if (tileLabelFits(block, cfg, dpi, heading, maxPt, withDetails)) {
        *outPt = maxPt;
        return true;
    }
    int lo = minPt;
    int hi = maxPt;
    int best = minPt;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        if (tileLabelFits(block, cfg, dpi, heading, mid, withDetails)) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    *outPt = best;
    return true;
}

bool findBestShortenedHeadingAtMinFont(const model::BlockLayout& block, const config::TreemapSettings& cfg,
                                       int dpi, const QString& heading, int minPt, bool withDetails,
                                       QString* outHeading) {
    if (minPt <= 0) {
        return false;
    }
    const QString ph = labelPlaceholder(cfg);
    const int n = heading.size();
    const int phLen = ph.size();
    if (n == 0 || phLen == 0 || n <= phLen) {
        return false;
    }
    const int maxKeep = n - phLen;
    if (maxKeep <= 0) {
        return false;
    }
    if (!tileLabelFits(block, cfg, dpi, ph, minPt, withDetails)) {
        return false;
    }
    for (int keep = maxKeep; keep >= 0; --keep) {
        const QString candidate = shortenedHeadingByKeep(heading, ph, keep);
        if (tileLabelFits(block, cfg, dpi, candidate, minPt, withDetails)) {
            *outHeading = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace

QString labelPlaceholder(const config::TreemapSettings& cfg) {
    const QString s = cfg.labelPlaceholder.trimmed();
    if (s.isEmpty()) {
        return QStringLiteral("...");
    }
    return s;
}

QString labelDummy(const config::TreemapSettings& cfg) {
    const QString s = cfg.labelDummy.trimmed();
    if (s.isEmpty()) {
        return QStringLiteral("...");
    }
    return s;
}

bool computeLabelLayout(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                        const QString& heading, int namePt, bool withDetails, LabelLayout* lay) {
    if (!lay) {
        return false;
    }
    *lay = LabelLayout{};

    int metaPt = static_cast<int>(static_cast<double>(namePt) * ratioOr(cfg.detailsFontSizeRatio, 0.8) + 0.5);
    if (metaPt < 1) {
        metaPt = 1;
    }

    lay->nameFont = QFont(cfg.tileFontName);
    lay->nameFont.setPointSize(namePt);
    lay->metaFont = QFont(cfg.tileFontName);
    lay->metaFont.setPointSize(metaPt);

    int padL = 0;
    int padT = 0;
    int padR = 0;
    int padB = 0;
    tilePaddingPx(cfg, dpi, &padL, &padT, &padR, &padB);
    lay->inner = block.rect.adjusted(padL, padT, -padR, -padB);
    if (lay->inner.isEmpty()) {
        return false;
    }

    lay->nameLH =
        static_cast<int>(static_cast<double>(namePt) * ratioOr(cfg.headingLineHeight, 1.2) * dpi / 72.0 + 0.5);
    lay->metaLH =
        static_cast<int>(static_cast<double>(metaPt) * ratioOr(cfg.detailsLineHeight, 1.5) * dpi / 72.0 + 0.5);

    QFontMetrics nameFm(lay->nameFont);
    QFontMetrics metaFm(lay->metaFont);
    const int measuredNameH = nameFm.boundingRect(QStringLiteral("Agjpyq")).height();
    if (measuredNameH > 0 && lay->nameLH < measuredNameH + 2) {
        lay->nameLH = measuredNameH + 2;
    }
    const int measuredMetaH = metaFm.boundingRect(QStringLiteral("Agjpyq")).height();
    if (measuredMetaH > 0 && lay->metaLH < measuredMetaH + 2) {
        lay->metaLH = measuredMetaH + 2;
    }

    lay->gap = static_cast<int>(static_cast<double>(metaPt) * ratioOr(cfg.aboveDetailsRatio, 1.0) * dpi / 72.0 + 0.5);
    lay->shareText = util::formatShareLine(block.item.driveShare, &lay->showShare);
    if (!withDetails) {
        lay->showShare = false;
    }

    const int nameW = nameFm.horizontalAdvance(heading);
    lay->contentW = nameW;
    lay->contentH = lay->nameLH;
    if (withDetails) {
        const int sizeW = metaFm.horizontalAdvance(util::formatObjectSize(block.item.size));
        if (sizeW > lay->contentW) {
            lay->contentW = sizeW;
        }
        lay->contentH += lay->gap + lay->metaLH;
        if (lay->showShare) {
            const int shareW = metaFm.horizontalAdvance(lay->shareText);
            if (shareW > lay->contentW) {
                lay->contentW = shareW;
            }
            lay->contentH += lay->metaLH;
        }
    }
    return true;
}

bool tileLabelFits(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                   const QString& heading, int headingPt, bool withDetails) {
    LabelLayout lay;
    if (!computeLabelLayout(block, cfg, dpi, heading, headingPt, withDetails, &lay)) {
        return false;
    }
    return lay.contentW <= lay.inner.width() && lay.contentH <= lay.inner.height();
}

LabelChoice resolveTileLabel(const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi) {
    LabelChoice out;
    if (block.rect.isEmpty()) {
        out.mode = LabelMode::Hidden;
        return out;
    }

    int maxPt = cfg.headingMaxFontSizePt;
    int minPt = cfg.headingMinFontSizePt;
    if (maxPt <= 0) {
        maxPt = 30;
    }
    if (minPt <= 0) {
        minPt = 8;
    }
    if (minPt > maxPt) {
        minPt = maxPt;
    }

    int pt = 0;
    if (findBestFontSizeForMode(block, cfg, dpi, block.item.name, minPt, maxPt, true, &pt)) {
        out.mode = LabelMode::HorizWithDetails;
        out.heading = block.item.name;
        out.pt = pt;
        out.withDetails = true;
        return out;
    }

    QString shortened;
    if (findBestShortenedHeadingAtMinFont(block, cfg, dpi, block.item.name, minPt, true, &shortened)) {
        out.mode = LabelMode::HorizWithDetailsShort;
        out.heading = shortened;
        out.pt = minPt;
        out.withDetails = true;
        return out;
    }

    if (tileLabelFits(block, cfg, dpi, block.item.name, minPt, false)) {
        out.mode = LabelMode::HorizNoDetails;
        out.heading = block.item.name;
        out.pt = minPt;
        out.withDetails = false;
        return out;
    }

    if (findBestShortenedHeadingAtMinFont(block, cfg, dpi, block.item.name, minPt, false, &shortened)) {
        out.mode = LabelMode::HorizNoDetailsShort;
        out.heading = shortened;
        out.pt = minPt;
        out.withDetails = false;
        return out;
    }

    out.mode = LabelMode::Hidden;
    out.pt = minPt;
    return out;
}

bool tileNeedsTooltip(const LabelChoice& choice, const config::TreemapSettings& cfg) {
    if (choice.mode == LabelMode::Hidden) {
        return true;
    }
    return choice.heading.contains(labelPlaceholder(cfg));
}

}  // namespace wtw::treemap
