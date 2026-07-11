#include "app/MainWindow.h"

#include "app/Product.h"

#include "config/ConfigStore.h"
#include "model/FolderDescriptor.h"
#include "platform/ShellOpen.h"
#include "platform/VolumeInfo.h"
#include "scan/ScanResult.h"
#include "scan/ScanWorker.h"
#include "scan/SubtreeMerge.h"
#include "treemap/TreemapLayout.h"
#include "treemap/TreemapProjection.h"
#include "treemap/TreemapWidget.h"
#include "ui/AboutDialog.h"
#include "ui/AlertDialogs.h"
#include "ui/MenuLabels.h"
#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcons.h"
#include "util/Format.h"

#include <QAction>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wtw::app {

namespace {

QString normalizeDriveIndicatorToken(const QString& raw) {
    QString s = raw.trimmed();
    if (s.isEmpty()) {
        return QStringLiteral("?");
    }
    while (s.endsWith(QLatin1Char(':'))) {
        s.chop(1);
    }
    return s.isEmpty() ? QStringLiteral("?") : s;
}

QToolButton* makeToolbarButton(QWidget* parent, const QIcon& icon, const QString& tooltip) {
    auto* button = new QToolButton(parent);
    button->setIcon(icon);
    button->setToolTip(tooltip);
    ui::applyToolbarButtonStyle(button);
    return button;
}

}  // namespace

MainWindow::MainWindow(const config::TreemapSettings& settings, QWidget* parent)
    : QMainWindow(parent), m_cfg(settings) {
    qRegisterMetaType<scan::ScanResult>("wtw::scan::ScanResult");
    setWindowTitle(productDisplayName());
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* strip = new QWidget(central);
    strip->setObjectName(QStringLiteral("commandStrip"));
    strip->setStyleSheet(QStringLiteral(
        "#commandStrip { background: #f3f3f3; border-bottom: 1px solid #c5c5c5; }"
        "#commandStrip QToolButton {"
        "  border: 1px solid transparent; border-radius: 4px; background: transparent; }"
        "#commandStrip QToolButton:hover:enabled {"
        "  background: #e6e6e6; border-color: #b8b8b8; }"
        "#commandStrip QToolButton:pressed:enabled { background: #d4d4d4; }"
        "#commandStrip QToolButton:disabled { opacity: 0.4; }"
        "#commandStrip QLabel#volIndicator { padding: 0 4px; }"
        "#commandStrip QPushButton#volFreeBtn {"
        "  min-width: 72px; padding: 2px 8px; border: 1px solid #b0b0b0;"
        "  border-radius: 3px; background: #ffffff; }"
        "#commandStrip QPushButton#volFreeBtn:hover:enabled { background: #f0f7ff; border-color: #7eb8ff; }"
        "#commandStrip QPushButton#volFreeBtn:pressed:enabled { background: #dcecff; }"
        "#commandStrip QPushButton#volFreeBtn:disabled { color: #888888; background: #f7f7f7; }"));

    auto* stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(8, 5, 8, 5);
    stripLayout->setSpacing(4);

    m_openBtn = makeToolbarButton(strip, ui::toolbarOpenIcon(), QStringLiteral("Open a folder"));
    m_upBtn = makeToolbarButton(strip, ui::toolbarUpIcon(), QStringLiteral("Go up"));
    m_exploreBtn = makeToolbarButton(strip, ui::toolbarManageIcon(), QStringLiteral("Open in file manager"));
    m_scanBtn = makeToolbarButton(strip, ui::toolbarUpdateIcon(), QStringLiteral("Update the folder data"));
    stripLayout->addWidget(m_openBtn);
    stripLayout->addWidget(m_upBtn);
    stripLayout->addWidget(m_exploreBtn);
    stripLayout->addWidget(m_scanBtn);

    connect(m_openBtn, &QToolButton::clicked, this, &MainWindow::onOpenFolder);
    connect(m_upBtn, &QToolButton::clicked, this, &MainWindow::onUp);
    connect(m_exploreBtn, &QToolButton::clicked, this, &MainWindow::onExplore);
    connect(m_scanBtn, &QToolButton::clicked, this, &MainWindow::onUpdateOrStop);

    auto* separator = new QFrame(strip);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setFixedHeight(26);
    stripLayout->addSpacing(6);
    stripLayout->addWidget(separator);
    stripLayout->addSpacing(6);

    m_totalLabel = new QLabel(QStringLiteral("Total at -: -"), strip);
    m_totalLabel->setObjectName(QStringLiteral("volIndicator"));
    m_totalLabel->setToolTip(QStringLiteral("Total capacity of the volume"));
    stripLayout->addWidget(m_totalLabel);

    auto* freeGroup = new QWidget(strip);
    auto* freeLayout = new QHBoxLayout(freeGroup);
    freeLayout->setContentsMargins(0, 0, 0, 0);
    freeLayout->setSpacing(4);
    m_freeLabel = new QLabel(QStringLiteral("Free at -:"), freeGroup);
    m_freeLabel->setObjectName(QStringLiteral("volIndicator"));
    m_freeLabel->setToolTip(QStringLiteral("Free space on the volume"));
    m_freeBtn = new QPushButton(QStringLiteral("-"), freeGroup);
    m_freeBtn->setObjectName(QStringLiteral("volFreeBtn"));
    m_freeBtn->setToolTip(QStringLiteral("Free space on the volume. Click to refresh."));
    m_freeBtn->setEnabled(false);
    freeLayout->addWidget(m_freeLabel);
    freeLayout->addWidget(m_freeBtn);
    stripLayout->addWidget(freeGroup);

    connect(m_freeBtn, &QPushButton::clicked, this, &MainWindow::onRefreshFree);

    stripLayout->addStretch();

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
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    m_openAct = fileMenu->addAction(ui::openFolderMenuLabel(), this, &MainWindow::onOpenFolder);
    m_openAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    fileMenu->addSeparator();
    m_exitAct = fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close);
    m_exitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));

    auto* inspectMenu = menuBar()->addMenu(QStringLiteral("&Inspect"));
    m_upAct = inspectMenu->addAction(QStringLiteral("&Up"), this, &MainWindow::onUp);
    m_upAct->setShortcut(QKeySequence(Qt::Key_Backspace));
    m_exploreAct = inspectMenu->addAction(ui::exploreMenuLabel(), this, &MainWindow::onExplore);
    m_exploreAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    inspectMenu->addSeparator();
    m_updateAct = inspectMenu->addAction(QStringLiteral("&Update"), this, &MainWindow::onUpdateOrStop);
    m_updateAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    m_stopAct = inspectMenu->addAction(QStringLiteral("&Stop"), this, &MainWindow::onUpdateOrStop);
    m_stopAct->setShortcut(QKeySequence(Qt::Key_Escape));

    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
    m_settingsAct = toolsMenu->addAction(ui::settingsMenuLabel(), this, &MainWindow::onSettings);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    m_aboutAct = helpMenu->addAction(ui::aboutMenuLabel(), this, &MainWindow::onAbout);
}

