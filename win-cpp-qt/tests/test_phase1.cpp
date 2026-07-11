#include "app/ScanDelivery.h"
#include "app/ScanSessionGate.h"
#include "app/Session.h"
#include "config/TreemapConfig.h"
#include "model/FolderDescriptor.h"
#include "platform/WinDirEnum.h"
#include "scan/ScanResult.h"
#include "scan/ScanWorker.h"
#include "scan/SubtreeMerge.h"
#include "treemap/TreemapProjection.h"
#include "util/Format.h"

#include "util/CheckedMath.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtTest>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

using namespace wtw;

namespace {

bool resetDirectoryAcl(const QString& path);

struct DenyReadAclGuard {
    QString path;
    bool active = false;

    ~DenyReadAclGuard() {
        if (active) {
            resetDirectoryAcl(path);
        }
    }
};

bool resetDirectoryAcl(const QString& path) {
    return QProcess::execute(QStringLiteral("icacls"),
                             {QDir::toNativeSeparators(path), QStringLiteral("/reset")}) == 0;
}

bool denyDirectoryRead(const QString& path) {
    const QString native = QDir::toNativeSeparators(path);
    if (QProcess::execute(QStringLiteral("icacls"), {native, QStringLiteral("/inheritance:r")}) != 0) {
        return false;
    }
    const QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty()) {
        return false;
    }
    return QProcess::execute(QStringLiteral("icacls"),
                             {native, QStringLiteral("/deny"), user + QStringLiteral(":(OI)(CI)R")}) == 0;
}

bool installDeniedReadFixture(const QString& path) {
    if (!denyDirectoryRead(path)) {
        return false;
    }
    const auto probe = platform::enumerateDirectory(path, []() { return false; });
    if (probe.status() != scan::DirectoryReadStatus::AccessDenied) {
        resetDirectoryAcl(path);
        return false;
    }
    return true;
}

bool platformFixturesRequired() {
    return qEnvironmentVariableIntValue("WTW_REQUIRE_PLATFORM_FIXTURES") != 0;
}

void requirePlatformFixture(bool available, const char* detail) {
    if (available) {
        return;
    }
    if (platformFixturesRequired()) {
        QFAIL(qPrintable(QStringLiteral("Required platform fixture unavailable: ") +
                         QString::fromUtf8(detail)));
    }
    QSKIP(detail);
}

bool snapshotsEqual(const app::SessionDeliverySnapshot& a, const app::SessionDeliverySnapshot& b) {
    return a.scanning == b.scanning && a.scanId == b.scanId && a.sessionId == b.sessionId &&
           a.descriptorVersion == b.descriptorVersion && a.treemapComplete == b.treemapComplete &&
           a.contextPath == b.contextPath && a.publishedTreeName == b.publishedTreeName &&
           a.hasPendingUpdate == b.hasPendingUpdate && a.pendingContextPath == b.pendingContextPath &&
           a.latestProgressPath == b.latestProgressPath && a.lastProgressEmitMs == b.lastProgressEmitMs;
}

app::Session populatedUpdateSession() {
    app::Session session;
    session.scanning = true;
    session.scanId = 9;
    session.sessionId = 42;
    session.descriptorVersion = 7;
    session.treemapComplete = true;
    session.contextPath = QStringLiteral("C:/ctx");
    session.targetPath = QStringLiteral("C:/ctx");
    session.scanRootPath = QStringLiteral("C:/ctx/sub");
    session.scanKind = app::ScanKind::UpdateContext;
    session.publishedTree.name = QStringLiteral("keep-me");
    app::UpdateSnapshot pending;
    pending.contextPath = QStringLiteral("C:/ctx");
    pending.tree.name = QStringLiteral("snapshot-tree");
    session.pendingUpdateSnapshot = std::move(pending);
    return session;
}

std::optional<scan::ScanResult> runWorkerSync(const QString& root, const scan::ScanIdentity& identity,
                                              const std::function<void(scan::ScanWorker*)>& beforeRun = {}) {
    scan::ScanWorker worker(root, identity);
    std::optional<scan::ScanResult> captured;
    QObject::connect(&worker, &scan::ScanWorker::finished, &worker,
                     [&](scan::ScanResult result) { captured = std::move(result); });
    if (beforeRun) {
        beforeRun(&worker);
    }
    worker.run();
    return captured;
}

}  // namespace

