#pragma once

#include "app/Session.h"
#include "model/FolderDescriptor.h"

namespace wtw::app {

enum class UpdatePublicationStatus { Ready, ContextMissing, MergeFailed };

struct PreparedUpdatePublication {
    UpdatePublicationStatus status = UpdatePublicationStatus::MergeFailed;
    model::FolderDescriptor tree;
    QString contextPath;
    quint64 descriptorVersion = 0;
    QString error004Target;
};

const model::FolderDescriptor* findFolderByContextPath(const model::FolderDescriptor& publishedTree,
                                                     const QString& targetPath, const QString& contextPath);

PreparedUpdatePublication prepareUpdatePublication(const UpdateSnapshot& snapshot, const QString& scanRoot,
                                                   const model::FolderDescriptor& newSubtree,
                                                   const QString& liveContextPath, const QString& targetPath,
                                                   quint64 currentDescriptorVersion, quint64 driveTotal);

void publishPreparedUpdate(Session& session, PreparedUpdatePublication prepared);

QString resolveRestoredUpdateContext(const model::FolderDescriptor& restoredTree, const QString& targetPath,
                                     const QString& liveContextPath, const QString& snapshotContextPath);

void restorePendingUpdateSession(Session& session);

}  // namespace wtw::app
