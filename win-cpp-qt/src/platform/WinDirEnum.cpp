#include "platform/WinDirEnum.h"

#include <QDir>
#include <QString>

#include <windows.h>

namespace wtw::platform {

namespace {

#ifdef WTW_UNIT_TEST
int g_openFindHandles = 0;
#endif

class FindHandle {
public:
    explicit FindHandle(HANDLE handle) : handle_(handle) {
#ifdef WTW_UNIT_TEST
        if (handle_ != INVALID_HANDLE_VALUE) {
            ++g_openFindHandles;
        }
#endif
    }

    FindHandle(const FindHandle&) = delete;
    FindHandle& operator=(const FindHandle&) = delete;

    FindHandle(FindHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }

    FindHandle& operator=(FindHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    ~FindHandle() { close(); }

    HANDLE get() const { return handle_; }
    bool valid() const { return handle_ != INVALID_HANDLE_VALUE; }

private:
    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            FindClose(handle_);
#ifdef WTW_UNIT_TEST
            --g_openFindHandles;
#endif
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

quint64 fileSizeFromFindData(const WIN32_FIND_DATAW& data) {
    ULARGE_INTEGER size;
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart;
}

bool isDotOrDotDot(const wchar_t* name) {
    return wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0;
}

scan::DirectoryReadResult failureFromWin32(DWORD error) {
    switch (error) {
    case ERROR_ACCESS_DENIED:
        return scan::DirectoryReadResult::accessDenied(error);
    case ERROR_SHARING_VIOLATION:
        return scan::DirectoryReadResult::sharingViolation(error);
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return scan::DirectoryReadResult::notFound(error);
    default:
        return scan::DirectoryReadResult::otherError(error);
    }
}

platform::DirEntry entryFromFindData(const QString& parentPath, const WIN32_FIND_DATAW& data) {
    platform::DirEntry entry;
    entry.name = QString::fromWCharArray(data.cFileName);
    entry.fullPath = QDir::cleanPath(parentPath + QLatin1Char('/') + entry.name);
    entry.attributes = data.dwFileAttributes;
    entry.reparseTag = data.dwReserved0;
    entry.size = fileSizeFromFindData(data);
    entry.creationTime = data.ftCreationTime;
    entry.lastWriteTime = data.ftLastWriteTime;
    entry.isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry.isReparsePoint = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    return entry;
}

}  // namespace

scan::DirectoryReadResult enumerateDirectory(const QString& path, CancelPredicate isCancelled) {
    if (isCancelled && isCancelled()) {
        return scan::DirectoryReadResult::otherError(ERROR_OPERATION_ABORTED);
    }

    const QString nativePath = QDir::toNativeSeparators(path);
    const QString pattern = nativePath + QStringLiteral("\\*");
    WIN32_FIND_DATAW data{};
    FindHandle handle(FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()), FindExInfoBasic,
                                       &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH));
    if (!handle.valid()) {
        return failureFromWin32(GetLastError());
    }

    QVector<platform::DirEntry> entries;
    do {
        if (isCancelled && isCancelled()) {
            return scan::DirectoryReadResult::otherError(ERROR_OPERATION_ABORTED);
        }
        if (!isDotOrDotDot(data.cFileName)) {
            entries.push_back(entryFromFindData(path, data));
        }
    } while (FindNextFileW(handle.get(), &data));

    const DWORD enumError = GetLastError();
    if (enumError != ERROR_NO_MORE_FILES) {
        return failureFromWin32(enumError);
    }

    return scan::DirectoryReadResult::ok(std::move(entries));
}

#ifdef WTW_UNIT_TEST
int testOpenFindHandles() { return g_openFindHandles; }

void testResetFindHandleState() { g_openFindHandles = 0; }
#endif

}  // namespace wtw::platform
