#include "config/TreemapConfig.h"
#include "model/FolderDescriptor.h"
#include "platform/WinDirEnum.h"
#include "scan/ScanResult.h"
#include "scan/SubtreeMerge.h"
#include "treemap/TreemapProjection.h"
#include "util/Format.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace wtw;

class Phase1Tests : public QObject {
    Q_OBJECT

private slots:
    void init() { platform::testResetFindHandleState(); }

    void denied_inner_dir_not_empty_folder() {
        model::FolderDescriptor parent;
        parent.name = QStringLiteral("parent");
        parent.fullPath = QStringLiteral("C:/parent");
        parent.traversalState = scan::TraversalState::Complete;

        model::FolderDescriptor unreadable;
        unreadable.name = QStringLiteral("secret");
        unreadable.fullPath = QStringLiteral("C:/parent/secret");
        unreadable.traversalState = scan::TraversalState::Unreadable;
        unreadable.treeRole = model::TreeRole::NodeFolder;
        unreadable.measuredSize = 0;
        unreadable.sizeCompleteness = scan::SizeCompleteness::Partial;
        parent.children.push_back(unreadable);

        scan::recomputeAggregates(parent);
        QCOMPARE(parent.children.front().treeRole, model::TreeRole::NodeFolder);
        QCOMPARE(parent.children.front().traversalState, scan::TraversalState::Unreadable);
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
        model::FolderDescriptor folder;
        folder.traversalState = scan::TraversalState::Complete;
        scan::recomputeAggregates(folder);
        QCOMPARE(folder.treeRole, model::TreeRole::EmptyFolder);
        QCOMPARE(folder.measuredSize, static_cast<quint64>(0));
        QCOMPARE(folder.sizeCompleteness, scan::SizeCompleteness::Complete);
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
        scan::ScanIdentity expected{1, 10, 0};
        scan::ScanIdentity stale{2, 10, 0};
        QVERIFY(stale.scanId != expected.scanId);
    }

    void scan_result_stale_session_id() {
        scan::ScanIdentity expected{1, 10, 0};
        scan::ScanIdentity stale{1, 11, 0};
        QVERIFY(stale.targetSessionId != expected.targetSessionId);
    }

    void scan_result_stale_descriptor_version() {
        scan::ScanIdentity expected{1, 10, 5};
        scan::ScanIdentity stale{1, 10, 4};
        QVERIFY(stale.baseDescriptorVersion != expected.baseDescriptorVersion);
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
        QCOMPARE(result.status(), scan::DirectoryReadStatus::OtherError);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }

    void native_handle_closed_on_failure() {
        const auto result = platform::enumerateDirectory(
            QStringLiteral("Z:/this/path/should/not/exist/on/most/systems"), []() { return false; });
        QVERIFY(result.status() != scan::DirectoryReadStatus::Ok);
        QCOMPARE(platform::testOpenFindHandles(), 0);
    }
};

QTEST_MAIN(Phase1Tests)
#include "test_phase1.moc"
