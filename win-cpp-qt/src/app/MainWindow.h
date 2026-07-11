#pragma once

#include "app/Session.h"
#include "config/TreemapConfig.h"
#include "scan/ScanResult.h"
#include <QMainWindow>

class QToolButton;
class QLabel;
class QPushButton;
class QThread;

namespace wtw::scan {
class ScanWorker;
}

namespace wtw::treemap {
class TreemapWidget;
}

namespace wtw::app {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const config::TreemapSettings& settings, QWidget* parent = nullptr);

private slots:
    void onOpenFolder();
    void onUp();
    void onExplore();
    void onUpdateOrStop();
    void onSettings();
    void onAbout();
    void onRefreshFree();
    void onScanProgress(wtw::scan::ScanIdentity identity, const QString& path);
    void onScanFinished(wtw::scan::ScanResult result);
    void onDive(const QString& folderPath);
    void onExploreTile(const QString& path);
    void onOpenFile(const QString& path);

private:
    void buildUi();
    void buildMenus();
    void updateChrome();
    void rebuildTreemap();
    void applyResetTreemapUi();
    void setStatusText(const QString& text);
    void refreshVolumeToolbar();
    void startScan(const QString& scanRoot, ScanKind kind);
    void restorePendingUpdate();
    void unsetTreemap();
    void busyPointerTwoSeconds();
    QString statusForContext() const;

    Session m_session;
    config::TreemapSettings m_cfg;
    treemap::TreemapWidget* m_treemap = nullptr;
    QLabel* m_totalLabel = nullptr;
    QLabel* m_freeLabel = nullptr;
    QPushButton* m_freeBtn = nullptr;

    QAction* m_openAct = nullptr;
    QAction* m_exitAct = nullptr;
    QAction* m_upAct = nullptr;
    QAction* m_exploreAct = nullptr;
    QAction* m_updateAct = nullptr;
    QAction* m_stopAct = nullptr;
    QAction* m_settingsAct = nullptr;
    QAction* m_aboutAct = nullptr;

    QToolButton* m_openBtn = nullptr;
    QToolButton* m_upBtn = nullptr;
    QToolButton* m_exploreBtn = nullptr;
    QToolButton* m_scanBtn = nullptr;

    QThread* m_scanThread = nullptr;
    scan::ScanWorker* m_scanWorker = nullptr;
    QString m_latestProgressPath;
    qint64 m_lastProgressEmitMs = 0;
};

}  // namespace wtw::app
