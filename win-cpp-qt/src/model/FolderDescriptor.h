#pragma once

#include <QColor>
#include <QDateTime>
#include <QRect>
#include <QString>
#include <optional>
#include <vector>

namespace wtw::model {

enum class PackingType { Native, PackedFile, PackedFolder, PackedClump };
enum class TreeRole { NodeFolder, LeafFolder, EmptyFolder };
enum class ObjectType { File, Folder };

struct FileDescriptor {
    QString name;
    QString fullPath;
    qint64 size = 0;
    double volumeShare = 0.0;
    ObjectType type = ObjectType::File;
    PackingType packing = PackingType::Native;
    std::optional<QDateTime> oldestFile;
    std::optional<QDateTime> newestFile;
    int nestedFolderCount = 0;
    int nestedFileCount = 0;
};

struct FolderDescriptor {
    QString name;
    QString fullPath;
    qint64 size = 0;
    double volumeShare = 0.0;
    int nestedFolderCount = 0;
    int nestedFileCount = 0;
    std::optional<QDateTime> oldestFile;
    std::optional<QDateTime> newestFile;
    TreeRole treeRole = TreeRole::EmptyFolder;
    std::vector<FolderDescriptor> children;
    std::vector<FileDescriptor> files;
    bool reparseSkipped = false;
    QString skipReason;
};

using FolderTree = FolderDescriptor;

enum class TreemapItemKind { Folder, File, Clump };

struct TreemapItem {
    QString name;
    QString path;
    qint64 size = 0;
    double driveShare = 0.0;
    QColor bg;
    QColor text;
    TreemapItemKind kind = TreemapItemKind::File;
    bool isNode = false;
    bool isEmpty = false;
    bool isExecFile = false;
};

struct BlockLayout {
    TreemapItem item;
    QRect rect;
};

void annotateShares(FolderDescriptor& node, quint64 driveTotal);

}  // namespace wtw::model
