#include "app/ScanDelivery.h"

#include "app/ScanSessionGate.h"
#include "app/UpdatePublish.h"

#include <QDir>
#include <QFileInfo>

namespace wtw::app {

namespace {

void restorePendingUpdateSession(Session& session) {
    if (!session.pendingUpdateSnapshot) {
        return;
    }
    session.publishedTree = cloneFolder(session.pendingUpdateSnapshot->tree);
    session.contextPath = session.pendingUpdateSnapshot->contextPath;
    session.treemapComplete = true;
}

void unsetTreemapSession(Session& session) { session.resetToInitial(); }

void appendUnsetTreemap(Session& session, ScanFinishApply& out) {
    unsetTreemapSession(session);
    out.uiActions.push_back(ScanFinishUiAction::ResetTreemapUi);
}

}  // namespace

SessionDeliverySnapshot captureDeliverySnapshot(const Session& session, const ScanProgressState& progress) {
    SessionDeliverySnapshot snap;
    snap.scanning = session.scanning;
    snap.scanId = session.scanId;
    snap.sessionId = session.sessionId;
    snap.descriptorVersion = session.descriptorVersion;
    snap.treemapComplete = session.treemapComplete;
    snap.contextPath = session.contextPath;
    snap.publishedTreeName = session.publishedTree.name;
    snap.hasPendingUpdate = session.pendingUpdateSnapshot.has_value();
    if (session.pendingUpdateSnapshot) {
        snap.pendingContextPath = session.pendingUpdateSnapshot->contextPath;
    }
    snap.latestProgressPath = progress.latestProgressPath;
    snap.lastProgressEmitMs = progress.lastProgressEmitMs;
    return snap;
}

ScanProgressApply applyScanProgressIfCurrent(Session& session, ScanProgressState& progress,
                                             const scan::ScanIdentity& identity, const QString& path,
                                             qint64 nowMs) {
    ScanProgressApply out;
    if (!acceptsScanDelivery(identity, session)) {
        return out;
    }
    out.accepted = true;
    progress.latestProgressPath = path;
    if (progress.lastProgressEmitMs != 0 && nowMs - progress.lastProgressEmitMs < 500) {
        return out;
    }
    progress.lastProgressEmitMs = nowMs;
    out.statusText = path;
    return out;
}

ScanFinishApply applyScanFinishedIfCurrent(Session& session, const scan::ScanResult& result) {
    ScanFinishApply out;
    if (!acceptsScanDelivery(result.identity(), session)) {
        return out;
    }
    out.accepted = true;

    const ScanKind kind = session.scanKind;
    const QString scanRoot = session.scanRootPath;
    const QString expectedTarget = session.targetPath;

    session.scanning = false;

    switch (result.outcome()) {
    case scan::ScanOutcome::TechnicalFailure:
        if (kind == ScanKind::UpdateContext && session.pendingUpdateSnapshot) {
            restorePendingUpdateSession(session);
            out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
            out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
        } else {
            appendUnsetTreemap(session, out);
        }
        session.pendingUpdateSnapshot.reset();
        out.uiActions.push_back(ScanFinishUiAction::Error002);
        return out;
    case scan::ScanOutcome::Cancelled:
        if (kind == ScanKind::UpdateContext && session.pendingUpdateSnapshot) {
            restorePendingUpdateSession(session);
            out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
            out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
            out.uiActions.push_back(ScanFinishUiAction::InterruptionAlert);
        } else {
            appendUnsetTreemap(session, out);
            out.uiActions.push_back(ScanFinishUiAction::InterruptionAlert);
        }
        session.pendingUpdateSnapshot.reset();
        return out;
    case scan::ScanOutcome::RootUnavailable:
        if (kind == ScanKind::UpdateContext && session.pendingUpdateSnapshot) {
            restorePendingUpdateSession(session);
            out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
            out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
            out.uiActions.push_back(ScanFinishUiAction::Error001);
        } else {
            appendUnsetTreemap(session, out);
            out.uiActions.push_back(ScanFinishUiAction::Error001);
        }
        session.pendingUpdateSnapshot.reset();
        return out;
    case scan::ScanOutcome::Success:
        break;
    default:
        return out;
    }

    if (!result.tree()) {
        return out;
    }

    model::FolderDescriptor tree = *result.tree();
    model::annotateShares(tree, session.driveTotal);

    if (kind == ScanKind::OpenTarget) {
        if (!expectedTarget.isEmpty()) {
            const QFileInfo targetFi(expectedTarget);
            if (!targetFi.exists()) {
                out.error004Target = expectedTarget;
                appendUnsetTreemap(session, out);
                out.uiActions.push_back(ScanFinishUiAction::Error004);
                return out;
            }
        }
        session.publishedTree = std::move(tree);
        session.contextPath = scanRoot;
        session.treemapComplete = true;
        ++session.descriptorVersion;
        session.pendingUpdateSnapshot.reset();
        out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
        out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
        return out;
    }

    if (!session.pendingUpdateSnapshot) {
        return out;
    }
    const QFileInfo ctxFi(scanRoot);
    if (!ctxFi.exists()) {
        appendUnsetTreemap(session, out);
        out.error004Target = scanRoot;
        session.pendingUpdateSnapshot.reset();
        out.uiActions.push_back(ScanFinishUiAction::Error004);
        return out;
    }

    const QString liveContext = session.contextPath.isEmpty() ? session.targetPath : session.contextPath;
    PreparedUpdatePublication prepared = prepareUpdatePublication(
        *session.pendingUpdateSnapshot, scanRoot, tree, liveContext, session.targetPath,
        session.descriptorVersion, session.driveTotal);
    if (prepared.status == UpdatePublicationStatus::MergeFailed) {
        restorePendingUpdateSession(session);
        session.pendingUpdateSnapshot.reset();
        out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
        out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
        out.uiActions.push_back(ScanFinishUiAction::Error002);
        return out;
    }
    if (prepared.status == UpdatePublicationStatus::ContextMissing) {
        restorePendingUpdateSession(session);
        session.pendingUpdateSnapshot.reset();
        out.error004Target = prepared.error004Target;
        out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
        out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
        out.uiActions.push_back(ScanFinishUiAction::Error004);
        return out;
    }
    publishPreparedUpdate(session, std::move(prepared));
    out.uiActions.push_back(ScanFinishUiAction::RebuildTreemap);
    out.uiActions.push_back(ScanFinishUiAction::StatusForContext);
    return out;
}

}  // namespace wtw::app
