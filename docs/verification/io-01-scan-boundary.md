# IO-01 scan cancellation boundary (Phase 1)

## Shipped mechanism

Directory enumeration uses `FindFirstFileExW` / `FindNextFileW` in `platform/WinDirEnum.cpp`. `scan/ScanWorker` runs on a dedicated `QThread` off the Qt GUI thread.

## Cooperative cancellation

- `ScanWorker::requestCancel()` sets an atomic flag.
- `WinDirEnum` checks the cancel predicate **between returned directory entries** (after each `FindNextFileW` result is processed).
- When cancel is observed, enumeration returns `DirectoryReadStatus::Cancelled` and the native find handle is closed through RAII.

## Honest non-guarantees

- An individual Win32 syscall (`FindFirstFileExW`, `FindNextFileW`) may block until the OS or filesystem returns. There is **no** guaranteed wall-clock bound per directory in-process.
- The former `readDirBounded` / `std::async` path and any claim of a **30 s per-directory timeout** are removed. Techspec IO-04 aspiration for bounded network I/O is not implemented as a per-directory timer; network roots are rejected at open via `VolumeInfo::validateLocalVolume`.

## Handle lifetime

`FindHandle` RAII closes search handles on success, enumeration failure, cancellation, and stack unwind. Unit tests `native_handle_closed_on_cancel`, `native_handle_closed_on_failure`, and `cancel_between_entries` assert zero open handles via `platform::testOpenFindHandles()`.

## Evidence

- `win-cpp-qt/tests/test_phase1.cpp` (Phase 1 harness, `ctest` target `phase1_tests`)
- `docs/specs/impl-win-cpp-qt.md` §6
- `.github/workflows/win-cpp-qt-phase1.yml` with `WTW_REQUIRE_PLATFORM_FIXTURES=1`
