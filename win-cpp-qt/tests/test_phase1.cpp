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
            QSKIP("Could not install a denied-read ACL fixture on this machine");
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
            QSKIP("Could not install a denied-read ACL fixture on this machine");
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
        app::Session session;
        session.scanning = true;
        session.scanId = 5;
        session.sessionId = 10;
        session.descriptorVersion = 2;
        const scan::ScanResult result = scan::ScanResult::cancelled({4, 10, 2});
        QVERIFY(!app::applyScanFinishedIfCurrent(session, result));
        QVERIFY(session.scanning);
    }

    void scan_result_stale_session_id() {
        app::Session session;
        session.scanning = true;
        session.scanId = 1;
        session.sessionId = 10;
        session.descriptorVersion = 0;
        const scan::ScanResult result = scan::ScanResult::cancelled({1, 11, 0});
        QVERIFY(!app::applyScanFinishedIfCurrent(session, result));
        QVERIFY(session.scanning);
    }

    void scan_result_stale_descriptor_version() {
        app::Session session;
        session.scanning = true;
        session.scanId = 1;
        session.sessionId = 10;
        session.descriptorVersion = 5;
        const scan::ScanResult result = scan::ScanResult::cancelled({1, 10, 4});
        QVERIFY(!app::applyScanFinishedIfCurrent(session, result));
        QVERIFY(session.scanning);
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
        for (int i = 0; i < 200; ++i) {
            QFile file(temp.filePath(QStringLiteral("file_%1.txt").arg(i, 4, 10, QChar('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("abcdefghijklmnop");
            file.close();
        }

        scan::ScanIdentity identity{2, 1, 0};
        auto* worker = new scan::ScanWorker(QDir::cleanPath(temp.path()), identity);
        std::optional<scan::ScanResult> captured;
        QObject* app = QCoreApplication::instance();

        QThread thread;
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::started, worker, &scan::ScanWorker::run);
        QObject::connect(worker, &scan::ScanWorker::finished, app,
                         [&](scan::ScanResult result) { captured = std::move(result); }, Qt::QueuedConnection);

        thread.start();
        const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + 5000;
        while (!captured.has_value() && thread.isRunning() && QDateTime::currentMSecsSinceEpoch() < deadline) {
            worker->requestCancel();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            QThread::msleep(5);
        }
        thread.quit();
        QVERIFY(thread.wait(10000));
        worker->deleteLater();

        QVERIFY(captured.has_value());
        QCOMPARE(captured->outcome(), scan::ScanOutcome::Cancelled);
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
            QSKIP("Directory junction creation unavailable on this machine");
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
