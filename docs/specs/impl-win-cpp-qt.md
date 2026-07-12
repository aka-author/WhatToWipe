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
2. [legalspec.md](./legalspec.md) — normative licensing and distribution obligations.
3. [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) — normative platform add-ons.
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
| Executable | `EraseAndRewrite.exe` (~28 MB static build after MinGW strip; ~47 MB unstripped link output) |
| Deploy folder | `<ProjectRoot>/bin/win/current/` — exe plus `versioninfo.json`, `build-meta.json`, `.date` marker only (no Qt DLLs or plugin folders) |
| Product name (UI, PE strings) | Erase & Rewrite |
| Config path (new installs) | `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt` |
| Qt modules linked | `Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`, `Qt6::Svg` (static archives when `WTW_STATIC_QT=ON`) |
| C++ standard | C++17 |
| Toolchain | Qt 6.10.3 MinGW 13.1.0 via `toolchain-qt-mingw.cmake` |
| Qt kit (shipping) | `C:\cpp\qt\6.10.3\mingw_64_static` (built locally; not in repo) |
| Version resource | `resources/app.rc` from `versioninfo.json`; About reads `FileVersion` in `platform/AppVersion.cpp` |

**Shipping build:** `build.ps1 -StaticQt` links Qt and required plugins into one executable (`WTW_STATIC_QT` in CMake). See §13.

**Legacy dynamic build:** `build.ps1` without `-StaticQt` still supports shared Qt + `windeployqt` for development.


## 2. Source layout

The module contains the following top-level areas.

