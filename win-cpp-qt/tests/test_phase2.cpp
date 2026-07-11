#include "app/ScanDelivery.h"
#include "app/Session.h"
#include "app/UpdateChromePolicy.h"
#include "app/UpdatePublish.h"
#include "model/FolderDescriptor.h"
#include "scan/ScanResult.h"
#include "scan/SubtreeMerge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace wtw;

namespace {

model::FolderDescriptor makeFolder(const QString& path, std::vector<model::FolderDescriptor> children = {}) {
    model::FolderDescriptor folder;
    folder.fullPath = QDir::cleanPath(path);
    folder.name = QFileInfo(path).fileName();
    if (folder.name.isEmpty()) {
        folder.name = folder.fullPath;
    }
    folder.traversalState = scan::TraversalState::Complete;
    folder.children = std::move(children);
    scan::recomputeAggregates(folder);
    return folder;
}

app::Session updateScanSession() {
    app::Session session;
    session.targetPath = QStringLiteral("C:/target");
    session.contextPath = QStringLiteral("C:/target/sub");
    session.treemapComplete = true;
    session.scanning = true;
    session.scanKind = app::ScanKind::UpdateContext;
    session.scanId = 3;
    session.sessionId = 2;
    session.descriptorVersion = 4;
    session.scanRootPath = QStringLiteral("C:/target/sub");
    session.publishedTree = makeFolder(QStringLiteral("C:/target"),
                                       {makeFolder(QStringLiteral("C:/target/sub"))});
    app::UpdateSnapshot snap;
    snap.contextPath = QStringLiteral("C:/target/sub");
    snap.tree = session.publishedTree;
    session.pendingUpdateSnapshot = snap;
    return session;
}

void assertUiActions(const app::ScanFinishApply& apply,
                     std::initializer_list<app::ScanFinishUiAction> expected) {
    QCOMPARE(apply.uiActions.size(), expected.size());
    size_t index = 0;
    for (app::ScanFinishUiAction action : expected) {
        QCOMPARE(apply.uiActions.at(index++), action);
    }
}

}  // namespace

class Phase2Tests : public QObject {
    Q_OBJECT

private slots:
    void update_allows_up_during_scan() {
        app::Session session = updateScanSession();
        QCOMPARE(session.contextPath, QStringLiteral("C:/target/sub"));
        QVERIFY(app::canNavigateUp(session));
        QVERIFY(app::computeChromeAvailability(session).up);
        session.goUp();
        QCOMPARE(session.contextPath, QStringLiteral("C:/target"));
        QVERIFY(session.resolveContextFolder() != nullptr);
        QVERIFY(!session.canGoUp());
    }

    void update_blocks_open() {
        app::Session session = updateScanSession();
        const app::ChromeAvailability chrome = app::computeChromeAvailability(session);
        QVERIFY(!chrome.open);
        QVERIFY(!chrome.update);
        QVERIFY(chrome.stop);
    }

    void update_allows_dive_and_explore_during_scan() {
        app::Session session = updateScanSession();
        const app::ChromeAvailability chrome = app::computeChromeAvailability(session);
        QVERIFY(chrome.dive);
        QVERIFY(chrome.explore);
        QVERIFY(app::canNavigateDive(session));
    }

    void open_scan_blocks_up_and_dive() {
        app::Session session;
        session.scanning = true;
        session.scanKind = app::ScanKind::OpenTarget;
        session.treemapComplete = false;
        session.targetPath = QStringLiteral("C:/target");
        session.contextPath = QStringLiteral("C:/target/sub");
        session.publishedTree = makeFolder(QStringLiteral("C:/target"),
                                           {makeFolder(QStringLiteral("C:/target/sub"))});

        const app::ChromeAvailability chrome = app::computeChromeAvailability(session);
        QVERIFY(!chrome.up);
        QVERIFY(!chrome.dive);
        QVERIFY(!chrome.explore);
    }

    void update_publish_preserves_context() {
        app::Session session = updateScanSession();
        session.contextPath = QStringLiteral("C:/target/sub");

        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        model::FileDescriptor file;
        file.name = QStringLiteral("fresh.dat");
        file.size = 42;
        refreshed.files.push_back(file);
        scan::recomputeAggregates(refreshed);

        app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, session.scanRootPath, refreshed, session.contextPath,
            session.targetPath, session.descriptorVersion, session.driveTotal);
        QCOMPARE(prepared.status, app::UpdatePublicationStatus::Ready);
        QCOMPARE(prepared.contextPath, QStringLiteral("C:/target/sub"));
        QCOMPARE(prepared.descriptorVersion, static_cast<quint64>(5));