class Phase1Tests : public QObject {
    Q_OBJECT

private slots:
    void init() { platform::testResetFindHandleState(); }

    void denied_inner_dir_not_empty_folder() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString parentPath = temp.path();
        const QString deniedPath = temp.filePath(QStringLiteral("denied"));
        QVERIFY(QDir().mkpath(deniedPath));

        QFile visible(temp.filePath(QStringLiteral("visible.txt")));
        QVERIFY(visible.open(QIODevice::WriteOnly));
        visible.write("payload");
        visible.close();

        DenyReadAclGuard guard;
        guard.path = deniedPath;
        if (!installDeniedReadFixture(deniedPath)) {
            requirePlatformFixture(false, "denied-read ACL fixture");
        }
        guard.active = true;

        const auto captured = runWorkerSync(parentPath, {6, 1, 0});
        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Success);
        QVERIFY(captured->tree().has_value());

        const model::FolderDescriptor* deniedChild = nullptr;
        for (const auto& child : captured->tree()->children) {
            if (child.fullPath.compare(deniedPath, Qt::CaseInsensitive) == 0) {
                deniedChild = &child;
                break;
            }
        }
        QVERIFY(deniedChild != nullptr);
        QCOMPARE(deniedChild->traversalState, scan::TraversalState::Unreadable);
        QCOMPARE(deniedChild->treeRole, model::TreeRole::NodeFolder);
        QVERIFY(deniedChild->treeRole != model::TreeRole::EmptyFolder);
    }

    void root_access_denied() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        DenyReadAclGuard guard;
        guard.path = temp.path();
        if (!installDeniedReadFixture(temp.path())) {
            requirePlatformFixture(false, "root denied-read ACL fixture");
        }
        guard.active = true;

        const auto captured = runWorkerSync(temp.path(), {7, 1, 0});
        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::RootUnavailable);
        QVERIFY(!captured->tree().has_value());
    }

    void parent_partial_when_child_unreadable() {
        model::FolderDescriptor parent;
        parent.traversalState = scan::TraversalState::Complete;

        model::FileDescriptor file;
        file.size = 10 * 1024 * 1024;
        parent.files.push_back(file);

        model::FolderDescriptor unreadable;
        unreadable.traversalState = scan::TraversalState::Unreadable;
        unreadable.measuredSize = 0;
        unreadable.sizeCompleteness = scan::SizeCompleteness::Partial;
        parent.children.push_back(unreadable);

        scan::recomputeAggregates(parent);
        QCOMPARE(parent.measuredSize, static_cast<quint64>(10 * 1024 * 1024));
        QCOMPARE(parent.sizeCompleteness, scan::SizeCompleteness::Partial);
    }

    void parent_complete_when_all_children_complete() {
        model::FolderDescriptor parent;
        parent.traversalState = scan::TraversalState::Complete;

        model::FileDescriptor file;
        file.size = 512;
        parent.files.push_back(file);

        model::FolderDescriptor child;
        child.traversalState = scan::TraversalState::Complete;
        model::FileDescriptor childFile;
        childFile.size = 1024;
        child.files.push_back(childFile);
        parent.children.push_back(child);

        scan::recomputeAggregates(parent);
        QCOMPARE(parent.measuredSize, static_cast<quint64>(1536));
        QCOMPARE(parent.sizeCompleteness, scan::SizeCompleteness::Complete);
    }

    void empty_dir_is_empty_folder() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto captured = runWorkerSync(temp.path(), {4, 1, 0});
        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Success);
        QVERIFY(captured->tree().has_value());
        QCOMPARE(captured->tree()->treeRole, model::TreeRole::EmptyFolder);
        QCOMPARE(captured->tree()->measuredSize, static_cast<quint64>(0));
        QCOMPARE(captured->tree()->sizeCompleteness, scan::SizeCompleteness::Complete);
    }

    void reparse_entry_traversal_state() {
        model::FolderDescriptor reparse;
        reparse.traversalState = scan::TraversalState::ReparseTargetNotTraversed;
        reparse.measuredSize = 0;
        reparse.sizeCompleteness = scan::SizeCompleteness::Complete;
        reparse.treeRole = model::TreeRole::NodeFolder;
        scan::recomputeAggregates(reparse);
        QCOMPARE(reparse.traversalState, scan::TraversalState::ReparseTargetNotTraversed);
        QCOMPARE(reparse.measuredSize, static_cast<quint64>(0));
        QCOMPARE(reparse.treeRole, model::TreeRole::NodeFolder);
    }

    void scan_result_stale_scan_id() {
        app::Session session = populatedUpdateSession();
        app::ScanProgressState progress;
        progress.latestProgressPath = QStringLiteral("scanning/sub");
        progress.lastProgressEmitMs = 1000;
        const auto before = app::captureDeliverySnapshot(session, progress);

        model::FolderDescriptor tree;
        tree.name = QStringLiteral("must-not-publish");
        const scan::ScanResult result = scan::ScanResult::success({8, 42, 7}, std::move(tree));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);

        QVERIFY(!apply.accepted);
        QVERIFY(snapshotsEqual(before, app::captureDeliverySnapshot(session, progress)));
    }

    void scan_result_stale_session_id() {
        app::Session session = populatedUpdateSession();
        app::ScanProgressState progress;
        progress.latestProgressPath = QStringLiteral("scanning/sub");
        const auto before = app::captureDeliverySnapshot(session, progress);

        const scan::ScanResult result = scan::ScanResult::cancelled({9, 41, 7});
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);

        QVERIFY(!apply.accepted);
        QVERIFY(session.scanning);
        QVERIFY(snapshotsEqual(before, app::captureDeliverySnapshot(session, progress)));
    }

    void scan_result_stale_descriptor_version() {
        app::Session session = populatedUpdateSession();
        app::ScanProgressState progress;
        const auto before = app::captureDeliverySnapshot(session, progress);

        model::FolderDescriptor tree;
        tree.name = QStringLiteral("must-not-publish");
        const scan::ScanResult result = scan::ScanResult::success({9, 42, 6}, std::move(tree));
        const app::ScanFinishApply apply = app::applyScanFinishedIfCurrent(session, result);

        QVERIFY(!apply.accepted);
        QVERIFY(session.pendingUpdateSnapshot.has_value());
        QVERIFY(snapshotsEqual(before, app::captureDeliverySnapshot(session, progress)));
    }

    void stale_progress_delivery_inert() {
        app::Session session = populatedUpdateSession();
        app::ScanProgressState progress;
        progress.latestProgressPath = QStringLiteral("before");
        progress.lastProgressEmitMs = 1000;
        const auto before = app::captureDeliverySnapshot(session, progress);

        const app::ScanProgressApply apply =
            app::applyScanProgressIfCurrent(session, progress, {9, 99, 7}, QStringLiteral("after"), 2000);

        QVERIFY(!apply.accepted);
        QVERIFY(apply.statusText.isEmpty());
        QVERIFY(snapshotsEqual(before, app::captureDeliverySnapshot(session, progress)));
    }

    void stale_delivery_rejected_after_session_reset() {
        app::Session session;
        session.scanId = 3;
        session.sessionId = 7;
        session.descriptorVersion = 1;
        const scan::ScanIdentity delivered = app::liveScanIdentity(session);
        QVERIFY(app::acceptsScanDelivery(delivered, session));
        session.resetToInitial();
        QVERIFY(!app::acceptsScanDelivery(delivered, session));
    }

    void success_requires_tree() {
        model::FolderDescriptor tree;
        tree.name = QStringLiteral("root");
        const scan::ScanResult result = scan::ScanResult::success({1, 1, 0}, std::move(tree));
        QCOMPARE(result.outcome(), scan::ScanOutcome::Success);
        QVERIFY(result.tree().has_value());
    }

    void cancelled_rejects_tree() {
        const scan::ScanResult result = scan::ScanResult::cancelled({1, 1, 0});
        QCOMPARE(result.outcome(), scan::ScanOutcome::Cancelled);
        QVERIFY(!result.tree().has_value());
    }

    void unreadable_folder_partial_not_in_treemap() {
        model::FolderDescriptor context;
        context.measuredSize = 10;

        model::FolderDescriptor unreadable;
        unreadable.name = QStringLiteral("denied");
        unreadable.fullPath = QStringLiteral("C:/ctx/denied");
        unreadable.traversalState = scan::TraversalState::Unreadable;
        unreadable.measuredSize = 0;
        unreadable.sizeCompleteness = scan::SizeCompleteness::Partial;
        context.children.push_back(unreadable);

        model::FileDescriptor file;
        file.name = QStringLiteral("a.dat");
        file.fullPath = QStringLiteral("C:/ctx/a.dat");
        file.size = 10;
        context.files.push_back(file);

        config::TreemapSettings cfg;
        const auto items = treemap::buildTreemapItems(&context, 100, cfg);
        QCOMPARE(items.size(), static_cast<size_t>(1));
        QCOMPARE(items.front().name, QStringLiteral("a.dat"));
    }

    void zero_size_excluded() {
        model::FolderDescriptor context;
        model::FileDescriptor file;
        file.name = QStringLiteral("empty");
        file.size = 0;
        context.files.push_back(file);

        config::TreemapSettings cfg;
        const auto items = treemap::buildTreemapItems(&context, 100, cfg);
        QVERIFY(items.empty());
    }

    void partial_folder_tile_lower_bound_label() {
        const QString label = util::formatFolderSize(1024, scan::SizeCompleteness::Partial);
        QVERIFY(label.startsWith(QStringLiteral("\u2265 ")));
    }

    void empty_dir_enumeration() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto result = platform::enumerateDirectory(temp.path(), []() { return false; });
        QCOMPARE(result.status(), scan::DirectoryReadStatus::Ok);
        QCOMPARE(result.entries().size(), 0);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }

    void native_handle_closed_on_cancel() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QFile file(temp.filePath(QStringLiteral("one.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a");
        file.close();
        QFile file2(temp.filePath(QStringLiteral("two.txt")));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("b");
        file2.close();

        int seen = 0;
        const auto result = platform::enumerateDirectory(temp.path(), [&]() {
            ++seen;
            return seen > 1;
        });
        QCOMPARE(result.status(), scan::DirectoryReadStatus::Cancelled);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }

    void cancel_between_entries() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QFile file(temp.filePath(QStringLiteral("one.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a");
        file.close();
        QFile file2(temp.filePath(QStringLiteral("two.txt")));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("b");
        file2.close();

        int seen = 0;
        const auto result = platform::enumerateDirectory(temp.path(), [&]() {
            ++seen;
            return seen > 1;
        });
        QCOMPARE(result.status(), scan::DirectoryReadStatus::Cancelled);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }

    void native_handle_closed_on_failure() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QFile file(temp.filePath(QStringLiteral("one.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a");
        file.close();

        platform::testSetFindNextHook([](HANDLE, WIN32_FIND_DATAW*) {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        });
        const auto result = platform::enumerateDirectory(temp.path(), []() { return false; });
        platform::testClearFindNextHook();

        QCOMPARE(result.status(), scan::DirectoryReadStatus::AccessDenied);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }

    void cancel_before_first_entry() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QFile file(temp.filePath(QStringLiteral("one.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a");
        file.close();

        const auto captured = runWorkerSync(temp.path(), {1, 1, 0},
                                            [](scan::ScanWorker* worker) { worker->requestCancel(); });
        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Cancelled);
    }

    void cancel_after_several_entries() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        for (int i = 0; i < 20; ++i) {
            QFile file(temp.filePath(QStringLiteral("file_%1.txt").arg(i, 2, 10, QChar('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("abcdefghijklmnop");
            file.close();
        }

        scan::ScanWorker worker(QDir::cleanPath(temp.path()), {2, 1, 0});
        scan::ScanWorker* workerPtr = &worker;
        int entriesBeforeCancel = 0;
        platform::testSetFindNextHook([&](HANDLE handle, WIN32_FIND_DATAW* data) {
            const BOOL ok = FindNextFileW(handle, data);
            if (ok) {
                ++entriesBeforeCancel;
                if (entriesBeforeCancel >= 5) {
                    workerPtr->requestCancel();
                }
            }
            return ok;
        });

        std::optional<scan::ScanResult> captured;
        QObject::connect(&worker, &scan::ScanWorker::finished, &worker,
                         [&](scan::ScanResult result) { captured = std::move(result); });
        worker.run();
        platform::testClearFindNextHook();

        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Cancelled);
        QVERIFY(entriesBeforeCancel >= 5);
    }

    void root_deleted_before_enum() {
        const QString missing = QDir::tempPath() + QStringLiteral("/wtw_missing_root_") +
                                QString::number(QDateTime::currentMSecsSinceEpoch());
        const auto captured = runWorkerSync(missing, {3, 1, 0});
        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::RootUnavailable);
    }

    void reparse_target_nonempty_excluded() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString targetPath = temp.filePath(QStringLiteral("target"));
        const QString junctionPath = temp.filePath(QStringLiteral("junction"));
        QVERIFY(QDir().mkpath(targetPath));

        QFile inner(temp.filePath(QStringLiteral("target/inner.dat")));
        QVERIFY(inner.open(QIODevice::WriteOnly));
        inner.write("payload");
        inner.close();

        const int mklinkCode = QProcess::execute(
            QStringLiteral("cmd.exe"),
            {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
             QDir::toNativeSeparators(junctionPath), QDir::toNativeSeparators(targetPath)});
        if (mklinkCode != 0) {
            requirePlatformFixture(false, "directory junction creation");
        }

        scan::ScanIdentity identity{5, 1, 0};
        scan::ScanWorker worker(temp.path(), identity);
        std::optional<scan::ScanResult> captured;
        QObject::connect(&worker, &scan::ScanWorker::finished, &worker,
                         [&](scan::ScanResult result) { captured = std::move(result); });
        worker.run();

        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Success);
        QVERIFY(captured->tree().has_value());

        bool foundReparse = false;
        bool targetHasFile = false;
        for (const auto& child : captured->tree()->children) {
            if (child.fullPath.compare(junctionPath, Qt::CaseInsensitive) == 0) {
                foundReparse = true;
                QCOMPARE(child.traversalState, scan::TraversalState::ReparseTargetNotTraversed);
                QVERIFY(child.children.empty());
                QVERIFY(child.files.empty());
            }
            if (child.fullPath.compare(targetPath, Qt::CaseInsensitive) == 0) {
                targetHasFile = !child.files.empty();
            }
        }
        QVERIFY(foundReparse);
        QVERIFY(targetHasFile);
    }

    void clump_sum_overflow_throws() {
        model::FolderDescriptor context;
        context.traversalState = scan::TraversalState::Complete;
        context.measuredSize = std::numeric_limits<quint64>::max();
        for (int i = 0; i < 3; ++i) {
            model::FileDescriptor file;
            file.name = QStringLiteral("big");
            file.fullPath = QStringLiteral("C:/ctx/big");
            file.size = (std::numeric_limits<quint64>::max() / 2) + 1;
            context.files.push_back(file);
        }

        config::TreemapSettings cfg;
        cfg.maxTiles = 1;
        cfg.clumpThreshold = 0.01;
        bool threw = false;
        try {
            treemap::buildTreemapItems(&context, 100, cfg);
        } catch (const std::overflow_error&) {
            threw = true;
        }
        QVERIFY(threw);
    }

    void try_add_detects_overflow() {
        quint64 out = 0;
        QVERIFY(!util::tryAdd(std::numeric_limits<quint64>::max(), 1, &out));
    }

    void aggregate_overflow_throws() {
        model::FolderDescriptor root;
        root.traversalState = scan::TraversalState::Complete;
        model::FileDescriptor huge;
        huge.size = std::numeric_limits<quint64>::max();
        root.files.push_back(huge);
        root.files.push_back(huge);
        bool threw = false;
        try {
            scan::recomputeAggregates(root);
        } catch (const std::overflow_error&) {
            threw = true;
        }
        QVERIFY(threw);
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    qRegisterMetaType<scan::ScanResult>("wtw::scan::ScanResult");
    qRegisterMetaType<scan::ScanIdentity>("wtw::scan::ScanIdentity");
    Phase1Tests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_phase1.moc"
