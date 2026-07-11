#include "treemap/TreemapWidget.h"

#include "treemap/LabelFit.h"
#include "util/Format.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace wtw::treemap {

namespace {

int widgetDpi(const QWidget* w) {
    return static_cast<int>(w->devicePixelRatioF() * 96.0);
}

model::BlockLayout externalTileRect(const model::BlockLayout& block, const QRect& chartRect) {
    model::BlockLayout out = block;
    if (chartRect.isEmpty()) {
        return out;
    }
    out.rect = block.rect.intersected(chartRect);
    return out;
}

void drawTreemapTileLabel(QPainter& p, const model::BlockLayout& block, const config::TreemapSettings& cfg,
                          int dpi, const QString& heading, int namePt, bool withDetails) {
    LabelLayout lay;
    if (!computeLabelLayout(block, cfg, dpi, heading, namePt, withDetails, &lay)) {
        return;
    }
    p.setPen(block.item.text);
    int y = lay.inner.top();
    auto drawLine = [&](const QString& text, const QFont& font, int lh) {
        p.setFont(font);
        p.drawText(QRect(lay.inner.left(), y, lay.inner.width(), lh), Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                   text);
        y += lh;
    };
    drawLine(heading, lay.nameFont, lay.nameLH);
    if (withDetails) {
        y += lay.gap;
        drawLine(util::formatObjectSize(block.item.size), lay.metaFont, lay.metaLH);
        if (lay.showShare) {
            drawLine(lay.shareText, lay.metaFont, lay.metaLH);
        }
    }
}

void drawTileLabelAuto(QPainter& p, const model::BlockLayout& block, const config::TreemapSettings& cfg, int dpi,
                       const LabelChoice& choice) {
    if (choice.mode == LabelMode::Hidden) {
        int pt = choice.pt;
        if (pt <= 0) {
            pt = cfg.headingMinFontSizePt;
            if (pt <= 0) {
                pt = 8;
            }
        }
        drawTreemapTileLabel(p, block, cfg, dpi, labelDummy(cfg), pt, false);
        return;
    }
    drawTreemapTileLabel(p, block, cfg, dpi, choice.heading, choice.pt, choice.withDetails);
}

}  // namespace

TreemapWidget::TreemapWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void TreemapWidget::setBlocks(std::vector<model::BlockLayout> blocks, const config::TreemapSettings& cfg) {
    m_blocks = std::move(blocks);
    m_cfg = cfg;
    m_labels.clear();
    m_labels.reserve(m_blocks.size());
    const int dpi = widgetDpi(this);
    const QRect chartRect = rect();
    for (const auto& b : m_blocks) {
        m_labels.push_back(resolveTileLabel(externalTileRect(b, chartRect), m_cfg, dpi));
    }
    update();
}

void TreemapWidget::clearBlocks() {
    m_blocks.clear();
    m_labels.clear();
    update();
}

const model::BlockLayout* TreemapWidget::blockAt(const QPoint& pos) const {
    for (const auto& b : m_blocks) {
        if (b.rect.contains(pos)) {
            return &b;
        }
    }
    return nullptr;
}

void TreemapWidget::updateCursorFor(const QPoint& pos) {
    const model::BlockLayout* b = blockAt(pos);
    if (!b) {
        unsetCursor();
        QToolTip::hideText();
        return;
    }
    switch (b->item.kind) {
    case model::TreemapItemKind::Folder:
        if (b->item.isEmpty) {
            setCursor(Qt::ForbiddenCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        break;
    case model::TreemapItemKind::File:
        if (b->item.isExecFile) {
            setCursor(Qt::ForbiddenCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        break;
    case model::TreemapItemKind::Clump:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void TreemapWidget::showTileMenu(const model::BlockLayout& block, const QPoint& globalPos) {
    if (block.item.kind == model::TreemapItemKind::Clump) {
        return;
    }
    QMenu menu(this);
    if (block.item.kind == model::TreemapItemKind::Folder) {
        QAction* dive = menu.addAction(QStringLiteral("Dive"));
        dive->setEnabled(!block.item.isEmpty);
        QAction* explore = menu.addAction(QStringLiteral("Explore"));
        connect(dive, &QAction::triggered, this, [this, block]() { emit diveRequested(block.item.path); });
        connect(explore, &QAction::triggered, this, [this, block]() { emit exploreRequested(block.item.path); });
    } else if (block.item.kind == model::TreemapItemKind::File) {
        QAction* open = menu.addAction(QStringLiteral("Open"));
        open->setEnabled(!block.item.isExecFile);
        QAction* explore = menu.addAction(QStringLiteral("Explore"));
        connect(open, &QAction::triggered, this, [this, block]() { emit openFileRequested(block.item.path); });
        connect(explore, &QAction::triggered, this,
                [this, block]() { emit exploreRequested(QFileInfo(block.item.path).absolutePath()); });
    }
    if (!menu.isEmpty()) {
        menu.exec(globalPos);
    }
}

void TreemapWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(250, 250, 252));
    const int dpi = widgetDpi(this);
    const QRect chartRect = rect();

    for (size_t i = 0; i < m_blocks.size(); ++i) {
        const auto& b = m_blocks[i];
        p.fillRect(b.rect, b.item.bg);
        p.setPen(QColor(40, 40, 45));
        p.drawRect(b.rect.adjusted(0, 0, -1, -1));
        drawTileLabelAuto(p, externalTileRect(b, chartRect), m_cfg, dpi, m_labels[i]);
    }
    p.setPen(QColor(25, 25, 30));
    p.drawRect(chartRect.adjusted(0, 0, -1, -1));
}

void TreemapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_blocks.empty()) {
        return;
    }
    const int dpi = widgetDpi(this);
    const QRect chartRect = rect();
    m_labels.clear();
    m_labels.reserve(m_blocks.size());
    for (const auto& b : m_blocks) {
        m_labels.push_back(resolveTileLabel(externalTileRect(b, chartRect), m_cfg, dpi));
    }
    update();
}

void TreemapWidget::mouseMoveEvent(QMouseEvent* event) {
    updateCursorFor(event->pos());
    const model::BlockLayout* b = blockAt(event->pos());
    if (!b) {
        QToolTip::hideText();
        return;
    }
    const size_t idx = static_cast<size_t>(b - &m_blocks[0]);
    const LabelChoice& choice = m_labels[idx];
    if (tileNeedsTooltip(choice, m_cfg)) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           b->item.name + QStringLiteral("\n") + util::formatObjectSize(b->item.size), this);
    } else {
        QToolTip::hideText();
    }
}

void TreemapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const model::BlockLayout* b = blockAt(event->pos());
    if (!b) {
        return;
    }
    if (b->item.kind == model::TreemapItemKind::Folder && !b->item.isEmpty) {
        emit diveRequested(b->item.path);
    }
}

void TreemapWidget::contextMenuEvent(QContextMenuEvent* event) {
    const model::BlockLayout* b = blockAt(event->pos());
    if (!b) {
        return;
    }
    showTileMenu(*b, event->globalPos());
}

void TreemapWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    unsetCursor();
    QToolTip::hideText();
}

}  // namespace wtw::treemap