        app::publishPreparedUpdate(session, std::move(prepared));
        QVERIFY(session.resolveContextFolder() != nullptr);
        QCOMPARE(session.publishedTree.children.front().files.size(), static_cast<size_t>(1));
        QVERIFY(!session.pendingUpdateSnapshot.has_value());
    }

    void update_publish_uses_live_context_path() {
        app::Session session = updateScanSession();
        session.contextPath = QStringLiteral("C:/target");

        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, session.scanRootPath, refreshed, session.contextPath,
            session.targetPath, session.descriptorVersion, session.driveTotal);
        QCOMPARE(prepared.status, app::UpdatePublicationStatus::Ready);
        QCOMPARE(prepared.contextPath, QStringLiteral("C:/target"));
        app::publishPreparedUpdate(session, std::move(prepared));
        QCOMPARE(session.contextPath, QStringLiteral("C:/target"));
    }

    void update_context_deleted_shows_004() {
        app::Session session = updateScanSession();
        session.contextPath = QStringLiteral("C:/target/missing");

        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        const app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, session.scanRootPath, refreshed, session.contextPath,
            session.targetPath, session.descriptorVersion, session.driveTotal);
        QCOMPARE(prepared.status, app::UpdatePublicationStatus::ContextMissing);
        QCOMPARE(prepared.error004Target, QStringLiteral("C:/target/missing"));
    }

    void update_merge_missing_scan_root() {
        app::Session session = updateScanSession();
        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/elsewhere"));
        const app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, QStringLiteral("C:/nowhere"), refreshed, session.contextPath,
            session.targetPath, session.descriptorVersion, session.driveTotal);
        QCOMPARE(prepared.status, app::UpdatePublicationStatus::MergeFailed);
    }

    void update_descriptor_version_once() {
        app::Session session = updateScanSession();
        const quint64 before = session.descriptorVersion;
        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, session.scanRootPath, refreshed, session.contextPath,
            session.targetPath, before, session.driveTotal);
        QCOMPARE(prepared.descriptorVersion, before + 1);
        app::publishPreparedUpdate(session, std::move(prepared));
        QCOMPARE(session.descriptorVersion, before + 1);
    }

    void update_cancel_after_navigate_up_preserves_context() {
        app::Session session = updateScanSession();
        session.goUp();
        QCOMPARE(session.contextPath, QStringLiteral("C:/target"));

        const app::ScanFinishApply apply =
            app::applyScanFinishedIfCurrent(session, scan::ScanResult::cancelled({3, 2, 4}));
        QVERIFY(apply.accepted);
        QCOMPARE(session.contextPath, QStringLiteral("C:/target"));
        QVERIFY(session.resolveContextFolder() != nullptr);
        QVERIFY(!session.pendingUpdateSnapshot.has_value());
    }

    void update_technical_failure_after_navigate_sibling_preserves_context() {
        app::Session session = updateScanSession();
        session.publishedTree =
            makeFolder(QStringLiteral("C:/target"),
                       {makeFolder(QStringLiteral("C:/target/sub")), makeFolder(QStringLiteral("C:/target/other"))});
        session.pendingUpdateSnapshot->tree = session.publishedTree;
        session.pushContext(QStringLiteral("C:/target/other"));
        QCOMPARE(session.contextPath, QStringLiteral("C:/target/other"));

        const app::ScanFinishApply apply =
            app::applyScanFinishedIfCurrent(session, scan::ScanResult::technicalFailure({3, 2, 4}));
        QVERIFY(apply.accepted);
        QCOMPARE(session.contextPath, QStringLiteral("C:/target/other"));
        QVERIFY(session.resolveContextFolder() != nullptr);
    }

    void update_restore_falls_back_when_live_context_missing() {
        app::Session session = updateScanSession();
        session.contextPath = QStringLiteral("C:/target/missing");

        restorePendingUpdateSession(session);
        QCOMPARE(session.contextPath, QStringLiteral("C:/target/sub"));
        QVERIFY(session.resolveContextFolder() != nullptr);
    }

    void update_publish_sets_consistent_final_state() {
        app::Session session = updateScanSession();
        const QString expectedContext = session.contextPath;
        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        app::PreparedUpdatePublication prepared = app::prepareUpdatePublication(
            *session.pendingUpdateSnapshot, session.scanRootPath, refreshed, session.contextPath,
            session.targetPath, session.descriptorVersion, session.driveTotal);
        QVERIFY(session.pendingUpdateSnapshot.has_value());
        app::publishPreparedUpdate(session, std::move(prepared));
        QVERIFY(!session.pendingUpdateSnapshot.has_value());
        QVERIFY(session.treemapComplete);
        QCOMPARE(session.contextPath, expectedContext);
        QCOMPARE(session.descriptorVersion, static_cast<quint64>(5));
        QVERIFY(session.resolveContextFolder() != nullptr);
    }

    void update_success_delivery_ui_contract() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QVERIFY(QDir().mkpath(temp.filePath(QStringLiteral("sub"))));
        const QString root = QDir::cleanPath(temp.path());
        const QString sub = QDir::cleanPath(temp.filePath(QStringLiteral("sub")));

        app::Session session;
        session.targetPath = root;
        session.contextPath = sub;
        session.treemapComplete = true;
        session.scanning = true;
        session.scanKind = app::ScanKind::UpdateContext;
        session.scanId = 3;
        session.sessionId = 2;
        session.descriptorVersion = 4;
        session.scanRootPath = sub;
        session.publishedTree = makeFolder(root, {makeFolder(sub)});
        app::UpdateSnapshot snap;
        snap.contextPath = sub;
        snap.tree = session.publishedTree;
        session.pendingUpdateSnapshot = snap;

        model::FolderDescriptor refreshed = makeFolder(sub);
        model::FileDescriptor file;
        file.name = QStringLiteral("fresh.dat");
        file.size = 42;
        refreshed.files.push_back(file);
        scan::recomputeAggregates(refreshed);

        const scan::ScanResult result = scan::ScanResult::success({3, 2, 4}, std::move(refreshed));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);
        QVERIFY(apply.accepted);
        assertUiActions(apply, {app::ScanFinishUiAction::RebuildTreemap, app::ScanFinishUiAction::StatusForContext});
        QVERIFY(session.resolveContextFolder() != nullptr);
        QCOMPARE(session.publishedTree.children.front().files.size(), static_cast<size_t>(1));
    }

    void update_context_missing_delivery_restores_snapshot() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QVERIFY(QDir().mkpath(temp.filePath(QStringLiteral("sub"))));
        const QString root = QDir::cleanPath(temp.path());
        const QString sub = QDir::cleanPath(temp.filePath(QStringLiteral("sub")));
        const QString missing = QDir::cleanPath(temp.filePath(QStringLiteral("missing")));

        app::Session session;
        session.targetPath = root;
        session.contextPath = missing;
        session.treemapComplete = true;
        session.scanning = true;
        session.scanKind = app::ScanKind::UpdateContext;
        session.scanId = 3;
        session.sessionId = 2;
        session.descriptorVersion = 4;
        session.scanRootPath = sub;
        session.publishedTree = makeFolder(root, {makeFolder(sub)});
        app::UpdateSnapshot snap;
        snap.contextPath = sub;
        snap.tree = session.publishedTree;
        session.pendingUpdateSnapshot = snap;

        model::FolderDescriptor refreshed = makeFolder(sub);
        const scan::ScanResult result = scan::ScanResult::success({3, 2, 4}, std::move(refreshed));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);
        QVERIFY(apply.accepted);
        assertUiActions(apply, {app::ScanFinishUiAction::RebuildTreemap, app::ScanFinishUiAction::StatusForContext,
                                app::ScanFinishUiAction::Error004});
        QCOMPARE(apply.error004Target, missing);
        QCOMPARE(session.contextPath, sub);
        QCOMPARE(session.publishedTree.children.size(), static_cast<size_t>(1));
        QVERIFY(!session.pendingUpdateSnapshot.has_value());
    }

    void stale_update_result_inert() {
        app::Session session = updateScanSession();
        app::ScanProgressState progress;
        progress.latestProgressPath = QStringLiteral("scanning/sub");
        const auto before = app::captureDeliverySnapshot(session, progress);

        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        const scan::ScanResult result = scan::ScanResult::success({2, 2, 4}, std::move(refreshed));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);
        QVERIFY(!apply.accepted);
        QCOMPARE(app::captureDeliverySnapshot(session, progress).publishedTreeName, before.publishedTreeName);
        QVERIFY(session.scanning);
        QVERIFY(session.pendingUpdateSnapshot.has_value());
    }

    void stale_update_result_ignored() {
        app::Session session = updateScanSession();
        model::FolderDescriptor refreshed = makeFolder(QStringLiteral("C:/target/sub"));
        const scan::ScanResult result = scan::ScanResult::success({3, 99, 4}, std::move(refreshed));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);
        QVERIFY(!apply.accepted);
        QVERIFY(session.pendingUpdateSnapshot.has_value());
        QCOMPARE(session.descriptorVersion, static_cast<quint64>(4));
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    qRegisterMetaType<scan::ScanResult>("wtw::scan::ScanResult");
    qRegisterMetaType<scan::ScanIdentity>("wtw::scan::ScanIdentity");
    Phase2Tests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_phase2.moc"
