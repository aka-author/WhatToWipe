#include "app/MainWindow.h"

#include "config/ConfigStore.h"
#include "model/FolderDescriptor.h"
#include "platform/ShellOpen.h"
#include "platform/VolumeInfo.h"
#include "scan/ScanWorker.h"
#include "scan/SubtreeMerge.h"
#include "treemap/TreemapLayout.h"
#include "treemap/TreemapProjection.h"
#include "treemap/TreemapWidget.h"
#include "ui/AboutDialog.h"
#include "ui/AlertDialogs.h"
#include "ui/SettingsDialog.h"
#include "util/Format.h"

#include <QAction>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wtw::app {

MainWindow::MainWindow(const config::TreemapSettings& settings, QWidget* parent)
    : QMainWindow(parent), m_cfg(settings) {
    setWindowTitle(QStringLiteral("WhatToWipe"));
    resize(1100, 720);
    buildUi();
    buildMenus();
    m_session.resetToInitial();
    setStatusText(QStringLiteral("Choose a target folder"));
    updateChrome();
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* strip = new QWidget(central);
    auto* stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(0, 0, 0, 0);

    m_openBtn = new QToolButton(strip);
    m_openBtn->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_openBtn->setToolTip(QStringLiteral("Open a folder"));
    m_openBtn->setAutoRaise(true);
    stripLayout->addWidget(m_openBtn);

    m_upBtn = new QToolButton(strip);
    m_upBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_upBtn->setToolTip(QStringLiteral("Go up"));
    m_upBtn->setAutoRaise(true);
    stripLayout->addWidget(m_upBtn);

    m_exploreBtn = new QToolButton(strip);
    m_exploreBtn->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
    m_exploreBtn->setToolTip(QStringLiteral("Open in file manager"));
    m_exploreBtn->setAutoRaise(true);
    stripLayout->addWidget(m_exploreBtn);

    m_scanBtn = new QToolButton(strip);
    m_scanBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_scanBtn->setToolTip(QStringLiteral("Update the folder data"));
    m_scanBtn->setAutoRaise(true);
    stripLayout->addWidget(m_scanBtn);

    connect(m_openBtn, &QToolButton::clicked, this, &MainWindow::onOpenFolder);
    connect(m_upBtn, &QToolButton::clicked, this, &MainWindow::onUp);
    connect(m_exploreBtn, &QToolButton::clicked, this, &MainWindow::onExplore);
    connect(m_scanBtn, &QToolButton::clicked, this, &MainWindow::onUpdateOrStop);

    m_totalLabel = new QLabel(QStringLiteral("Total at ?:"), strip);
    m_freeLabel = new QLabel(QStringLiteral("Free at ?:"), strip);
    m_freeBtn = new QPushButton(QStringLiteral("-"), strip);
    connect(m_freeBtn, &QPushButton::clicked, this, &MainWindow::onRefreshFree);
    stripLayout->addStretch();
    stripLayout->addWidget(m_totalLabel);
    stripLayout->addWidget(m_freeLabel);
    stripLayout->addWidget(m_freeBtn);

    m_treemap = new treemap::TreemapWidget(central);
    connect(m_treemap, &treemap::TreemapWidget::diveRequested, this, &MainWindow::onDive);
    connect(m_treemap, &treemap::TreemapWidget::exploreRequested, this, &MainWindow::onExploreTile);
    connect(m_treemap, &treemap::TreemapWidget::openFileRequested, this, &MainWindow::onOpenFile);

    layout->addWidget(strip);
    layout->addWidget(m_treemap, 1);
    setCentralWidget(central);
    statusBar()->showMessage(QString());
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    m_openAct = fileMenu->addAction(QStringLiteral("Open a Folder"), this, &MainWindow::onOpenFolder);
    m_openAct->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close, QKeySequence::Quit);

    auto* inspectMenu = menuBar()->addMenu(QStringLiteral("Inspect"));
    m_upAct = inspectMenu->addAction(QStringLiteral("Up"), this, &MainWindow::onUp);
    m_exploreAct = inspectMenu->addAction(QStringLiteral("Explore"), this, &MainWindow::onExplore);
    inspectMenu->addSeparator();
    m_updateAct = inspectMenu->addAction(QStringLiteral("Update"), this, &MainWindow::onUpdateOrStop);
    m_stopAct = inspectMenu->addAction(QStringLiteral("Stop"), this, &MainWindow::onUpdateOrStop);

    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    m_settingsAct = toolsMenu->addAction(QStringLiteral("Settings..."), this, &MainWindow::onSettings);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("Help"));
    m_aboutAct = helpMenu->addAction(QStringLiteral("About"), this, &MainWindow::onAbout);
}

