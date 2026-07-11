# Erase & Rewrite: Architecture (Windows, C++, Qt)

Document type: informal architecture description for implementers. It is not a specification.

Normative stack:

1. [funcspec.md](./funcspec.md) (FS) defines all product behavior, UI text, and acceptance tests.
2. [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) adds Windows, C++, Qt, PE, DPI, I/O, responsiveness, settings-grid, and build baselines that FS does not spell out.
3. This file suggests how to structure code and workflows so the first two stay satisfied. If anything here disagrees with FS or the technical specification, fix this file.


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


## 3. Repository layout (proposed)

Implementation root: **`win-cpp-qt/`** (slug matches `techspec-win-cpp-qt.md` per [folder-structure.md](../folder-structure.md)).

Suggested tree:

```
win-cpp-qt/
  CMakeLists.txt
  src/
    main.cpp
    app/
      Application.{h,cpp}          # QApplication setup, DPI, translators
      MainWindow.{h,cpp}           # shell, menus, command strip, status bar
      Session.{h,cpp}              # target path, nav stack, scan state
    treemap/
      TreemapWidget.{h,cpp}        # paint, hit-test, tooltips, labels
      TreemapLayout.{h,cpp}        # squarify / area allocation
      LabelFit.{h,cpp}             # FS label-fit algorithm
    scan/
      ScanWorker.{h,cpp}           # QThread or run() on QThreadPool
      ArchiveClassifier.{h,cpp}    # zip/rar packing types
    config/
      Config.{h,cpp}               # load/save, defaults, legacy path
      TreemapSettings.{h,cpp}      # typed treemap parameters
    ui/
      SettingsDialog.{h,cpp}       # FS settings grid
      AboutDialog.{h,cpp}
      ErrorDialogs.{h,cpp}
    platform/
      VolumeInfo.{h,cpp}           # GetDiskFreeSpaceExW, drive label
      ShellOpen.{h,cpp}            # Explorer, file associations
  resources/
    app.rc                         # VERSIONINFO, icon
    icons/                         # links or copies from codebase/assets/icons
  cmake/
    QtDeploy.cmake
  build.ps1                        # version bump, output to bin/win/current
```

Build outputs follow the existing project rule: **`<ProjectRoot>/bin/win/current/`**, not inside `codebase/`.


## 4. Subsystems

Logical roles; class names are suggestions.

### Application shell (`MainWindow`)

`QMainWindow` with:

- **Menu bar** — FS File / Inspect / Tools / Help structure.
- **Command strip** — horizontal `QWidget` + `QHBoxLayout` (not a mixed `QToolBar` that fights FS Total/Free semantics):
  - `QToolButton` — Open, Up, Explore, Update/Stop (icons from `assets/icons` rasterized at 24×24 at 96 DPI, `@2x` for HiDPI).
  - `QLabel` — Total at X: (static text per FS).
  - `QLabel` + `QPushButton` — Free at X: label and free-space button.
- **Treemap host** — `TreemapWidget` (stretch).
- **Status bar** — `QStatusBar`.

Menu `QAction`s are the source of truth for shortcuts; a single `updateChrome()` syncs menu, strip, and treemap enabled state (mirrors `setScanChrome` in `win-go`).

FS wording "toolbar" means these command affordances, not a specific Win32 or Qt toolbar class.

### Session

Owns:

- `targetPath`, `navPath` (stack of folder paths within scanned tree)
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
- Reparse policy: same as documented `win-go` choice (visited canonical path set, skip on cycle) unless verification note revises it.
- On completion: emit `FolderTree` snapshot to GUI thread; never paint partial mixed trees.

### Treemap model

In-memory tree (`FolderNode` equivalent) built by scanner:

- Sizes, volume shares, packing type, tree role among children
- Sufficient to list direct children of current context for treemap tiles

Port field semantics from `win-go/internal/model` and `scan.BuildTreemapItems` as the behavioral baseline.

### Treemap layout

Pure function: child metrics + pixel rectangle → tile rectangles. Areas follow FS volume-share proportions among siblings. Cache until navigation, data, resize, or DPI change (techspec RS-02, DP-02). Squarified layout (same algorithm family as `win-go/internal/layout/squarify.go`) is the default.

### Treemap view (`TreemapWidget`)

`QWidget` with `paintEvent`:

- Fill tiles using FS colors from loaded config.
- Label fit algorithm (FS priority: form quality → shortening quality → font size quality):

  1. Binary search heading font between min and max — detailed form.
  2. Shortening at min font — detailed form.
  3. Brief form at min font.
  4. Shortening at min font — brief form.
  5. `treemap.labelDummy`.

- Hit testing for dive-in, context menu (Explore / Open).
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
  1. FS path: `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt`
  2. Legacy: `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt`
