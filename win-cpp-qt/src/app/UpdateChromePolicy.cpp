#include "app/UpdateChromePolicy.h"

namespace wtw::app {

ChromeAvailability computeChromeAvailability(const Session& session) {
    ChromeAvailability chrome;
    const bool scanning = session.scanning;
    const bool openScanning = scanning && session.scanKind == ScanKind::OpenTarget;
    const bool updateScanning = scanning && session.scanKind == ScanKind::UpdateContext;
    const bool hasTarget = !session.targetPath.isEmpty();
    const bool complete = session.treemapComplete;

    chrome.open = !scanning;
    chrome.update = !scanning && hasTarget && complete;
    chrome.stop = scanning;
    chrome.up = canNavigateUp(session);
    chrome.dive = canNavigateDive(session);
    chrome.explore = complete && !openScanning;
    chrome.settings = !scanning;
    return chrome;
}

bool canNavigateUp(const Session& session) {
    if (!session.canGoUp()) {
        return false;
    }
    if (session.scanning && session.scanKind == ScanKind::OpenTarget) {
        return false;
    }
    return true;
}

bool canNavigateDive(const Session& session) {
    if (!session.treemapComplete) {
        return false;
    }
    if (session.scanning && session.scanKind == ScanKind::OpenTarget) {
        return false;
    }
    return true;
}

}  // namespace wtw::app
