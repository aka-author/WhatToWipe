#pragma once

#include "app/ScanSessionGate.h"
#include "app/Session.h"
#include "scan/ScanResult.h"

namespace wtw::app {

// Mirrors the first mutation in MainWindow::onScanFinished after identity validation.
inline bool applyScanFinishedIfCurrent(Session& session, const scan::ScanResult& result) {
    if (!acceptsScanDelivery(result.identity(), session)) {
        return false;
    }
    session.scanning = false;
    return true;
}

}  // namespace wtw::app
