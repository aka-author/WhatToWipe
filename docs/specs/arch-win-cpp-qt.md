# Erase & Rewrite: Architecture (Windows, C++, Qt)

Document type: informal architecture description for implementers. It is not a specification.

Normative stack:

1. [funcspec.md](./funcspec.md) (FS) defines all product behavior, UI text, and acceptance tests.
2. [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) adds Windows, C++, Qt, PE, DPI, I/O, responsiveness, settings-grid, and build baselines that FS does not spell out.
3. This file suggests how to structure code and workflows so the first two stay satisfied. If anything here disagrees with FS or the technical specification, fix this file.
4. [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) records the as-built module; use it to verify that this architecture matches what ships.

Prose in project docs follows [XMSTP](https://github.com/aka-author/xmstp). Agents do not edit FS.


## 1. Scope and migration rationale

This architecture targets one interactive Windows x64 executable built with C++ and Qt 6, as described in techspec §5. It assumes a single process with a Qt GUI thread and background workers for scanning.

### Why leave Go + walk

The `win-go/` delivery implemented most FS use cases but **failed the Settings Form** — the strict FS *real grid* requirement. Attempts included:

- `walk` property-sheet layouts (rejected as grid-like when FS mandates a true grid).
- `SysListView32` with owner-drawn overlay editors (`custom_grid_windows.go`) — cells were not reliably editable at runtime; this matches FS automatic rejection criteria (editors spawned on activation, detached overlay behavior).

Qt provides mature tabular widgets and permanent per-cell widget hosting (`QTableView::setIndexWidget`, composite cell widgets, item delegates with always-visible editors). Moving the Windows delivery to **native Qt** avoids WebView2/Node cost (Wails + AG Grid) while targeting FS grid compliance directly.

The `win-go/` tree remains as reference until feature parity and FS verification are complete on `win-cpp-qt/`.


## 2. Binding FS and techspec into design

- FS tells you what the program must do; it does not name Qt classes or threading primitives.
- Techspec tells you deliverable shape, PE fields, DPI, I/O policy, settings-grid gates (SG-01–SG-10), config compatibility (CF-01–CF-04), and verification rules. Design choices should cite those rows when you verify a build.

Keep a short internal map from FS sections to modules. That is hygiene, not part of this document.


## 3. Repository layout

Implementation root: **`win-cpp-qt/`** (slug matches `techspec-win-cpp-qt.md` per [folder-structure.md](../folder-structure.md)).

The tree below matches the shipping module. See [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §2 for the authoritative file list.

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
    app.rc, app.qrc, toolbar.qrc
  src/
    main.cpp
    app/           Application, MainWindow, Session, Product
    config/        TreemapConfig, ConfigStore
    model/         FolderDescriptor
    scan/          ScanWorker, ArchiveClassifier, SubtreeMerge
    treemap/       TreemapProjection, TreemapLayout, LabelFit, TreemapWidget
    platform/      VolumeInfo, ShellOpen, AppVersion
    ui/            SettingsSchema, SettingsDialog, AboutDialog, AlertDialogs,
                   MenuLabels, ToolbarIcons
    util/          Format
```

CMake target and executable name: `EraseAndRewrite` → `EraseAndRewrite.exe`.

Build outputs follow the project rule: `<ProjectRoot>/bin/win/current/`, not inside `codebase/`.

Qt sources and the compiled static Qt kit live **outside** the repository (see §9).


## 4. Subsystems

Logical roles; class names are suggestions.

### Application shell (`MainWindow`)

`QMainWindow` with:

- **Menu bar** — FS File / Inspect / Tools / Help structure.
- **Command strip** — horizontal `QWidget` + `QHBoxLayout` (not a mixed `QToolBar` that fights FS Total/Free semantics):
  - `QToolButton` — Open, Up, Explore, Update/Stop (icons from `assets/icons` rasterized at 24×24 at 96 DPI, `@2x` for HiDPI).
  - `QLabel` — Total at X: (static text per FS).
  - `QLabel` + `QPushButton` — Free at X: label and free-space button; refreshed after successful Update scan.
- **Treemap host** — `TreemapWidget` (`QSizePolicy::Expanding`; relayout on resize via `layoutAreaChanged`).
- **Status bar** — `QStatusBar`; paths shown with native Windows backslashes.

Menu `QAction`s are the source of truth for shortcuts; `updateChrome()` syncs menu, strip, and treemap enabled state via `app/UpdateChromePolicy` (mirrors `setScanChrome` in `win-go`).

During `UpdateContext` scans, navigation (**Up**, treemap **Dive**, **Explore**) continues against `publishedTree` until the merged snapshot is published atomically through `app/UpdatePublish`. During `OpenTarget` scans, navigation stays disabled because no complete published treemap exists yet.

FS wording "toolbar" means these command affordances, not a specific Win32 or Qt toolbar class.

### Session

Owns:

- `targetPath`, `contextPath` (current folder within the scanned tree)
- `scanning` flag, cancel token
- `treemapComplete` / unset state per FS
- `pendingUpdateSnapshot` for techspec UX-01 restore on cancelled Update

Do not invent alternate meanings for FS treemap states.

### Scanner (`ScanWorker`)

- Runs off the GUI thread.
- Recursive directory walk from context/target folder.
- Cooperative cancellation checked frequently (techspec RS-01).
- Progress: emit throttled path updates (`scanning.updateInterval` = 0.5 s default; not user-editable per FS table).
- Classify `.zip` / `.rar` archives when catalog readable; otherwise packed clump.
- Reparse policy: directory junctions and symlinks are not traversed; record `reparseSkipped` with a reason (techspec IO-03; see impl §6.2).
- On completion: emit `FolderTree` snapshot to GUI thread; never paint partial mixed trees.

### Treemap model

In-memory tree (`FolderNode` equivalent) built by scanner:

- Sizes, volume shares, packing type, tree role among children
- Sufficient to list direct children of current context for treemap tiles

Port field semantics from `win-go/internal/model` and `scan.BuildTreemapItems` as the behavioral baseline.

### Treemap layout

Pure function: child metrics + pixel rectangle → tile rectangles. Areas follow FS volume-share proportions among siblings. Cache until navigation, data, resize, or DPI change (techspec RS-02, DP-02). Squarified layout (same algorithm family as `win-go/internal/layout/squarify.go`) is the default.

**Full-area fill.** Each recursive squarify split extends the trailing child rectangle to the parent area edge on the split axis. Integer rounding therefore does not leave unfilled bands or margins inside the treemap region. The layout input rectangle is the widget’s full `rect()`; there is no inner inset or clipping margin around the diagram.

### Treemap view (`TreemapWidget`)

`QWidget` with `paintEvent`:

- `QSizePolicy::Expanding` on both axes so the treemap consumes all space below the command strip.
- On resize, `resizeEvent` emits `layoutAreaChanged`; `MainWindow` connects this to `rebuildTreemap()` so tile geometry rescales to the new region (techspec DP-02).
- Fill tiles using FS colors from loaded config.
- Label fit algorithm (FS priority: form quality → shortening quality → font size quality):

  1. Binary search heading font between min and max — detailed form.
  2. Shortening at min font — detailed form.
  3. Brief form at min font.
  4. Shortening at min font — brief form.
  5. `treemap.labelDummy`.

- Hit testing for dive-in (left click) and context menu (folder: Explore…; file: Open…).
- Tooltips when label hidden or shortened.
- Mouse cursors per FS table.
- `QAccessible` names optional (techspec A11Y recommendation).

Measure fonts with the same DPI used for painting (techspec DP-01).

### Shell helpers (`platform/`)

- Folder picker: `QFileDialog::getExistingDirectory` (FS: no "Create New Folder" requirement — use native dialog options appropriately).
- Explore: `QDesktopServices::openUrl` or `ShellExecuteW` for directories; handle FS error #003/#004.
- Open file: default association; block executables per `treemap.win.exeFiles`.

### Config (`config/`)

- Parse/write same key=value text format as `win-go/internal/config/file.go`.
- Defaults from FS table.
- Load order (techspec CF-03):
  1. FS path: `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt`
  2. Legacy: `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt` (interim Qt builds only)
- Save: prefer FS path for new installs; document behavior when legacy file was the source.

---

## 5. Settings dialog — FS real grid (critical subsystem)

This is the primary reason for the Qt migration. The design must pass FS Settings Form and techspec SG rows.

### Control strategy declaration (required before coding)

Record in `docs/verification/settings-grid-qt-strategy.md`:

> We use `QTableView` + `QAbstractTableModel` with permanent cell widgets attached through `setIndexWidget` for every cell. Parameter names are read-only labels in column 0. Column 1 hosts the value editor. Color rows split swatch and picker into columns 2 and 3 so all four columns share stable boundaries. Editors are created when the dialog opens and remain visible until it closes. We do not use activation-only delegate editing, `SysListView32` overlays, detached panels, or per-row independent layouts.

### Layout (as built)

The settings grid uses four shared columns.

| Column | Content |
|--------|---------|
| 0 | Parameter name (`QLabel`, read-only) |
| 1 | Value — permanent `QLineEdit` or editable `QComboBox` |
| 2 | Preview — color swatch or empty placeholder |
| 3 | Picker — color … button or empty placeholder |

Row count: one per user-editable FS treemap parameter visible on the host OS (31 rows on Windows — `treemap.win.exeFiles` only; Linux and macOS exe rows omitted). Column widths and resize behavior are documented in impl §8.2.

### Editor mapping

| FS type | Qt widget |
|---------|-----------|
| Integer | `QSpinBox` or `QLineEdit` with `QIntValidator` |
| Float / ratio | `QDoubleSpinBox` with decimals per parameter |
| Percentage (`treemap.clumpThreshold`) | `QDoubleSpinBox` storing fraction 0–1 (display as decimal; FS allows `%` in file but editors use numeric fraction internally) |
| TSize | `QLineEdit` with validator for `pt`/`px`/`mm`/`cm`/`in` suffix |
| String | `QLineEdit` |
| Color | hex in column 1; swatch in column 2; `QPushButton` "…" in column 3 opens `QColorDialog` |
| `treemap.tileFontName` | editable `QComboBox` filled from `QFontDatabase::families()` |

Color swatch: `QLabel` with `setStyleSheet` or `QPalette` background; update on every valid color change and on Reset.

### Validation and actions

Mirror `win-go/internal/ui/validation.go` rules:

- Field validation on commit (focus loss / Enter).
- `validateAll` + `validateObject` stub before save.
- Inline error `QLabel` below grid (red); no `QMessageBox` for field validation (FS-aligned UX).
- `QMessageBox` for catastrophic I/O only; field validation stays inline.

Buttons:

- **Reset Treemap Defaults** — reload UI from built-in defaults; no disk write until Apply/OK.
- **Apply** — validate all → save → apply to live treemap config → keep dialog open.
- **OK** — Apply then accept.
- **Cancel** — close without dirty confirmation (matches Go delivery; see impl §8.4).

Atomic save: serialize full treemap section only after all fields pass.

### Qt patterns to avoid (FS rejection)

| Pattern | Why rejected |
|---------|----------------|
| Default `QTableView` click-to-edit delegate only | Editors not permanently visible |
| `QFormLayout` stack of row pairs | FS: independent per-row layouts |
| `QTabWidget` | FS: no tabs |
| Bottom `QLineEdit` detail panel | FS: detached editor |
| `SysListView32` overlay (Go approach) | Failed in production; overlay on click |

### Qt patterns that are acceptable

| Pattern | Notes |
|---------|-------|
| `QTableView` + `setIndexWidget` | Permanent widgets in cells — primary choice |
| `QScrollArea` + `QGridLayout` with 2–4 columns | FS-acceptable *real grid* if columns align across all rows (see [arch-for-true-grid-win-go.md](./arch-for-true-grid-win-go.md) Q&A) |
| Custom `QStyledItemDelegate` with `createEditor` always shown | Only if editor is **never** hidden; `setIndexWidget` is simpler to verify |

---

## 6. Concurrency and data ownership

```
┌─────────────┐     signals      ┌──────────────┐
│ MainWindow  │ ◄─────────────── │ ScanWorker   │
│ (GUI thread)│ ── start/cancel ►│ (QThread)    │
└──────┬──────┘                  └──────────────┘
       │ owns current snapshot
       ▼
┌─────────────┐
│ TreemapWidget│ reads snapshot, layout cache
└─────────────┘
```

- Scanner builds a new tree in worker memory.
- On success, `MainWindow` replaces `std::shared_ptr<const FolderTree>` (or value swap) on the GUI thread.
- `TreemapWidget` reads only the current snapshot per frame.
- Cancel: drop in-flight worker result when superseded by newer scan id.

Drive share denominator: volume total bytes via `GetDiskFreeSpaceExW` on the volume containing the scan root (same as `win-go/internal/volume`).


## 7. Threading and Qt event loop

- All widget mutation on the GUI thread only.
- Cross-thread updates via `Qt::QueuedConnection` signals.
- Do not call Win32 shell APIs that pump a nested modal loop from the scanner thread.


## 8. Treemap label performance

Large tile counts: label fitting is O(tiles × font sizes). Options:

- Cache label choices per tile until geometry/DPI/config changes (same strategy as `win-go` label cache).
- Optionally defer label solve across `QTimer::singleShot(0)` slices to keep Stop responsive (techspec RS-02).

---

## 9. Qt runtime and linking

The Windows delivery links **Qt 6.10.3** with **MinGW 13.1.0** (`toolchain-qt-mingw.cmake`). The shipping build is **static**: Qt libraries, required image-format and platform plugins, and the MinGW C++ runtime are linked into one `EraseAndRewrite.exe`. There is no `windeployqt` step and no `Qt6*.dll` beside the program.

### Two Qt installations on the build machine

| Path (typical) | Role |
|----------------|------|
| `C:\cpp\qt\6.10.3\mingw_64` | Shared Qt kit from the Qt installer; import libraries + DLLs. Used only as the reference for toolchain discovery and as the starting point when building the static kit. |
| `C:\cpp\qt\6.10.3\mingw_64_static` | Static Qt kit produced locally. CMake `CMAKE_PREFIX_PATH` for shipping builds. `Qt6::Core` is `STATIC IMPORTED`. |
| `C:\cpp\qt-src\6.10.3\Src\` | Qt **source** trees (`qtbase`, `qtsvg`) downloaded by `build-qt-static.ps1`. Not in the Git repository. |

The repository contains build scripts only. Qt source archives are fetched on demand via `aqt install-src` when the static prefix is missing.

### Building the static Qt kit (`build-qt-static.ps1`)

One-time (or after Qt version bump):

1. Download `qtbase` and `qtsvg` source for 6.10.3 into `SourceRoot` (default `C:\cpp\qt-src`).
2. Configure and build **static** `qtbase` with CMake/Ninja (`BUILD_SHARED_LIBS=OFF`, bundled zlib/PNG/JPEG/HarfBuzz/Freetype).
3. Build and install **static** `qtsvg` against that prefix.
4. Install to `mingw_64_static` beside the shared kit.

The script verifies the result by checking `Qt6CoreTargets.cmake` for `STATIC IMPORTED`. Expect a long first run (on the order of one hour).

### Application link model (`CMakeLists.txt`)

CMake option `WTW_STATIC_QT` (ON for shipping):

- Sets `QT_USE_STATIC_LIBS` before `find_package(Qt6)`.
- Links `Qt6::Widgets` and `Qt6::Svg`.
- Calls `qt_import_plugins` so plugin code is embedded rather than loaded from `platforms/` or `imageformats/` at runtime:
  - `QWindowsIntegrationPlugin` (Windows platform)
  - `QSvgPlugin`, `QICOPlugin`, `QJpegPlugin`, `QGifPlugin` (toolbar SVGs, `app.ico`, about PNG)
- MinGW link flags: `-static -static-libgcc -static-libstdc++` so the executable does not import `libstdc++-6.dll` or `libgcc_s_seh-1.dll`.
- `WIN32` subsystem on `qt_add_executable` (techspec PL-07): no console window on launch.

Dynamic linking remains available for development (`WTW_STATIC_QT=OFF`): same CMake project, shared prefix, `deploy-standalone.ps1` + `windeployqt`, and `build/` instead of `build-static/`.

### Shipping executable size and debug overlay stripping

A static Qt Widgets link naturally produces a large PE image (~29 MB of code and read-only data: Qt Widgets → Gui → Core, platform plugin, HarfBuzz/FreeType, embedded plugins, and the application). That size is expected for a monolithic Qt6 GUI binary even when the program uses only a subset of Qt APIs.

MinGW static linking has a second, **avoidable** cost: the linker can append a **debug overlay** (~20 MB) after the last PE section. The overlay is DWARF debug data pulled from static `.a` archives; it is not required to run the program and is not visible as normal PE sections in the loaded image. Observed sizes on this line:

| Stage | Typical size | Notes |
|-------|--------------|--------|
| Link output (`build-static/EraseAndRewrite.exe`) | ~47 MB | PE image ~29 MB + debug overlay ~20 MB |
| After `strip --strip-all` (shipping) | ~28 MB | Overlay removed; `.text` / `.rdata` unchanged |

**Shipping policy:** `build.ps1 -StaticQt` calls `Strip-MingwStaticExe` immediately after copying the exe to `bin/win/current/`. The step runs the MinGW `strip.exe` from the same toolchain as the link (`QT_MINGW_ROOT`) with **`--strip-all`**, which removes the out-of-image debug tail.

**WR-03 compatibility:** techspec WR-03 forbids stripping that removes standard PE metadata. This step targets the MinGW debug overlay only. The `.rsrc` section (Windows `VERSIONINFO`, icon) and normal executable metadata remain. `test-run.ps1 -StaticQt` smoke-launches the stripped binary after every build.

**Not used:** `strip --strip-debug` alone — it removes only in-section debug tables (~100 KB) and leaves the large overlay intact.

Further size reduction (optional, not part of shipping today): Qt feature pruning when building `mingw_64_static`, link-time optimization (LTO), dropping unused image plugins, or rasterizing toolbar SVGs to remove `Qt6::Svg`. Those are separate from the overlay strip.

### Runtime dependencies after static link

The process still loads Windows system DLLs (`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `dwmapi`, etc.). It does **not** load `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Svg.dll`, or plugin DLLs from a deploy folder.

### Resources compiled into the binary

- Toolbar icons: SVG files in `toolbar.qrc` (rendered through the linked SVG plugin).
- Window icon and about art: `app.qrc` (`app.ico`, `about-bunny.png`).
- Windows `VERSIONINFO`: `resources/app.rc` from `versioninfo.json`.

No Qt QML, no WebEngine, no separate translation deploy.


## 10. Build and release

- **Shipping command:** `build.ps1 -StaticQt` on branch `dev`.
- **CMake** + **Qt 6.10.3 MinGW 13.1.0**; build directory `build-static/` when `-StaticQt`.
- `build.ps1` flow: bump version resource, commit snapshot, archive prior `bin/win/current` via `.date` marker, optionally **wipe legacy Qt deploy artifacts** (DLLs and plugin folders) from all `bin/win/*` folders, configure with `-DWTW_STATIC_QT=ON`, build, copy **only** `EraseAndRewrite.exe` to `bin/win/current`, **`strip --strip-all`** (~47 MB link output → ~28 MB shipping exe), write `versioninfo.json` / `build-meta.json` / `.date`, append `docs/history/builds.txt`, run `test-run.ps1 -StaticQt`.
- `test-run.ps1` (static): requires the exe only; `objdump` checks that neither MinGW runtime DLLs nor `Qt6*.dll` appear in the import table; smoke-launches the window.
- **Dynamic fallback:** `build.ps1` without `-StaticQt` still runs `deploy-standalone.ps1` / `windeployqt` into `bin/win/current`.
- Icon pipeline: `assets/art/broombunny.png` → `win-go/tools/genicons` → `app.ico`; toolbar SVGs from `codebase/assets/icons` via `toolbar.qrc`.
- Installer: Inno Setup reads `bin/win/current`; static builds ship a single large exe plus sidecar metadata files.

---

## 11. Testing hints

| Area | Approach |
|------|----------|
| FS use cases | Manual scripts keyed to FS headings |
| Techspec IO/DPI/RS | Fixtures + VM screenshots |
| Settings grid SG | Manual FS definition-of-done checklist on clean VM |
| Config CF | Round-trip tests: parse `win-go/dist/Erase & Rewrite.config.txt`, save, reload |
| Scanner | Unit tests for squarify, packing, share math (no Qt GUI needed) |
| UX-01 | Automated or manual: Update → cancel → prior snapshot visible |

Priority test: **settings grid manual pass** before declaring Go target retired.


## 12. Relationship to Go implementation

| Go module (`win-go/`) | Qt port source |
|-----------------------|----------------|
| `internal/model` | `treemap/` types |
| `internal/scan` | `scan/` |
| `internal/layout/squarify.go` | `TreemapLayout` |
| `internal/config` | `config/` (format 1:1) |
| `internal/ui/run_windows.go` | `MainWindow` + `TreemapWidget` |
| `internal/ui/row_schema.go` etc. | `SettingsSchema` + `SettingsDialog` |
| `internal/ui/custom_grid_windows.go` | **Do not port** — replace with Qt permanent cell widgets |

Behavioral parity is the goal; line-by-line port is not required.


## 13. Non-goals

- Linux or macOS Qt builds (FS allows parameters for other OSes; this arch is Windows-only).
- Embedding web views for settings or treemap.
- Changing FS config key names or treemap semantics.
- Maintaining two active Windows delivery lines after Qt FS verification is complete.