void MainWindow::setStatusText(const QString& text) { statusBar()->showMessage(text); }

QString MainWindow::statusForContext() const {
    if (!m_session.treemapComplete) {
        return QStringLiteral("Choose a target folder");
    }
    return m_session.contextPath.isEmpty() ? m_session.targetPath : m_session.contextPath;
}

void MainWindow::refreshVolumeToolbar() {
    if (m_session.targetPath.isEmpty() || m_session.volBarRoot.isEmpty()) {
        m_totalLabel->setText(QStringLiteral("Total at -: -"));
        m_freeLabel->setText(QStringLiteral("Free at -:"));
        m_freeBtn->setText(QStringLiteral("-"));
        m_freeBtn->setEnabled(false);
        return;
    }

    const QString label = normalizeDriveIndicatorToken(m_session.volLabel);
    const quint64 total = m_session.driveTotal;
    const quint64 free = m_session.driveFree;

    if (total == 0) {
        m_totalLabel->setText(QStringLiteral("Total at %1: -").arg(label));
        m_freeLabel->setText(QStringLiteral("Free at %1:").arg(label));
        m_freeBtn->setText(QStringLiteral("-"));
    } else {
        m_totalLabel->setText(QStringLiteral("Total at %1: %2")
                                  .arg(label, util::formatObjectSize(static_cast<qint64>(total))));
        m_freeLabel->setText(QStringLiteral("Free at %1:").arg(label));
        m_freeBtn->setText(util::formatObjectSize(static_cast<qint64>(free)));
    }
    m_freeBtn->setEnabled(!m_session.scanning);
}

