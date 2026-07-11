#pragma once

#include "platform/DirEntry.h"

#include <QVector>
#include <optional>

namespace wtw::scan {

enum class ScanOutcome {
    Invalid,
    Success,
    Cancelled,
    RootUnavailable,
    TechnicalFailure,
};

enum class DirectoryReadStatus {
    Invalid,
    Ok,
    AccessDenied,
    SharingViolation,
    NotFound,
    OtherError,
};

enum class TraversalState {
    Complete,
    Unreadable,
    ReparseTargetNotTraversed,
};

enum class SizeCompleteness {
    Complete,
    Partial,
};

class DirectoryReadResult {
public:
    static DirectoryReadResult ok(QVector<platform::DirEntry> entries);
    static DirectoryReadResult accessDenied(DWORD nativeError);
    static DirectoryReadResult sharingViolation(DWORD nativeError);
    static DirectoryReadResult notFound(DWORD nativeError);
    static DirectoryReadResult otherError(DWORD nativeError);

    DirectoryReadStatus status() const { return status_; }
    const QVector<platform::DirEntry>& entries() const { return entries_; }
    DWORD nativeError() const { return nativeError_; }

private:
    explicit DirectoryReadResult(DirectoryReadStatus status, DWORD nativeError = 0);
    DirectoryReadResult(DirectoryReadStatus status, QVector<platform::DirEntry> entries);

    DirectoryReadStatus status_ = DirectoryReadStatus::Invalid;
    QVector<platform::DirEntry> entries_;
    DWORD nativeError_ = 0;
};

struct ScanDiagnostics {
    int unreadableDirectoryCount = 0;
    int reparseNotTraversedCount = 0;
};

}  // namespace wtw::scan
