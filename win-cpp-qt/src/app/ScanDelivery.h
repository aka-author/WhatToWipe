#pragma once

#include "app/Session.h"
#include "scan/ScanResult.h"

#include <QString>
#include <vector>

namespace wtw::app {

struct ScanProgressState {
    QString latestProgressPath;
    qint64 lastProgressEmitMs = 0;
};

struct ScanProgressApply {
    bool accepted = false;
    QString statusText;
};

struct SessionDeliverySnapshot {
    bool scanning = false;
    quint64 scanId = 0;
    quint64 sessionId = 0;
    quint64 descriptorVersion = 0;
    bool treemapComplete = false;
    QString contextPath;
    QString publishedTreeName;
    bool hasPendingUpdate = false;
    QString pendingContextPath;
    QString latestProgressPath;
    qint64 lastProgressEmitMs = 0;
};

enum class ScanFinishUiAction {
    Error001,
    Error002,
    Error004,
    InterruptionAlert,
    RebuildTreemap,
    StatusForContext,
};

struct ScanFinishApply {
    bool accepted = false;
    std::vector<ScanFinishUiAction> uiActions;
    QString error004Target;
};

SessionDeliverySnapshot captureDeliverySnapshot(const Session& session, const ScanProgressState& progress);

ScanProgressApply applyScanProgressIfCurrent(Session& session, ScanProgressState& progress,
                                             const scan::ScanIdentity& identity, const QString& path,
                                             qint64 nowMs);

ScanFinishApply applyScanFinishedIfCurrent(Session& session, const scan::ScanResult& result);

}  // namespace wtw::app