void MainWindow::updateChrome() {
    const bool scanning = m_session.scanning;
    const bool openScanning = scanning && m_session.scanKind == ScanKind::OpenTarget;
    const bool updateScanning = scanning && m_session.scanKind == ScanKind::UpdateContext;
    const bool hasTarget = !m_session.targetPath.isEmpty();
    const bool complete = m_session.treemapComplete;

    m_openAct->setEnabled(!scanning);
    m_openBtn->setEnabled(!scanning);
    m_exitAct->setEnabled(!scanning);
    m_upAct->setEnabled((!openScanning && m_session.canGoUp()) || (updateScanning && m_session.canGoUp()));
    m_upBtn->setEnabled(m_upAct->isEnabled());
    m_exploreAct->setEnabled(complete && !openScanning);
    m_exploreBtn->setEnabled(m_exploreAct->isEnabled());
    m_updateAct->setVisible(!scanning);
    m_stopAct->setVisible(scanning);
    m_updateAct->setEnabled(!scanning && hasTarget && complete);
    m_scanBtn->setEnabled(scanning || m_updateAct->isEnabled());
    m_scanBtn->setIcon(scanning ? ui::toolbarStopIcon() : ui::toolbarUpdateIcon());
    m_scanBtn->setToolTip(scanning ? QStringLiteral("Stop scanning folders")
                                   : QStringLiteral("Update the folder data"));
    m_settingsAct->setEnabled(!scanning);
    m_aboutAct->setEnabled(!scanning);
    m_freeBtn->setEnabled(!scanning && !m_session.targetPath.isEmpty());

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
    if ((m_session.scanKind == ScanKind::OpenTarget && m_session.scanning) || !m_session.canGoUp()) {
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
    if (m_session.targetPath.isEmpty() || m_session.volBarRoot.isEmpty()) {
        return;
    }
    quint64 total = 0;
    quint64 free = 0;
    if (platform::diskSpace(m_session.volBarRoot, &total, &free)) {
        m_session.driveTotal = total;
        m_session.driveFree = free;
        model::annotateShares(m_session.publishedTree, m_session.driveTotal);
        refreshVolumeToolbar();
        rebuildTreemap();
    }
}

void MainWindow::onDive(const QString& folderPath) {
    if (m_session.scanKind == ScanKind::OpenTarget && m_session.scanning) {
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

    m_activeScanIdentity = {scanId, m_session.sessionId, m_session.descriptorVersion};
    m_scanThread = new QThread(this);
    m_scanWorker = new scan::ScanWorker(m_session.scanRootPath, m_activeScanIdentity);
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

void MainWindow::onScanFinished(scan::ScanResult result) {
    if (result.identity().scanId != m_activeScanIdentity.scanId ||
        result.identity().targetSessionId != m_activeScanIdentity.targetSessionId ||
        result.identity().baseDescriptorVersion != m_activeScanIdentity.baseDescriptorVersion) {
        return;
    }

    const ScanKind kind = m_session.scanKind;
    const QString scanRoot = m_session.scanRootPath;
    const QString expectedTarget = m_session.targetPath;

    m_session.scanning = false;
    updateChrome();

    switch (result.outcome()) {
    case scan::ScanOutcome::TechnicalFailure:
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
        } else {
            unsetTreemap();
        }
        ui::showError002(this);
        m_session.pendingUpdateSnapshot.reset();
        return;
    case scan::ScanOutcome::Cancelled:
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
            ui::showInterruptionAlert(this);
        } else {
            unsetTreemap();
            ui::showInterruptionAlert(this);
        }
        m_session.pendingUpdateSnapshot.reset();
        return;
    case scan::ScanOutcome::RootUnavailable:
        if (kind == ScanKind::UpdateContext && m_session.pendingUpdateSnapshot) {
            restorePendingUpdate();
            ui::showError001(this);
        } else {
            unsetTreemap();
            ui::showError001(this);
        }
        m_session.pendingUpdateSnapshot.reset();
        return;
    case scan::ScanOutcome::Success:
        break;
    default:
        return;
    }

    if (!result.tree()) {
        return;
    }

    model::FolderDescriptor tree = *result.tree();
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
        ++m_session.descriptorVersion;
        m_session.pendingUpdateSnapshot.reset();
        rebuildTreemap();
        setStatusText(statusForContext());
        return;
    }

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
    ++m_session.descriptorVersion;
    m_session.pendingUpdateSnapshot.reset();
    rebuildTreemap();
    setStatusText(statusForContext());
}

}  // namespace wtw::app
