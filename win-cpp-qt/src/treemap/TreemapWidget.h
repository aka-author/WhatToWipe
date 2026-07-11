#pragma once

#include "config/TreemapConfig.h"
#include "model/FolderDescriptor.h"
#include "treemap/LabelFit.h"
#include <QWidget>
#include <vector>

namespace wtw::treemap {

class TreemapWidget : public QWidget {
    Q_OBJECT

public:
    explicit TreemapWidget(QWidget* parent = nullptr);

    void setBlocks(std::vector<model::BlockLayout> blocks, const config::TreemapSettings& cfg);
    void clearBlocks();
    bool hasBlocks() const { return !m_blocks.empty(); }

signals:
    void diveRequested(QString folderPath);
    void exploreRequested(QString path);
    void openFileRequested(QString path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    const model::BlockLayout* blockAt(const QPoint& pos) const;
    void updateCursorFor(const QPoint& pos);
    void showTileMenu(const model::BlockLayout& block, const QPoint& globalPos);

    std::vector<model::BlockLayout> m_blocks;
    config::TreemapSettings m_cfg;
    std::vector<wtw::treemap::LabelChoice> m_labels;
};

}  // namespace wtw::treemap