- Save: prefer FS path for new installs; document behavior when legacy file was the source.

---

## 5. Settings dialog — FS real grid (critical subsystem)

This is the primary reason for the Qt migration. The design must pass FS Settings Form and techspec SG rows.

### Control strategy declaration (required before coding)

Record in `docs/verification/settings-grid-qt-strategy.md`:

> We use `QTableView` + `QAbstractTableModel` with **permanent** cell widgets attached through `setIndexWidget` for every editable value cell. Parameter names are read-only labels in column 0. Column 1 hosts a row-specific composite widget (text, spin, combo, or color row). Editors are created when the dialog opens and remain visible until it closes. We do not use activation-only delegate editing, `SysListView32` overlays, detached panels, or per-row independent layouts.

### Recommended layout

| Column | Content |
|--------|---------|
| 0 | Parameter name (`QLabel` in read-only presentation) |
| 1 | Value cell — permanent editor widget |

**Row count:** one per user-editable FS treemap parameter (32 rows in current FS table).

### Editor mapping

| FS type | Qt widget |
|---------|-----------|
| Integer | `QSpinBox` or `QLineEdit` with `QIntValidator` |
| Float / ratio | `QDoubleSpinBox` with decimals per parameter |
| Percentage (`treemap.clumpThreshold`) | `QDoubleSpinBox` storing fraction 0–1 (display as decimal; FS allows `%` in file but editors use numeric fraction internally) |
| TSize | `QLineEdit` with validator for `pt`/`px`/`mm`/`cm`/`in` suffix |
| String | `QLineEdit` |
| Color | `QWidget` row: `QLineEdit` (`#RRGGBB`) + `QLabel` swatch + `QToolButton` "…" → `QColorDialog` |
| `treemap.tileFontName` | editable `QComboBox` filled from `QFontDatabase::families()` |

Color swatch: `QLabel` with `setStyleSheet` or `QPalette` background; update on every valid color change and on Reset.

### Validation and actions

Mirror `win-go/internal/ui/validation.go` rules:

- Field validation on commit (focus loss / Enter).
- `validateAll` + `validateObject` stub before save.
- Inline error `QLabel` below grid (red); no `QMessageBox` for field validation (FS-aligned UX).
- `QMessageBox` only for Cancel-with-dirty confirmation and catastrophic I/O.

Buttons:

- **Reset Treemap Defaults** — reload UI from built-in defaults; no disk write until Apply/OK.
- **Apply** — validate all → save → apply to live treemap config → keep dialog open.
- **OK** — Apply then accept.
- **Cancel** — confirm if dirty.

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

## 9. Build and release

- **CMake** + **MSVC** + **Qt 6**.
- `build.ps1`: bump version resource, `cmake --build`, copy to `bin/win/current`, append `docs/history/builds.txt` (same discipline as `win-go/build.ps1`).
- `windeployqt` in installer pipeline; Inno Setup script can reuse `installer/Erase & Rewrite.iss` with updated `SourceDir`.
- Icon pipeline: reuse `codebase/assets/icons` SVG → ICO/PNG generation (port `genicons` or use `rasterize` in CMake).

---

## 10. Testing hints

| Area | Approach |
|------|----------|
| FS use cases | Manual scripts keyed to FS headings |
| Techspec IO/DPI/RS | Fixtures + VM screenshots |
| Settings grid SG | Manual FS definition-of-done checklist on clean VM |
| Config CF | Round-trip tests: parse `win-go/dist/Erase & Rewrite.config.txt`, save, reload |
| Scanner | Unit tests for squarify, packing, share math (no Qt GUI needed) |
| UX-01 | Automated or manual: Update → cancel → prior snapshot visible |

Priority test: **settings grid manual pass** before declaring Go target retired.


## 11. Relationship to Go implementation

| Go module (`win-go/`) | Qt port source |
|-----------------------|----------------|
| `internal/model` | `treemap/` types |
| `internal/scan` | `scan/` |
| `internal/layout/squarify.go` | `TreemapLayout` |
| `internal/config` | `config/` (format 1:1) |
| `internal/ui/run_windows.go` | `MainWindow` + `TreemapWidget` |
| `internal/ui/row_schema.go` etc. | `SettingsDialog` model |
| `internal/ui/custom_grid_windows.go` | **Do not port** — replace with Qt permanent cell widgets |

Behavioral parity is the goal; line-by-line port is not required.


## 12. Non-goals

- Linux or macOS Qt builds (FS allows parameters for other OSes; this arch is Windows-only).
- Embedding web views for settings or treemap.
- Changing FS config key names or treemap semantics.
- Maintaining two active Windows delivery lines after Qt FS verification is complete.
