#pragma once

#include "model/FolderDescriptor.h"
#include <QString>
#include <optional>

namespace wtw::app {

enum class ScanKind { OpenTarget, UpdateContext };

struct UpdateSnapshot {
    model::FolderDescriptor tree;
    QString contextPath;
};

class Session {
public:
    QString targetPath;
    QString contextPath;
    QString volBarRoot;
    QString volLabel;
    quint64 driveTotal = 0;
    quint64 driveFree = 0;

    model::FolderDescriptor publishedTree;
    bool treemapComplete = false;

    bool scanning = false;
    quint64 scanId = 0;
    ScanKind scanKind = ScanKind::OpenTarget;
    QString scanRootPath;
    std::optional<UpdateSnapshot> pendingUpdateSnapshot;

    void resetToInitial();
    const model::FolderDescriptor* resolveContextFolder() const;
    void setContextPath(const QString& path);
    void pushContext(const QString& folderPath);
    bool canGoUp() const;
    void goUp();
};

model::FolderDescriptor cloneFolder(const model::FolderDescriptor& src);

}  // namespace wtw::app
