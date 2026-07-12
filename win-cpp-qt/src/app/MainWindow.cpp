#include "app/MainWindow.h"

#include "app/Product.h"
#include "app/ScanDelivery.h"
#include "app/UpdateChromePolicy.h"
#include "app/UpdatePublish.h"

#include "config/ConfigStore.h"
#include "model/FolderDescriptor.h"
#include "platform/ShellOpen.h"
#include "platform/VolumeInfo.h"
#include "platform/WinWindowIcon.h"
#include "scan/ScanResult.h"
#include "scan/ScanWorker.h"
#include "treemap/TreemapLayout.h"
#include "treemap/TreemapProjection.h"
#include "treemap/TreemapWidget.h"
#include "ui/AboutDialog.h"
#include "ui/AlertDialogs.h"
#include "ui/AppIcon.h"
#include "ui/MenuLabels.h"
#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcons.h"
#include "util/Format.h"

#include <QSizePolicy>
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
#include <QShowEvent>
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
    qRegisterMetaType<scan::ScanIdentity>("wtw::scan::ScanIdentity");
    setWindowTitle(productDisplayName());
    setWindowIcon(ui::appWindowIcon());
    resize(1100, 720);
    buildUi();
    buildMenus();
    m_session.resetToInitial();
    setStatusText(QStringLiteral("Choose a target folder"));
    updateChrome();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!m_winIconsApplied) {
        platform::applyWin32WindowIcons(this);
        m_winIconsApplied = true;
    }
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
    m_treemap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_treemap, &treemap::TreemapWidget::diveRequested, this, &MainWindow::onDive);
    connect(m_treemap, &treemap::TreemapWidget::exploreRequested, this, &MainWindow::onExploreTile);
    connect(m_treemap, &treemap::TreemapWidget::openFileRequested, this, &MainWindow::onOpenFile);
    connect(m_treemap, &treemap::TreemapWidget::layoutAreaChanged, this, &MainWindow::rebuildTreemap);

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
    const QString path = m_session.contextPath.isEmpty() ? m_session.targetPath : m_session.contextPath;
    return util::formatPathForStatusBar(path);
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
    const ChromeAvailability chrome = computeChromeAvailability(m_session);

    m_openAct->setEnabled(chrome.open);
    m_openBtn->setEnabled(chrome.open);
    m_exitAct->setEnabled(chrome.open);
    m_upAct->setEnabled(chrome.up);
    m_upBtn->setEnabled(chrome.up);
    m_exploreAct->setEnabled(chrome.explore);
    m_exploreBtn->setEnabled(chrome.explore);
    m_updateAct->setVisible(!chrome.stop);
    m_stopAct->setVisible(chrome.stop);
    m_updateAct->setEnabled(chrome.update);
    m_scanBtn->setEnabled(chrome.stop || chrome.update);
    m_scanBtn->setIcon(chrome.stop ? ui::toolbarStopIcon() : ui::toolbarUpdateIcon());
    m_scanBtn->setToolTip(chrome.stop ? QStringLiteral("Stop scanning folders")
                                      : QStringLiteral("Update the folder data"));
    m_settingsAct->setEnabled(chrome.settings);
    m_aboutAct->setEnabled(chrome.settings);
    m_freeBtn->setEnabled(chrome.open && !m_session.targetPath.isEmpty());

    if (m_session.scanning) {
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
    std::vector<model::TreemapItem> items;
    try {
        items = treemap::buildTreemapItems(ctx, m_session.driveTotal, m_cfg);
    } catch (const std::exception&) {
        unsetTreemap();
        ui::showError002(this);
        return;
    }
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

void MainWindow::applyResetTreemapUi() {
    m_treemap->clearBlocks();
    refreshVolumeToolbar();
    setStatusText(QStringLiteral("Choose a target folder"));
}

void MainWindow::unsetTreemap() {
    m_session.resetToInitial();
    applyResetTreemapUi();
    updateChrome();
}

void MainWindow::restorePendingUpdate() {
    if (!m_session.pendingUpdateSnapshot) {
        return;
    }
    restorePendingUpdateSession(m_session);
    m_session.pendingUpdateSnapshot.reset();
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
    if (!canNavigateUp(m_session)) {
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
    if (!canNavigateDive(m_session)) {
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

    const scan::ScanIdentity workerIdentity{scanId, m_session.sessionId, m_session.descriptorVersion};
    QThread* thread = new QThread(this);
    scan::ScanWorker* worker = new scan::ScanWorker(m_session.scanRootPath, workerIdentity);
    m_scanThread = thread;
    m_scanWorker = worker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &scan::ScanWorker::run);
    connect(worker, &scan::ScanWorker::progress, this, &MainWindow::onScanProgress);
    connect(worker, &scan::ScanWorker::finished, this, &MainWindow::onScanFinished);
    connect(worker, &scan::ScanWorker::finished, thread, &QThread::quit);
    connect(worker, &scan::ScanWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread, worker]() {
        if (m_scanThread == thread) {
            m_scanThread = nullptr;
        }
        if (m_scanWorker == worker) {
            m_scanWorker = nullptr;
        }
    });

    thread->start();
}

void MainWindow::onScanProgress(scan::ScanIdentity identity, const QString& path) {
    ScanProgressState progress{m_latestProgressPath, m_lastProgressEmitMs};
    const ScanProgressApply apply =
        applyScanProgressIfCurrent(m_session, progress, identity, path, QDateTime::currentMSecsSinceEpoch());
    m_latestProgressPath = progress.latestProgressPath;
    m_lastProgressEmitMs = progress.lastProgressEmitMs;
    if (!apply.accepted || apply.statusText.isEmpty()) {
        return;
    }
    setStatusText(apply.statusText);
}

void MainWindow::onScanFinished(scan::ScanResult result) {
    const ScanFinishApply apply = applyScanFinishedIfCurrent(m_session, result);
    if (!apply.accepted) {
        return;
    }

    updateChrome();

    for (ScanFinishUiAction action : apply.uiActions) {
        switch (action) {
        case ScanFinishUiAction::Error001:
            ui::showError001(this);
            break;
        case ScanFinishUiAction::Error002:
            ui::showError002(this);
            break;
        case ScanFinishUiAction::Error004:
            ui::showError004(this, apply.error004Target);
            break;
        case ScanFinishUiAction::InterruptionAlert:
            ui::showInterruptionAlert(this);
            break;
        case ScanFinishUiAction::RebuildTreemap:
            rebuildTreemap();
            break;
        case ScanFinishUiAction::ResetTreemapUi:
            applyResetTreemapUi();
            break;
        case ScanFinishUiAction::StatusForContext:
            setStatusText(statusForContext());
            break;
        case ScanFinishUiAction::RefreshVolumeIndicators:
            refreshVolumeToolbar();
            break;
        }
    }
}

}  // namespace wtw::app
