#include "scan/ScanTypes.h"

namespace wtw::scan {

DirectoryReadResult::DirectoryReadResult(DirectoryReadStatus status, DWORD nativeError)
    : status_(status), nativeError_(nativeError) {}

DirectoryReadResult::DirectoryReadResult(DirectoryReadStatus status, QVector<platform::DirEntry> entries)
    : status_(status), entries_(std::move(entries)) {}

DirectoryReadResult DirectoryReadResult::ok(QVector<platform::DirEntry> entries) {
    return DirectoryReadResult(DirectoryReadStatus::Ok, std::move(entries));
}

DirectoryReadResult DirectoryReadResult::cancelled() {
    return DirectoryReadResult(DirectoryReadStatus::Cancelled, 0);
}

DirectoryReadResult DirectoryReadResult::accessDenied(DWORD nativeError) {
    return DirectoryReadResult(DirectoryReadStatus::AccessDenied, nativeError);
}

DirectoryReadResult DirectoryReadResult::sharingViolation(DWORD nativeError) {
    return DirectoryReadResult(DirectoryReadStatus::SharingViolation, nativeError);
}

DirectoryReadResult DirectoryReadResult::notFound(DWORD nativeError) {
    return DirectoryReadResult(DirectoryReadStatus::NotFound, nativeError);
}

DirectoryReadResult DirectoryReadResult::otherError(DWORD nativeError) {
    return DirectoryReadResult(DirectoryReadStatus::OtherError, nativeError);
}

}  // namespace wtw::scan
