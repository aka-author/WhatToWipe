# Erase & Rewrite: Implementation Description (Windows, C++, Qt)


## About This Document

### Identification

Document ID: IMPL-WIN-CPP-QT.

Genre: as-built implementation description.

Module: `win-cpp-qt/`.


### Purpose

This document records what the `win-cpp-qt/` module does today. Reviewers use it to trace FS and techspec requirements to concrete source files, build outputs, and known compliance gaps.


### Target audience

Developers, reviewers, and agents working on the Windows Qt delivery line.


### Precedence

Product meaning follows this order:

1. [funcspec.md](./funcspec.md) (FS) — normative; not edited by agents.
2. [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) — normative platform add-ons.
3. [arch-win-cpp-qt.md](./arch-win-cpp-qt.md) — informative design intent.
4. This file — factual as-built map.

If this file disagrees with FS or techspec, FS and techspec win and this file must be corrected.

Informative companion: [settings-grid-qt-strategy.md](../verification/settings-grid-qt-strategy.md) (techspec SG-09).


### Prose standard

This document follows [XMSTP](https://github.com/aka-author/xmstp) (GRTD, GPIT, ICIT, en-words).


## Terms

*Functional specification (FS):* [funcspec.md](./funcspec.md); the sole normative source for product behavior.

*Module:* the `win-cpp-qt/` source tree that builds `EraseAndRewrite.exe`.

*Program:* the interactive Windows executable `EraseAndRewrite.exe` produced from the module.

*Published tree:* the folder snapshot last produced by a complete scan and held in session state for treemap display.

*Settings grid:* the modal treemap configuration dialog implemented as a shared-column table with permanent cell widgets.


## 1. Shipped program

The module ships one Windows GUI program. Its technical parameters are listed below.

| Parameter | Value |
|-----------|-------|
| Executable | `EraseAndRewrite.exe` |
| Deploy folder | `<ProjectRoot>/bin/win/current/` |
| Product name (UI, PE strings) | Erase & Rewrite |
| Config path (new installs) | `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt` |
| Qt modules linked | `Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`, `Qt6::Svg` |
| C++ standard | C++17 |
| Toolchain | Qt 6.10.3 `mingw_64` + MinGW 13.1.0 via `toolchain-qt-mingw.cmake` |
| Version resource | `resources/app.rc` from `versioninfo.json`; About reads `FileVersion` in `platform/AppVersion.cpp` |

`build.ps1` bumps the build counter, commits a snapshot, runs CMake/ninja, copies the program to `bin/win/current`, runs standalone Qt deploy (`deploy-standalone.ps1`), writes `build-meta.json` and a `.date` marker, appends `docs/history/builds.txt`, and runs `test-run.ps1`. Unit tests `phase1_tests` and `phase2_tests` run via `ctest` in `win-cpp-qt/build`.


## 2. Source layout

The module contains the following top-level areas.

```
win-cpp-qt/
  CMakeLists.txt
  toolchain-qt-mingw.cmake
  build.ps1
  deploy-standalone.ps1
  test-run.ps1
  versioninfo.json
  resources/
    app.rc
    app.qrc
    toolbar.qrc
  src/
    main.cpp
    app/           Application, MainWindow, Session, ScanDelivery,
                   UpdateChromePolicy, UpdatePublish, Product
    config/        TreemapConfig, ConfigStore
    model/         FolderDescriptor
    scan/          ScanWorker, ScanTypes, ScanResult, ArchiveClassifier,
                   SubtreeMerge
    platform/      VolumeInfo, ShellOpen, AppVersion, WinDirEnum
    treemap/       TreemapProjection, TreemapLayout, LabelFit, TreemapWidget
    ui/            SettingsSchema, SettingsDialog, AboutDialog, AlertDialogs,
                   MenuLabels, ToolbarIcons
    util/          Format, CheckedMath
  tests/
    test_phase1.cpp
    test_phase2.cpp
```

Build products never live inside `codebase/`; they land under `<ProjectRoot>/bin/win/current/` per [folder-structure.md](../folder-structure.md).


## 3. Runtime structure

The program uses one GUI thread and one scan worker thread. The diagram below shows the main relationships.

```
┌────────────────── GUI thread (Qt event loop) ──────────────────┐
│ Application → MainWindow → TreemapWidget                       │
│   menus, command strip, status bar, dialogs                    │
│   rebuildTreemap() on navigation or settings apply             │
└───────────────▲────────────────────────────────────────────────┘
                │ Qt::QueuedConnection
┌───────────────┴────────────────────────────────────────────────┐
│ QThread + ScanWorker (scan/ScanWorker.cpp)                       │
│   recursive directory walk, cancel flag, progress throttle       │
└────────────────────────────────────────────────────────────────┘
```

All widget mutation stays on the GUI thread. `MainWindow::startScan` creates a `QThread`, moves `ScanWorker` onto it, and connects `progress` and `finished` with queued delivery.

Scan identity uses `Session::scanId`. The value increments at each scan start. Stale worker completions are ignored when `scanId` no longer matches.

Cancel calls `ScanWorker::requestCancel()`, which sets an atomic flag. The worker returns with `cancelled=true`.


## 4. Application shell

The application shell covers window chrome, menus, the command strip, and the status bar.


### 4.1 Window chrome

The main window title comes from `productDisplayName()` in `app/Product.h` and reads Erase & Rewrite.

The window icon is `:/app/app.ico`, generated by `win-go/tools/genicons` during `build.ps1`.


### 4.2 Menu bar

`MainWindow::buildMenus()` implements the menu bar. Labels and shortcuts match the Go reference `run_windows.go`.

The table below lists menu commands.

| Menu | Item | Shortcut | Notes |
|------|------|----------|-------|
| File | Open a Folder… | Ctrl+O | `ui::openFolderMenuLabel()` uses Unicode ellipsis U+2026 |
| File | Exit | Ctrl+X | |
| Inspect | Up | Backspace | |
| Inspect | Explore… | Ctrl+E | |
| Inspect | Update / Stop | Ctrl+S / Esc | mutually exclusive visibility |
| Tools | Settings… | | opens the settings grid |
| Help | About… | | |

Menu mnemonics use `&` in menu text. Dialog text uses a literal ampersand in the product name.


### 4.3 Command strip

The command strip is a horizontal `QWidget` with `QHBoxLayout`, not a `QToolBar`.

The strip contains Open, Up, Explore, and Update/Stop buttons at 32×32 pixels with 24×24 SVG icons from `ToolbarIcons.cpp` and `toolbar.qrc`.

Volume indicators sit after the buttons with a separator: a static Total at X label and a Free at X label with a refresh `QPushButton`.

`updateChrome()` mirrors menu enablement onto strip buttons via `app/UpdateChromePolicy`. During `UpdateContext` scans, **Up**, **Dive**, and **Explore** stay enabled against the published tree; during `OpenTarget` scans navigation is disabled. Update and Stop share one button and swap play/stop icons.


### 4.4 Status bar

The status bar shows the context path while the treemap is complete, a throttled scan path during scanning, and the initial prompt Choose a target folder.


## 5. Session state

`app/Session` holds navigation and scan state. The table below lists the main fields.

| Field | Role |
|-------|------|
| `targetPath` | user-chosen scan root |
| `contextPath` | current folder within the published tree |
| `volBarRoot`, `volLabel`, `driveTotal`, `driveFree` | volume strip data |
| `publishedTree` | last complete scan snapshot |
| `treemapComplete` | treemap displayed as complete |
| `scanning`, `scanId`, `scanKind`, `scanRootPath` | active scan |
| `pendingUpdateSnapshot` | techspec UX-01 snapshot for Update cancel or failure |

`pushContext`, `goUp`, and `canGoUp` walk `contextPath` relative to `targetPath`. Dive-in is a left click on a non-empty folder tile; `TreemapWidget` emits `diveRequested`.


### 5.1 Update navigation and publication

Update behavior is split across three helpers:

| Module | Role |
|--------|------|
| `app/UpdateChromePolicy` | `computeChromeAvailability`, `canNavigateUp`, `canNavigateDive` — scanKind-gated chrome during `OpenTarget` vs `UpdateContext` |
| `app/ScanDelivery` | identity-gated progress and scan-finished delivery; maps outcomes to session mutations and `ScanFinishUiAction` list |
| `app/UpdatePublish` | `prepareUpdatePublication`, `publishPreparedUpdate`, `restorePendingUpdateSession`, `resolveRestoredUpdateContext` |

During `UpdateContext`, the treemap reads only `publishedTree`; the in-flight worker tree is never painted. **Up**, **Dive**, and **Explore** stay enabled against the published snapshot. Open, Update, and Settings stay disabled until the scan ends.

On successful Update completion, `prepareUpdatePublication` merges the pending snapshot with the scanned subtree, resolves the **live** `contextPath` (navigation during the scan counts), validates that path in the merged tree, increments `descriptorVersion` once, and `publishPreparedUpdate` assigns tree, context, version, and completeness atomically.

On Update cancel or failure, `restorePendingUpdateSession` restores the snapshot tree but **preserves the live `contextPath`** when that path still exists in the restored tree; otherwise it falls back to the snapshot context or target. `ResetTreemapUi` clears painted tiles, volume strip, and status text when Open paths unset the session.

Automated coverage: `tests/test_phase1.cpp` (`phase1_tests`) for scanner foundation; `tests/test_phase2.cpp` (`phase2_tests`) for Update chrome, publication, stale delivery, and rollback semantics. Windows CI: `.github/workflows/win-cpp-qt-phase1.yml` with `WTW_REQUIRE_PLATFORM_FIXTURES=1`.


## 6. Scanning

`scan/ScanWorker` performs the directory walk on a dedicated `QThread` off the Qt GUI thread. Results are delivered as typed `scan::ScanResult` values with `scan::ScanIdentity` (`scanId`, `targetSessionId`, `baseDescriptorVersion`).


### 6.1 Algorithm

`scanDir()` walks recursively from the scan root using `platform::WinDirEnum` (`FindFirstFileExW` / `FindNextFileW`). Children sort by descending `measuredSize`. `ArchiveClassifier` handles `.zip` and `.rar` when the catalog is readable. Each folder node stores `measuredSize`, `sizeCompleteness`, `traversalState`, oldest and newest file times, and recomputed aggregates via `scan::recomputeAggregates()`.


### 6.2 Reparse points

Directory junctions and symlinks are not traversed (techspec IO-03). The worker records a child with `traversalState = ReparseTargetNotTraversed`, `measuredSize = 0`, and `sizeCompleteness = Complete`. Linked-target contents are not nested under the reparse entry. Policy: [io-03-reparse-policy.md](../verification/io-03-reparse-policy.md).


### 6.3 Enumeration, cancellation, and I/O boundary

`platform/WinDirEnum` returns typed `DirectoryReadResult` values (`Ok`, `AccessDenied`, `SharingViolation`, `NotFound`, `Cancelled`, and other errors). Cooperative cancel is checked between returned entries; individual Win32 syscalls may block until the OS returns. Native find handles close through RAII on success, failure, cancellation, and exceptions. There is **no** in-process 30 s per-directory wall-clock guarantee. Boundary notes: [io-01-scan-boundary.md](../verification/io-01-scan-boundary.md).

`VolumeInfo::validateLocalVolume` rejects network drives (`DRIVE_REMOTE`) at open (techspec IO-04 partial: no bounded network wait yet).


### 6.4 Unreadable directories

When enumeration fails, the node keeps `traversalState = Unreadable`, `treeRole = NodeFolder`, `measuredSize = 0`, and `sizeCompleteness = Partial`. `TreeRole::EmptyFolder` is assigned only after successful enumeration establishes zero nested objects. Root enumeration failure maps to `ScanOutcome::RootUnavailable`.


### 6.5 Progress

`maybeEmitProgress` emits at most once per 500 ms with `(ScanIdentity, path)`, matching the FS default `scanning.updateInterval`. `MainWindow::onScanProgress` ignores deliveries whose identity does not match the live session.


### 6.6 Scan completion

`MainWindow::onScanFinished` and `onScanProgress` delegate session mutations to `app/ScanDelivery` after identity validation. Stale deliveries are observationally inert for progress, chrome-related session fields, diagnostics, published descriptor, and pending update state.

| Outcome | Behavior |
|---------|----------|
| `Cancelled` | Update restores snapshot tree (live `contextPath` preserved when valid) and shows interruption alert; Open unsets treemap |
| `TechnicalFailure` | error 002; restore or unset per scan kind |
| `RootUnavailable` | error 001 |
| Open success | replace `publishedTree`, set context to scan root, rebuild treemap |
| Update success | `prepareUpdatePublication` + `publishPreparedUpdate`; live `contextPath` preserved on restore after cancel/failure when still valid in snapshot tree |


### 6.7 Open compliance gap (IO-02)

`ScanDiagnostics` records unreadable-directory and reparse-skip counts, but `onScanFinished` does not yet surface an incomplete-run summary to the user. Partial read failures therefore do not yet trigger the techspec IO-02 incomplete-run summary.


## 7. Treemap pipeline

The treemap pipeline has four stages: projection, layout, label fit, and view.


### 7.1 Projection

`treemap/TreemapProjection.cpp` ports `win-go/internal/scan/treemap_build.go`. It selects direct children of the context folder, applies clumping by `treemap.clumpThreshold`, assigns packing types and FS colors from `TreemapSettings`, and detects executables via `treemap.win.exeFiles`.


### 7.2 Layout

`treemap/TreemapLayout.cpp` ports `win-go/internal/layout/squarify.go`. Minimum tile dimensions convert from config point fields using widget DPI (`devicePixelRatio * 96 / 72`).


### 7.3 Label fit

`treemap/LabelFit.cpp` ports Go `resolveTileLabel` with binary search and shortening. The five-step FS priority runs from detailed form through brief form to `treemap.labelDummy`. Shortened headings get a tooltip.


### 7.4 View

`treemap/TreemapWidget.cpp` paints tiles, hit-tests, and handles input.

A left click on a folder tile dives in. A right click opens a context menu: Explore… on folder tiles, Open… on file tiles (disabled for executables). Dive is not duplicated on the context menu.

Cursors: pointing hand on diveable folders; forbidden on empty folders and executable files.


## 8. Settings grid

The settings grid is implemented in `ui/SettingsDialog.cpp` and `ui/SettingsSchema.cpp`.


### 8.1 Control strategy

See [settings-grid-qt-strategy.md](../verification/settings-grid-qt-strategy.md) for techspec SG-01–SG-09.

The dialog uses `QTableView`, `SettingsGridModel`, and `setIndexWidget` on every cell. It does not use activation-only delegates, detached editor panels, or tabs.


### 8.2 Columns

The grid has four shared columns.

| Col | Header | Content |
|-----|--------|---------|
| 0 | Parameter | read-only `QLabel`, 170 px fixed |
| 1 | Value | permanent `QLineEdit` or editable `QComboBox`, stretch |
| 2 | Preview | color swatch 32×18 or empty placeholder, 58 px fixed |
| 3 | (empty) | color picker … button or empty placeholder, 34 px fixed |

`applyGridColumnWidths()` runs on show and resize. The Value column receives the remaining viewport width with a 110 px minimum.


### 8.3 Rows

`SettingsSchema::treemapRowSchemas()` supplies 32 rows in the same order and with the same validation rules as `win-go/internal/ui/row_schema.go`, `validation.go`, and `config_mapper.go`.

Numeric fields use `QLineEdit` with schema bounds. TSize fields accept `pt` and `mm` via `parsePointsInputToPt`. `treemap.clumpThreshold` stores a fraction and serializes with a `%` suffix. `treemap.tileFontName` is an editable `QComboBox` from `QFontDatabase::families()`. Color rows keep hex in the Value column, a live swatch in Preview, and `QColorDialog` from the picker column.


### 8.4 Actions

Reset Treemap Defaults reloads the UI from `defaultTreemapSettings()` without writing disk.

Apply runs `validateAllRows`, `validateObjectRows`, and `rowStatesToTreemap`, shows inline errors, performs atomic `saveTreemap`, and emits `settingsApplied`.

OK runs Apply then accepts the dialog.

Cancel closes without a dirty confirmation, matching Go behavior (dispute §14).

Dialog geometry persists in `treemap.ui.settingsDialog*` keys on Apply or OK.


## 9. Configuration

Configuration lives in `config/TreemapConfig.h` and `config/ConfigStore.cpp`.


### 9.1 File format

The file uses key=value lines and `#` comments. Keys are case-insensitive on read. `ConfigStore::serializeTreemap` matches the Go `file.go` field set.


### 9.2 Paths

| Path | Use |
|------|-----|
| `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt` | primary (techspec CF-02) |
| `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt` | legacy import when the primary path is absent (techspec CF-03) |

`loadOrInitTreemap` creates defaults when missing. `importLegacyIntoFsFile` merges a legacy file into the primary path.


### 9.3 Typed settings

`TreemapSettings` holds all `treemap.*` parameters plus dialog X, Y, W, and H. Struct default initializers match the FS default table.


## 10. Platform layer

### 10.1 Volume

`platform/VolumeInfo.cpp` calls `GetDiskFreeSpaceExW` for total and free bytes, formats drive labels, and rejects `DRIVE_REMOTE` paths in `validateLocalVolume`.


### 10.2 Shell

`platform/ShellOpen.cpp` opens folders in Explorer and files with the default association, mapping FS errors 003 and 004.


### 10.3 Version

`platform/AppVersion.cpp` reads PE `FileVersion` via `version.dll`, falls back to adjacent `versioninfo.json`, and supplies About and `QApplication::setApplicationVersion`.


## 11. Dialogs and alerts

The table below maps dialogs to source files.

| Dialog | File | Notes |
|--------|------|-------|
| About | `AboutDialog.cpp` | bunny art, Go text lines, version from PE |
| Errors 001–004 | `AlertDialogs.cpp` | FS error text; title is product name |
| Interruption | `AlertDialogs.cpp` | scan cancelled message |
| Settings | `SettingsDialog.cpp` | see §8 |


## 12. Resources and icons

| Asset | Source |
|-------|--------|
| `app.ico` | `win-go/tools/genicons` via `build.ps1` |
| Toolbar SVGs | `codebase/assets/icons/toolbar-*.svg` in `toolbar.qrc` |
| About PNG | `genaboutpng` or `assets/art/about-bunny.png` at build time |

Toolbar icons render at 24×24 inside 32×32 hit targets. Qt scales for HiDPI.


## 13. Build and deployment

### 13.1 CMake and linking

CMake target name: `EraseAndRewrite`. MinGW builds statically link `libstdc++`, `libgcc`, and `winpthread` to avoid ABI mismatch with deployed Qt DLLs. Windows libraries: `shell32`, `ole32`, `version`.


### 13.2 build.ps1

The script mirrors `win-go/build.ps1`: version bump, git commit snapshot, `Prepare-CurrentBuildFolder`, deploy to `bin/win/current`, history append.


### 13.3 Deployment

`deploy-standalone.ps1` and `windeployqt` copy Qt runtime DLLs and plugins. `scripts/deploy-win-cpp-qt.ps1` redeploys without a full rebuild.


### 13.4 test-run.ps1

The script launches the program, checks that the main window appears, verifies required DLLs, and confirms the executable does not dynamically import `libstdc++-6.dll`.


## 14. Go reference map

The table below maps Go packages to Qt sources.

| Go (`win-go/`) | Qt (`win-cpp-qt/`) |
|----------------|---------------------|
| `internal/ui/run_windows.go` | `MainWindow.cpp` |
| `internal/ui/row_schema.go` etc. | `SettingsSchema.cpp` |
| `internal/ui/custom_grid_windows.go` | not ported; replaced by `SettingsDialog` |
| `internal/scan/treemap_build.go` | `TreemapProjection.cpp` |
| `internal/layout/squarify.go` | `TreemapLayout.cpp` |
| label fit in `run_windows.go` | `LabelFit.cpp` |
| `internal/scan/scan_windows.go` | `ScanWorker.cpp` |
| `internal/config/file.go` | `ConfigStore.cpp` |
| `internal/volume` | `VolumeInfo.cpp` |
| `internal/ui/about_windows.go` | `AboutDialog.cpp` |


## 15. Techspec compliance snapshot

The table below records techspec row status as of the current tree. Update it when gaps close.

| Row | Status | Notes |
|-----|--------|-------|
| TS-01 / TS-02 | implemented | settings real grid shipped |
| PL-01–PL-06 | implemented | see §1, §13 |
| WR-01–WR-03 | implemented | `app.rc` + `AppVersion` |
| WR-04 | open | Authenticode not in build script |
| WR-05 | implemented | `Copy-WithRetry` in `build.ps1` |
| DP-01–DP-02 | implemented | pt→px in layout/labels; relayout on resize |
| IO-01 | open | no documented `longPathAware` manifest |
| IO-02 | gap | `errorCount` not shown after scan |
| IO-03 | implemented | skip reparse; see [io-03-reparse-policy.md](../verification/io-03-reparse-policy.md) |
| IO-04 | partial | network rejected at open; no per-directory wall-clock bound — see [io-01-scan-boundary.md](../verification/io-01-scan-boundary.md) |
| RS-01–RS-03 | implemented | worker thread, cancel, throttled progress |
| SG-01–SG-10 | implemented | strategy doc present; manual VM gate external |
| CF-01–CF-04 | implemented | see §9 |
| UX-01 | implemented | `pendingUpdateSnapshot` restore |


### 15.1 Dispute findings addressed in Phase 1

| Finding | Status | Evidence |
|---------|--------|----------|
| 23 | pending CI | `WinDirEnum` replaces `readDirBounded`; [io-01-scan-boundary.md](../verification/io-01-scan-boundary.md); handle-closure tests; passing run required from [win-cpp-qt-phase1.yml](../../.github/workflows/win-cpp-qt-phase1.yml) |
| 24 | pending CI | typed `DirectoryReadResult`; `TraversalState::Unreadable`; ACL fixtures with `WTW_REQUIRE_PLATFORM_FIXTURES=1` |
| 25 | pending CI | `TraversalState::ReparseTargetNotTraversed`; junction fixture in mandatory CI mode |
| 39 | pending CI | `ScanResult` + `ScanIdentity`; `ScanDelivery` session and UI-action contract tests |
| 40 | pending CI | typed `ScanOutcome`; string-heuristic outcomes removed from scanner path |

Findings in this table move to **closed** only after a passing Windows CI run with mandatory platform fixtures.


### 15.2 Dispute findings addressed in Phase 2

| Finding | Status | Evidence |
|---------|--------|----------|
| 22 | pending CI | `UpdateChromePolicy`, `UpdatePublish`, `test_phase2.cpp`; navigation during `UpdateContext` scan |

Finding 22 moves to **closed** after `phase2_tests` pass in CI.


## 16. Document maintenance

When `win-cpp-qt/` behavior changes:

1. Update this file for module boundaries, FS-visible behavior, or build outputs.
2. Update [arch-win-cpp-qt.md](./arch-win-cpp-qt.md) when design intent changes.
3. Update [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) only for new normative requirements.
4. Keep [settings-grid-qt-strategy.md](../verification/settings-grid-qt-strategy.md) aligned with settings column and widget choices.

Do not edit [funcspec.md](./funcspec.md) from agent workflows; the user owns FS.