void MainWindow::setStatusText(const QString& text) { statusBar()->showMessage(text); }

QString MainWindow::statusForContext() const {
    if (!m_session.treemapComplete) {
        return QStringLiteral("Choose a target folder");
    }
    return m_session.contextPath.isEmpty() ? m_session.targetPath : m_session.contextPath;
}

void MainWindow::refreshVolumeToolbar() {
    const QString label = m_session.volLabel.isEmpty() ? QStringLiteral("?") : m_session.volLabel;
    QString totalText = QStringLiteral("Total at %1:").arg(label);
    totalText.replace(QStringLiteral("::"), QStringLiteral(":"));
    m_totalLabel->setText(totalText);
    QString freeText = QStringLiteral("Free at %1:").arg(label);
    freeText.replace(QStringLiteral("::"), QStringLiteral(":"));
    m_freeLabel->setText(freeText);
    if (m_session.driveTotal > 0) {
        m_totalLabel->setToolTip(util::formatObjectSize(static_cast<qint64>(m_session.driveTotal)));
    }
    m_freeBtn->setText(m_session.driveFree > 0 ? util::formatObjectSize(static_cast<qint64>(m_session.driveFree))
                                               : QStringLiteral("-"));
}

void MainWindow::updateChrome() {
    const bool scanning = m_session.scanning;
    const bool hasTarget = !m_session.targetPath.isEmpty();
    const bool complete = m_session.treemapComplete;

    m_openAct->setEnabled(!scanning);
    m_openBtn->setEnabled(!scanning);
    m_upAct->setEnabled(!scanning && m_session.canGoUp());
    m_upBtn->setEnabled(m_upAct->isEnabled());
    m_exploreAct->setEnabled(complete);
    m_exploreBtn->setEnabled(complete);
    m_updateAct->setVisible(!scanning);
    m_stopAct->setVisible(scanning);
    m_updateAct->setEnabled(!scanning && hasTarget && complete);
    m_scanBtn->setEnabled(scanning || m_updateAct->isEnabled());
    m_scanBtn->setIcon(style()->standardIcon(scanning ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    m_scanBtn->setToolTip(scanning ? QStringLiteral("Stop scanning folders")
                                   : QStringLiteral("Update the folder data"));
    m_settingsAct->setEnabled(!scanning);
    m_aboutAct->setEnabled(!scanning);

    if (scanning) {
        setCursor(Qt::BusyCursor);
    } else {
        unsetCursor();
    }
}

void MainWindow::rebuildTreemap() {
    const model::FolderDescriptor* ctx = m_session.resolveContextFolder();
    if (!ctx) {
        m_treemap->clearBlocks();
        return;
    }
    const auto items = treemap::buildTreemapItems(ctx, m_session.driveTotal, m_cfg);
    const QRect area = m_treemap->rect();
    const int dpi = static_cast<int>(m_treemap->devicePixelRatioF() * 96.0);
    auto mulDiv = [](int value, int numerator, int denominator) {
        return static_cast<int>((static_cast<qint64>(value) * numerator + denominator / 2) / denominator);
    };
    int minW = mulDiv(m_cfg.minTileWidthPt, dpi, 72);
    int minH = mulDiv(m_cfg.minTileHeightPt, dpi, 72);
    if (minW < 1) {
        minW = 1;
    }
    if (minH < 1) {
        minH = 1;
    }
    const auto blocks = treemap::squarify(items, area, minW, minH);
    m_treemap->setBlocks(blocks, m_cfg);
}

void MainWindow::unsetTreemap() {
    m_session.resetToInitial();
    m_treemap->clearBlocks();
    refreshVolumeToolbar();
    setStatusText(QStringLiteral("Choose a target folder"));
    updateChrome();
}

void MainWindow::restorePendingUpdate() {
    if (!m_session.pendingUpdateSnapshot) {
        return;
    }
    m_session.publishedTree = cloneFolder(m_session.pendingUpdateSnapshot->tree);
    m_session.contextPath = m_session.pendingUpdateSnapshot->contextPath;
    m_session.pendingUpdateSnapshot.reset();
    m_session.treemapComplete = true;
    rebuildTreemap();
    setStatusText(statusForContext());
}

void MainWindow::busyPointerTwoSeconds() {
    setCursor(Qt::BusyCursor);
    QTimer::singleShot(2000, this, [this]() {
        if (!m_session.scanning) {
            unsetCursor();
        }
    });
}

void MainWindow::onOpenFolder() {
    if (m_session.scanning) {
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Open a folder"));
    if (dir.isEmpty()) {
        return;
    }

    const auto vol = platform::validateLocalVolume(dir);
    if (!vol.ok) {
        ui::showError001(this);
        return;
    }

    m_session.pendingUpdateSnapshot.reset();
    m_session.targetPath = QDir::cleanPath(dir);
    m_session.contextPath = m_session.targetPath;
    m_session.volBarRoot = vol.root;
    m_session.volLabel = vol.label;
    m_session.driveTotal = vol.totalBytes;
    m_session.driveFree = vol.freeBytes;
    refreshVolumeToolbar();
    m_treemap->clearBlocks();
    m_session.treemapComplete = false;
    startScan(m_session.targetPath, ScanKind::OpenTarget);
}

void MainWindow::onUp() {
    if (m_session.scanning || !m_session.canGoUp()) {
        return;
    }
    m_session.goUp();
    rebuildTreemap();
    setStatusText(statusForContext());
    updateChrome();
}

void MainWindow::onExplore() {
    const model::FolderDescriptor* ctx = m_session.resolveContextFolder();
    if (!ctx) {
        return;
    }
    onExploreTile(ctx->fullPath);
}

void MainWindow::onExploreTile(const QString& path) {
    const QString cleaned = QDir::cleanPath(path);
    const QFileInfo fi(cleaned);
    if (!fi.exists() || !fi.isDir()) {
        ui::showError004(this, cleaned);
        if (cleaned == m_session.contextPath || cleaned == m_session.targetPath) {
            unsetTreemap();
        }
        return;
    }
    switch (platform::exploreFolder(cleaned)) {
    case platform::ShellExploreResult::FolderMissing:
        ui::showError004(this, cleaned);
        if (cleaned == m_session.contextPath || cleaned == m_session.targetPath) {
            unsetTreemap();
        }
        break;
    case platform::ShellExploreResult::ExplorerFailed:
        ui::showError003(this);
        break;
    case platform::ShellExploreResult::Success:
        busyPointerTwoSeconds();
        break;
    }
}

void MainWindow::onOpenFile(const QString& path) {
    switch (platform::openFile(path)) {
    case platform::ShellOpenFileResult::FileMissing:
        break;
    case platform::ShellOpenFileResult::LaunchFailed:
    case platform::ShellOpenFileResult::IsDirectory:
        break;
    case platform::ShellOpenFileResult::Success:
        busyPointerTwoSeconds();
        break;
    }
}

void MainWindow::onUpdateOrStop() {
    if (m_session.scanning) {
        if (m_scanWorker) {
            m_scanWorker->requestCancel();
        }
        return;
    }
    if (m_session.targetPath.isEmpty() || !m_session.treemapComplete) {
        return;
    }
    UpdateSnapshot snap;
    snap.tree = cloneFolder(m_session.publishedTree);
    snap.contextPath = m_session.contextPath.isEmpty() ? m_session.targetPath : m_session.contextPath;
    m_session.pendingUpdateSnapshot = snap;
    const QString scanRoot = snap.contextPath;
    startScan(scanRoot, ScanKind::UpdateContext);
}

void MainWindow::onSettings() {
    ui::showSettingsDialog(this, m_cfg, [this](const config::TreemapSettings& next) {
        m_cfg = next;
        rebuildTreemap();
    });
}

void MainWindow::onAbout() {
    ui::AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::onRefreshFree() {
    if (m_session.targetPath.isEmpty()) {
        return;
    }
    quint64 total = 0;
    quint64 free = 0;
    if (platform::diskSpace(m_session.targetPath, &total, &free)) {
        m_session.driveTotal = total;
        m_session.driveFree = free;
        model::annotateShares(m_session.publishedTree, m_session.driveTotal);
        refreshVolumeToolbar();
        rebuildTreemap();
    }
}

void MainWindow::onDive(const QString& folderPath) {
    if (m_session.scanning) {
        return;
    }
    m_session.pushContext(folderPath);
    rebuildTreemap();
    setStatusText(statusForContext());
    updateChrome();
}

void MainWindow::startScan(const QString& scanRoot, ScanKind kind) {
    if (m_session.scanning) {
        return;
    }

    ++m_session.scanId;
    const quint64 scanId = m_session.scanId;
    m_session.scanning = true;
    m_session.scanKind = kind;
    m_session.scanRootPath = QDir::cleanPath(scanRoot);
    m_lastProgressEmitMs = 0;
    m_latestProgressPath.clear();
    updateChrome();

    if (kind == ScanKind::OpenTarget) {
        m_session.treemapComplete = false;
        m_treemap->clearBlocks();
    }

    m_scanThread = new QThread(this);
    m_scanWorker = new scan::ScanWorker(m_session.scanRootPath);
    m_scanWorker->moveToThread(m_scanThread);

    connect(m_scanThread, &QThread::started, m_scanWorker, &scan::ScanWorker::run);
    connect(m_scanWorker, &scan::ScanWorker::progress, this, &MainWindow::onScanProgress);
    connect(m_scanWorker, &scan::ScanWorker::finished, this, &MainWindow::onScanFinished);
    connect(m_scanWorker, &scan::ScanWorker::finished, m_scanThread, &QThread::quit);
    connect(m_scanWorker, &scan::ScanWorker::finished, m_scanWorker, &QObject::deleteLater);
    connect(m_scanThread, &QThread::finished, m_scanThread, &QObject::deleteLater);
    connect(m_scanThread, &QThread::finished, this, [this, scanId]() {
        if (m_session.scanId == scanId) {
            m_scanThread = nullptr;
            m_scanWorker = nullptr;
        }
    });

    m_scanThread->start();
}

void MainWindow::onScanProgress(const QString& path) {
    m_latestProgressPath = path;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastProgressEmitMs != 0 && now - m_lastProgressEmitMs < 500) {
        return;
    }
    m_lastProgressEmitMs = now;
    setStatusText(m_latestProgressPath);
}

void MainWindow::onScanFinished(model::FolderDescriptor tree, int errorCount, bool cancelled, bool rootUnavailable,
                                bool technicalFailure) {
    Q_UNUSED(errorCount);
    const ScanKind kind = m_session.scanKind;
    const quint64 finishedScanId = m_session.scanId;
    const QString scanRoot = m_session.scanRootPath;
    const QString expectedTarget = m_session.targetPath;

    m_session.scanning = false;
    updateChrome();

    if (technicalFailure) {
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
        } else {
            unsetTreemap();
        }
        ui::showError002(this);
        m_session.pendingUpdateSnapshot.reset();
        return;
    }

    if (cancelled) {
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
            ui::showInterruptionAlert(this);
        } else {
            unsetTreemap();
            ui::showInterruptionAlert(this);
        }
        m_session.pendingUpdateSnapshot.reset();
        return;
    }

    if (rootUnavailable) {
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
            ui::showError001(this);
        } else {
            unsetTreemap();
            ui::showError001(this);
        }
        m_session.pendingUpdateSnapshot.reset();
        return;
    }

    tree.fullPath = scanRoot;
    tree.name = QFileInfo(scanRoot).fileName();
    if (tree.name.isEmpty()) {
        tree.name = scanRoot;
    }
    model::annotateShares(tree, m_session.driveTotal);

    if (kind == ScanKind::OpenTarget) {
        if (!expectedTarget.isEmpty()) {
            const QFileInfo targetFi(expectedTarget);
            if (!targetFi.exists()) {
                ui::showError004(this, expectedTarget);
                unsetTreemap();
                return;
            }
        }
        m_session.publishedTree = std::move(tree);
        m_session.contextPath = scanRoot;
        m_session.treemapComplete = true;
        m_session.pendingUpdateSnapshot.reset();
        rebuildTreemap();
        setStatusText(statusForContext());
        return;
    }

    // UpdateContext: merge subtree into published target tree.
    if (!m_session.pendingUpdateSnapshot) {
        return;
    }
    const QFileInfo ctxFi(scanRoot);
    if (!ctxFi.exists()) {
        unsetTreemap();
        ui::showError004(this, scanRoot);
        m_session.pendingUpdateSnapshot.reset();
        return;
    }

    auto merged = scan::mergeSubtree(m_session.pendingUpdateSnapshot->tree, scanRoot, tree);
    if (!merged) {
        restorePendingUpdate();
        ui::showError002(this);
        m_session.pendingUpdateSnapshot.reset();
        return;
    }
    model::annotateShares(*merged, m_session.driveTotal);
    m_session.publishedTree = std::move(*merged);
    m_session.contextPath = m_session.pendingUpdateSnapshot->contextPath;
    m_session.treemapComplete = true;
    m_session.pendingUpdateSnapshot.reset();
    rebuildTreemap();
    setStatusText(statusForContext());
}

}  // namespace wtw::app