```
win-cpp-qt/
  CMakeLists.txt
  toolchain-qt-mingw.cmake
  build.ps1
  build-qt-static.ps1
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

Qt **source** trees and the compiled static Qt prefix live on the build machine outside the repository (`build-qt-static.ps1`; see §13.3).


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

The window icon is `:/app/app.ico`, generated from `assets/art/broombunny.png` by `win-go/tools/genicons` during `build.ps1`.


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

`MainWindow::refreshVolumeToolbar()` formats Total and Free from `Session::driveTotal`, `driveFree`, and `volLabel`. Opening a folder calls `platform::validateLocalVolume` and updates session volume fields before the first scan. After a successful **Update**, `ScanDelivery::applyScanFinishedIfCurrent` re-queries the current volume, updates session totals, re-annotates `publishedTree` shares, and queues `ScanFinishUiAction::RefreshVolumeIndicators` so the strip shows fresh capacity and free space. The Free button still refreshes free space alone on click.

`updateChrome()` mirrors menu enablement onto strip buttons via `app/UpdateChromePolicy`. During `UpdateContext` scans, **Up**, **Dive**, and **Explore** stay enabled against the published tree; during `OpenTarget` scans navigation is disabled. Update and Stop share one button and swap play/stop icons.


### 4.4 Status bar

The status bar shows Choose a target folder when the treemap is unset, the context path when the treemap is complete, and the folder currently being scanned during a scan (throttled per `scanning.updateInterval` via `ScanDelivery::applyScanProgressIfCurrent`).

Path text for the status bar is formatted in `util/Format.cpp` by `formatPathForStatusBar()`. On Windows this uses `QDir::toNativeSeparators` after `QDir::cleanPath`, so displayed paths use backslash separators (for example `C:\Users\...\Projects`) per FS Status Bar rules. `MainWindow::statusForContext()` and scan progress delivery pass paths through this helper before `QStatusBar::showMessage`.


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

On successful Update completion, `prepareUpdatePublication` merges the pending snapshot with the scanned subtree, resolves the **live** `contextPath` (navigation during the scan counts), validates that path in the merged tree, increments `descriptorVersion` once, and `publishPreparedUpdate` assigns tree, context, version, and completeness atomically. `applyScanFinishedIfCurrent` then refreshes volume-indicator session fields from `validateLocalVolume(targetPath)` and requests toolbar refresh.

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
| Update success | merge + publish; refresh volume indicators; rebuild treemap; live `contextPath` preserved on restore after cancel/failure when still valid in snapshot tree |

`ScanFinishUiAction` values handled in `MainWindow::onScanFinished` include `RebuildTreemap`, `ResetTreemapUi`, `StatusForContext`, `RefreshVolumeIndicators`, and FS error or interruption alerts.


### 6.7 Open compliance gap (IO-02)

`ScanDiagnostics` records unreadable-directory and reparse-skip counts, but `onScanFinished` does not yet surface an incomplete-run summary to the user. Partial read failures therefore do not yet trigger the techspec IO-02 incomplete-run summary.


## 7. Treemap pipeline

The treemap pipeline has four stages: projection, layout, label fit, and view.


### 7.1 Projection

`treemap/TreemapProjection.cpp` ports `win-go/internal/scan/treemap_build.go`. It selects direct children of the context folder, applies clumping by `treemap.clumpThreshold`, assigns packing types and FS colors from `TreemapSettings`, and detects executables via `treemap.win.exeFiles`.


### 7.2 Layout

`treemap/TreemapLayout.cpp` ports `win-go/internal/layout/squarify.go`. Minimum tile dimensions convert from config point fields using widget DPI (`devicePixelRatio * 96 / 72`).

Each recursive split extends the trailing child rectangle to the parent area edge on the split axis so integer rounding does not leave unfilled bands inside the treemap region. Test `squarify_fills_layout_area` in `phase1_tests` checks that the laid-out tile bounds reach all four edges of the layout rectangle.


### 7.3 Label fit

`treemap/LabelFit.cpp` ports Go `resolveTileLabel` with binary search and shortening. The five-step FS priority runs from detailed form through brief form to `treemap.labelDummy`. Shortened headings get a tooltip.

In the details block, the size line is rendered by `TreemapWidget` using `util::formatObjectSize()` only. The FS size format is one fractional digit, a space, and a unit suffix (`TB`, `GB`, `MB`, or `KB`). Partial folder `measuredSize` values still use that format; there is no lower-bound prefix on tile labels.


### 7.4 View

`treemap/TreemapWidget.cpp` paints tiles, hit-tests, and handles input.

**Stretch and relayout (FS treemap size / techspec DP-02).**

- `QSizePolicy::Expanding` on both axes; the widget is the stretch child below the command strip in `MainWindow::buildUi`.
- `MainWindow::rebuildTreemap()` projects context children and runs `squarify()` over the widget’s full `rect()` — no inner margin or clip inset.
- `TreemapWidget::resizeEvent` emits `layoutAreaChanged`; `MainWindow` is connected to call `rebuildTreemap()` again so tiles rescale when the window is resized or maximized.
- Layout §7.2 edge extension prevents gaps inside that rectangle after integer rounding.

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

`SettingsSchema::treemapRowSchemas()` supplies treemap parameter rows in the same order and with the same validation rules as `win-go/internal/ui/row_schema.go`, `validation.go`, and `config_mapper.go`. On Windows the grid lists 31 rows: host-OS-specific executable-extension parameters show only `treemap.win.exeFiles` (not Linux or macOS rows). Other platforms include only their own `treemap.*.exeFiles` key.

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


### 10.4 Display formatting

`util/Format.cpp` centralizes user-visible numeric and path formatting:

| Function | Use |
|----------|-----|
| `formatObjectSize` | Tile size lines and other byte counts; FS one-decimal unit suffix |
| `formatPathForStatusBar` | Status bar paths; native separators on Windows |
| `formatShareLine` | Volume share percentage lines in tile labels |


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
| `app.ico` | `assets/art/broombunny.png` via `win-go/tools/genicons` during `build.ps1` |
| Toolbar SVGs | `codebase/assets/icons/toolbar-*.svg` in `toolbar.qrc` |
| About PNG | `genaboutpng` or `assets/art/about-bunny.png` at build time |

Toolbar icons render at 24×24 inside 32×32 hit targets. Qt scales for HiDPI.


## 13. Build, Qt linking, and deployment

### 13.1 CMake and linking

CMake target name: `EraseAndRewrite`.

| Setting | Shipping (`-StaticQt`) | Dynamic (dev) |
|---------|------------------------|---------------|
| CMake option | `WTW_STATIC_QT=ON` | `WTW_STATIC_QT=OFF` (default) |
| Build directory | `build-static/` | `build/` |
| Qt prefix | `mingw_64_static` | `mingw_64` (shared kit) |
| Qt link mode | Static `.a` archives | Shared `.dll` + import libs |
| Plugin delivery | `qt_import_plugins` embeds platform and image plugins | `windeployqt` copies `platforms/`, etc. |
| MinGW runtime | `-static -static-libgcc -static-libstdc++` | `-static-libgcc -static-libstdc++` + dynamic `winpthread` via Qt DLLs |
| Deploy folder contents | `EraseAndRewrite.exe` only (+ metadata written after build) | exe + `Qt6*.dll` + plugin folders + MinGW runtime DLLs |

Common to both modes:

- `qt_add_executable(..., WIN32)` — GUI subsystem, no startup console (techspec PL-07).
- Linked Qt components: `Qt6::Widgets`, `Qt6::Svg`.
- Windows libraries: `shell32`, `ole32`, `version`.
- Embedded plugins when static (from `CMakeLists.txt`):
  - `QWindowsIntegrationPlugin`
  - `QSvgPlugin`, `QICOPlugin`, `QJpegPlugin`, `QGifPlugin`

Unit tests `phase1_tests` and `phase2_tests` are built by the same CMake project; run via `ctest` from the active build directory.


### 13.2 build.ps1 (shipping: `-StaticQt`)

The script mirrors `win-go/build.ps1` discipline:

1. Bump `versioninfo.json` build counter; regenerate `app.rc` / icons.
2. Git commit snapshot (`build: version …`) when `.git` is present.
3. `Prepare-CurrentBuildFolder` — archive prior `bin/win/current` using the `.date` marker stem.
4. **`Wipe-BinQtDeployArtifacts`** — remove obsolete `Qt6*.dll`, MinGW runtime DLLs, and plugin directories (`platforms/`, `styles/`, `imageformats/`, etc.) from every folder under `bin/win/`, including archived builds. Keeps exe and metadata files.
5. Resolve static Qt prefix (`mingw_64_static` beside shared kit). If missing, invoke `build-qt-static.ps1`.
6. CMake configure/build `EraseAndRewrite` into `build-static/`.
7. Copy exe to `bin/win/current/`; **`strip --strip-all`** removes ~20 MB MinGW debug overlay (WR-03: PE `VERSIONINFO` / `.rsrc` preserved). Strip any non-exe files from `current` (static mode).
8. `test-run.ps1 -StaticQt` — smoke launch; `objdump` verifies no `Qt6*.dll` or `libstdc++-6.dll` imports.
9. Write `versioninfo.json`, `build-meta.json`, `.date` marker; append `docs/history/builds.txt`; second git commit for history.

Environment overrides: `CMAKE_PREFIX_PATH` (shared kit hint), `QT_STATIC_PREFIX` (explicit static prefix).


### 13.3 build-qt-static.ps1 (one-time Qt kit)

Not run on every app build once `mingw_64_static` exists.

| Step | Action |
|------|--------|
| Download | `python -m aqt install-src windows 6.10.3` — archives `qtbase`, `qtsvg` into `SourceRoot` (default `C:\cpp\qt-src`) |
| Build qtbase | CMake/Ninja, `BUILD_SHARED_LIBS=OFF`, install to `mingw_64_static` |
| Build qtsvg | Same, with `CMAKE_PREFIX_PATH` pointing at installed qtbase |
| Verify | `Qt6::Core` is `STATIC IMPORTED` in `Qt6CoreTargets.cmake` |

Uses the MinGW compiler from `Qt/Tools/mingw1310_64` (same as app builds). First run is long; subsequent app builds reuse the installed prefix.

**Repository policy:** Qt source and static libraries are **not** vendored in Git. Only scripts and CMake options are in `codebase/`.


### 13.4 deploy-standalone.ps1 (dynamic mode only)

`windeployqt --release --compiler-runtime` copies Qt DLLs and `platforms/qwindows.dll` into a target folder, then overwrites MinGW runtime DLLs from the Qt toolchain. Used when `build.ps1` runs **without** `-StaticQt`. `scripts/deploy-win-cpp-qt.ps1` redeploys into an existing `bin/win/current` without rebuilding.


### 13.5 test-run.ps1

| Mode | Checks |
|------|--------|
| Static (`-StaticQt` or no `Qt6Core.dll` beside exe) | `EraseAndRewrite.exe` exists; no `Qt6*.dll` in import table; no `libstdc++-6.dll` / `libgcc_s_seh-1.dll` imports; main window appears within timeout |
| Dynamic | Above plus required DLLs and `platforms/qwindows.dll` present on disk |

Launches the exe from `bin/win/current` (or `-BinDir`).


### 13.6 Static executable size and MinGW debug overlay stripping

Static linking embeds Qt and plugins into one PE file. Two size components must be distinguished.

**Functional image (~29 MB).** Dominated by `.text` (~19.5 MB) and `.rdata` (~7.5 MB): static Qt6 Widgets/Gui/Core, Windows platform integration plugin, text rendering stack (HarfBuzz, FreeType), bundled codecs, embedded `toolbar.qrc` / `app.qrc`, and first-party code. The program links only `Qt6::Widgets` and `Qt6::Svg`, but the Widgets stack pulls a large dependency closure. This is the practical floor for the current feature set unless the static Qt kit or assets are redesigned.

**Debug overlay (~20 MB, removed before ship).** MinGW leaves DWARF debug data from static archive objects in a tail **after** the last PE section. `objdump -h` shows a loaded image of ~29 MB while the file on disk is ~47 MB. The overlay does not affect runtime behavior; it inflates installer payload and confuses size review.

**Implementation (`build.ps1`).** Function `Strip-MingwStaticExe` runs after `Copy-WithRetry` when `-StaticQt` is set:

```powershell
& $mingwRoot\bin\strip.exe --strip-all $ExePath
```

The script logs before/after byte counts. Build **1.0.0.001A** established the shipping line at ~27.8 MB after strip (link output ~49.3 MB).

| Check | Expectation |
|-------|-------------|
| WR-03 | `.rsrc` / `VERSIONINFO` preserved; no aggressive removal of standard PE metadata |
| WR-01–WR-03 | `app.rc` strings unchanged by strip; About and PE version still match `versioninfo.json` |
| Static imports | Still no `Qt6*.dll`, `libstdc++-6.dll`, or `libgcc_s_seh-1.dll` in import table |
| Smoke test | `test-run.ps1 -StaticQt` runs on the stripped exe in `bin/win/current` |

**Manual strip (emergency only).** To strip an already-built static exe without a full rebuild, run the same command from the Qt MinGW toolchain against `bin/win/current/EraseAndRewrite.exe`. Prefer rebuilding through `build.ps1 -StaticQt` so version metadata and history stay consistent.

**Future size work (documented, not shipping requirements):** prune unused qtbase features in `build-qt-static.ps1`, enable LTO, remove `QGifPlugin` / `QJpegPlugin` if assets stay PNG/SVG-only, exclude `QModernWindowsStylePlugin` (app uses Fusion), or rasterize toolbar SVGs and drop `Qt6::Svg`.


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
| PL-01–PL-06 | implemented | static Qt shipping build; see §13 |
| PL-07 | implemented | `WIN32` GUI subsystem; no startup console; direct `EraseAndRewrite.exe` launch |
| WR-01–WR-03 | implemented | `app.rc` + `AppVersion` |
| WR-04 | open | Authenticode not in build script |
| WR-05 | implemented | `Copy-WithRetry` in `build.ps1` |
| DP-01–DP-02 | implemented | pt→px in layout/labels; treemap relayout on widget resize via `layoutAreaChanged` |
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


## 16. Recent UI and treemap compliance (builds 0015–0019)

The following FS-aligned changes shipped before the static-Qt build line; they remain part of the current tree.

| Area | Change | Where |
|------|--------|-------|
| Treemap stretch | Widget expands to fill main-window client area below the command strip; relayout on resize | `TreemapWidget`, `MainWindow::buildUi`, `layoutAreaChanged` → `rebuildTreemap` |
| Treemap full fill | Squarify extends trailing tile to layout edge; no unfilled bands from rounding | `TreemapLayout.cpp`; test `squarify_fills_layout_area` |
| Tile size labels | `formatObjectSize()` only (one decimal + unit); no `≥` lower-bound prefix on partial folders | `Format.cpp`, `TreemapWidget` / `LabelFit` |
| Status bar paths | Backslash separators on Windows via `formatPathForStatusBar()` | `Format.cpp`, `MainWindow`, `ScanDelivery` |
| Volume strip after Update | Re-query volume on successful Update; refresh Total/Free labels | `ScanDelivery`, `ScanFinishUiAction::RefreshVolumeIndicators` |
| Settings grid rows | Windows shows 31 rows — `treemap.win.exeFiles` only; Linux/macOS exe rows hidden | `SettingsSchema.cpp` |
| Console on launch | `WIN32` flag on `qt_add_executable` (PL-07) | `CMakeLists.txt` |
| Bin layout (static) | `bin/win/current` holds monolithic exe + metadata; legacy Qt DLL/plugin trees wiped from archives | `build.ps1 -StaticQt`, `Wipe-BinQtDeployArtifacts` |
| Static exe size | Link ~47 MB; shipping ~28 MB after `strip --strip-all` removes MinGW debug overlay | `Strip-MingwStaticExe` in `build.ps1`; impl §13.6 |


## 17. Document maintenance

When `win-cpp-qt/` behavior changes:

1. Update this file for module boundaries, FS-visible behavior, or build outputs.
2. Update [arch-win-cpp-qt.md](./arch-win-cpp-qt.md) when design intent changes.
3. Update [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) only for new normative requirements.
4. Keep [settings-grid-qt-strategy.md](../verification/settings-grid-qt-strategy.md) aligned with settings column and widget choices.

Do not edit [funcspec.md](./funcspec.md) from agent workflows; the user owns FS.
