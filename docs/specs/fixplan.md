# Erase & Rewrite: Qt Implementation Fix Plan

## About This Document

### Identification

Document ID: FIXPLAN-WIN-CPP-QT.

Genre: engineering fix plan and verification roadmap.

Module: `win-cpp-qt/`.


### Purpose

This document records how the open implementation defects from [dispute.md](./dispute.md) findings 22–46 will be corrected. It turns the agreed reviewer priority order into concrete code changes, file targets, data-model decisions, and test gates.

This plan does not change product meaning. [funcspec.md](./funcspec.md) remains owner-edited and is not modified by this document.


### Basis

The following dispute sections are authoritative for scope:

- Strict C++/Qt implementation review (2026-07-11), findings 22–46.
- Developer reply to strict implementation review (2026-07-11).
- Reviewer reply to developer implementation response (2026-07-11).

Cross-reference: [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §15 lists current compliance gaps.


### Target audience

Developers and reviewers working on the Windows Qt delivery line.


### Release gate

No finding 22–46 closes until code, governing documents where applicable, and verification evidence agree. The Qt module must not be declared the active Windows delivery line until this plan is executed and a fresh FS-to-code review passes.


### Prose standard

This document follows [XMSTP](https://github.com/aka-author/xmstp).


## Terms

*Finding:* a numbered defect in dispute.md §22–46.

*Traversal state:* scanner knowledge about whether a folder was read completely, could not be read, or is an untraversed reparse entry. Traversal state is orthogonal to FS tree role.

*Scan result:* typed worker output including identity fields, outcome enum, diagnostics, and optional descriptor payload.

*Passthrough config document:* parsed configuration that preserves unknown lines and comments while rewriting only recognized keys.


## Priority overview

The table below lists execution phases in dependency order. Hard gates apply only where noted; preparation work may proceed in parallel.

| Phase | Scope | Findings | Hard gate before implementation |
|-------|-------|----------|--------------------------------|
| 1 | Scanner I/O, traversal state, typed scan results | 23, 24, 25, 39, 40 | none (start here) |
| 2 | Update navigation and result publication | 22, 39 | Phase 1 typed `ScanResult` and identity fields |
| 3 | Archive catalog reader | 26, 27 | may run proof matrix in parallel with Phase 1 |
| 4 | Treemap projection and layout | 28, 29, 37, 38 | stable projection contract from Phase 1 descriptor model |
| 5 | Descriptor timestamps | 30 | Phase 1 `DirEntry` native timestamps |
| 6 | Configuration parsing and persistence | 31–36 | may run parser unit tests in parallel with Phases 1–4 |
| 7 | Version, runtime, Settings evidence, full regression | 42–46, 43–45 | may run version generator work in parallel; release sign-off requires all phases |

Parallel work allowed without waiting for prior phase exit: archive-library proof, test-harness setup, `versioninfo.json` generator design, configuration parser unit tests.

Release sign-off requires every phase exit criteria, including tests, doc updates, and code inspection.


---

## Phase 1 — Scanner foundation

### 1.1 Goals

Replace false timeout semantics, distinguish unreadable directories from empty ones, model reparse skips without misusing `TreeRole::EmptyFolder`, and deliver typed scan outcomes with identity validation.


### 1.2 New types

Test framework: **Qt Test** (`QTest`). Runner: `ctest` from the CMake build directory. Record command in `win-cpp-qt/CMakeLists.txt`.

Add `scan/ScanTypes.h` with the following types.

```cpp
enum class ScanOutcome {
    Invalid,           // default; fail closed
    Success,
    Cancelled,
    RootUnavailable,
    TechnicalFailure,
};

enum class DirectoryReadStatus {
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

struct DirEntry {
    QString name;
    QString fullPath;
    DWORD attributes = 0;
    DWORD reparseTag = 0;
    qint64 size = 0;
    FILETIME creationTime{};
    FILETIME lastWriteTime{};
    bool isDirectory = false;
    bool isReparsePoint = false;
};

struct DirectoryReadResult {
    QVector<DirEntry> entries;
    DirectoryReadStatus status = DirectoryReadStatus::Ok;
    DWORD nativeError = 0;
};

struct ScanDiagnostics {
    int unreadableDirectoryCount = 0;
    int reparseNotTraversedCount = 0;
};

struct ScanResult {
    quint64 scanId = 0;
    quint64 targetSessionId = 0;
    ScanKind scanKind = ScanKind::OpenTarget;
    QString scanRootPath;
    quint64 baseDescriptorVersion = 0;
    ScanOutcome outcome = ScanOutcome::Invalid;
    ScanDiagnostics diagnostics;
    std::optional<model::FolderDescriptor> tree;  // only for Success
};
```

`ScanResult` must be constructed through named factories (`ScanResult::success(...)`, `ScanResult::cancelled(...)`, etc.). No default success. Cancelled and failed results must not carry a descriptor.

Extend `model/FolderDescriptor.h`:

- add `TraversalState traversalState` on folder nodes;
- keep `TreeRole` for FS display semantics only;
- add `std::optional<QDateTime>` for oldest/newest file fields (phase 5 wires birth time from `DirEntry`);
- drop `reparseSkipped` once `TraversalState` is wired.


### 1.3 Win32 directory enumeration

Delete `ScanWorker::readDirBounded()` and its `std::async` usage.

Add `platform/WinDirEnum.{h,cpp}` and `platform/DirEntry.h`:

- enumerate with `FindFirstFileExW` / `FindNextFileW`;
- skip `.` and `..`;
- populate immutable `DirEntry` from `WIN32_FIND_DATAW` (attributes, reparse tag, 64-bit size, creation and last-write times);
- check `m_cancelled` between entries;
- close handles through RAII on all paths;
- return `DirectoryReadResult` with `DirectoryReadStatus` and `GetLastError()` on failure.

Do not reconstruct `QFileInfo` per entry. Use `QFileInfo` only where a later operation genuinely requires it.

Cancellation boundary (document in [../verification/io-01-scan-boundary.md](../verification/io-01-scan-boundary.md)):

- cooperative cancel is checked between returned entries;
- an individual syscall may block until the OS returns;
- no claim of a guaranteed 30 s wall-clock bound per directory unless an external process isolation model is added later.


### 1.4 `scanDir()` semantics

In `scan/ScanWorker.cpp`:

| Situation | `traversalState` | `treeRole` | Size |
|-----------|------------------|------------|------|
| Enumeration succeeded, no children | `Complete` | `EmptyFolder` | 0 aggregate |
| Enumeration failed | `Unreadable` | not `EmptyFolder` | do not assert emptiness |
| Directory reparse point | `ReparseTargetNotTraversed` | not `EmptyFolder` | 0 for v1 (linked target excluded) |

Increment `diagnostics.unreadableDirectoryCount` only for failed enumeration. Increment `diagnostics.reparseNotTraversedCount` only for intentional reparse skips. Do not conflate the two.

Remove outcome heuristics:

- delete `skipReason.contains("timed out")`;
- delete empty-tree plus non-empty-skip-reason inference for `rootUnavailable`;
- set `ScanOutcome` explicitly in the worker.


### 1.5 Aggregate recomputation

In `model/FolderDescriptor.cpp` and `scan/SubtreeMerge.cpp`:

- `recomputeAggregates()` must not downgrade `Unreadable` or `ReparseTargetNotTraversed` to `EmptyFolder`;
- empty child lists after a failed read must not imply `EmptyFolder`.


### 1.6 Result delivery and stale-result guard

Change `ScanWorker::finished` signal to emit `ScanResult` (or equivalent struct).

In `app/MainWindow.cpp` `onScanFinished`:

0. at the first line, validate `scanId`, `targetSessionId`, and `baseDescriptorVersion` against the snapshot captured at `startScan()`; if stale, return immediately with no session, status, chrome, progress, error, or descriptor change;
1. only then clear worker state and map `ScanOutcome` to FS alerts;
2. publish trees only when `outcome == Success` and `tree` is engaged.

Worker/thread cleanup may proceed independently; stale completion must be observationally inert for the active session.


### 1.7 Phase 1 tests

Add `win-cpp-qt/tests/` using Qt Test with fixtures:

| Test | Proves |
|------|--------|
| `denied_inner_dir_not_empty_folder` | unreadable child is not `EmptyFolder` |
| `root_access_denied` | root failure is `RootUnavailable`, not empty tree |
| `root_deleted_before_enum` | typed root outcome |
| `empty_dir_is_empty_folder` | successful enumeration with zero entries |
| `reparse_entry_traversal_state` | reparse → `ReparseTargetNotTraversed`, size 0 |
| `reparse_target_nonempty_excluded` | linked target contents not nested in descriptor |
| `scan_result_stale_scan_id` | stale `scanId` does not mutate session |
| `scan_result_stale_session_id` | matching `scanId`, wrong `targetSessionId` ignored |
| `scan_result_stale_descriptor_version` | wrong `baseDescriptorVersion` ignored |
| `cancel_before_first_entry` | cooperative cancel |
| `cancel_after_several_entries` | cancel mid-enumeration |
| `cancel_between_entries` | cancel flag stops further enumeration |
| `native_handle_closed_on_cancel` | RAII closes search handle |

Supplement with manual denied-ACL fixture on a local volume.


### 1.8 Phase 1 doc updates

After code lands:

- update [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §6 and §15;
- add [../verification/io-03-reparse-policy.md](../verification/io-03-reparse-policy.md) with v1 size rule;
- remove any text claiming a 30 s per-directory guarantee.


### 1.9 Phase 1 exit criteria

- findings 23, 24, 25, 39, 40 marked closed in impl §15;
- all phase 1 tests pass via `ctest`;
- impl §6 and verification notes updated;
- code inspection confirms removal of `std::async` read path, string-heuristic outcomes, and `EmptyFolder` misuse;
- evidence links recorded in dispute.md.


---

## Phase 2 — Update navigation

### 2.1 Goals

Restore FS-permitted navigation during `UpdateContext` while a scan runs, without allowing a second scan or reading a partially built tree.


### 2.2 Chrome rules

In `MainWindow::updateChrome()`, `onUp()`, `onDive()`, and Explore handlers:

| Action | `OpenTarget` scanning | `UpdateContext` scanning | Idle |
|--------|----------------------|------------------------|------|
| Open | disabled | disabled | enabled |
| Update | disabled | disabled | enabled if complete |
| Stop | enabled | enabled | hidden |
| Up | disabled | enabled if `canGoUp()` | enabled if `canGoUp()` |
| Dive | disabled | enabled on published tree | enabled |
| Explore | disabled | enabled | enabled |
| Settings | disabled | disabled | enabled |

Treemap reads `m_session.publishedTree` during update scan, never the in-flight worker tree.


### 2.3 Publish path

On successful `UpdateContext` completion:

1. validate `ScanResult` identity at callback entry;
2. call `mergeSubtree(publishedSnapshot, scanRoot, newSubtree)` off the hot path if needed, but prepare a complete new session snapshot first (merged tree, incremented descriptor version, resolved `contextPath`);
3. if `contextPath` no longer exists in the merged tree, follow FS error path #004 for the missing context folder and restore or unset per scan kind — do not leave an unresolved path;
4. publish the snapshot in one GUI-thread assignment (`publishedTree`, `contextPath`, `descriptorVersion`, `treemapComplete`, clear `pendingUpdateSnapshot`);
5. rebuild treemap once from the published snapshot.

No observer may see a new tree with an old descriptor version, stale context, or mismatched action state.


### 2.4 Phase 2 tests

| Test | Proves |
|------|--------|
| `update_allows_up_during_scan` | Up changes context while scanning |
| `update_blocks_open` | Open disabled during update scan |
| `update_publish_preserves_context` | context path valid after merge |
| `stale_update_result_ignored` | wrong session id does not publish |


### 2.5 Phase 2 exit criteria

- finding 22 closed;
- phase 1 identity checks still pass under navigation stress.


---

## Phase 3 — Archive catalog reader

### 3.1 Goals

Remove the handwritten ZIP central-directory parser. Classify ZIP and RAR archives from catalog inspection with bounded, non-extracting reads.


### 3.2 Interface

Add `scan/IArchiveCatalogReader.h`:

```cpp
enum class CatalogReadOutcome {
    Readable,
    EncryptedOrUnavailable,
    UnsupportedFormat,
    Corrupt,
    ResourceLimit,
    Cancelled,
    IoFailure,
};

struct ArchiveCatalogEntry {
    QString path;       // normalized relative path
    bool isDirectory;
};

struct CatalogReadResult {
    CatalogReadOutcome outcome = CatalogReadOutcome::IoFailure;
    QVector<ArchiveCatalogEntry> entries;
    DWORD nativeError = 0;
};

class IArchiveCatalogReader {
public:
    virtual ~IArchiveCatalogReader() = default;
    virtual CatalogReadResult readCatalog(const QString& path,
                                          const std::atomic_bool& cancelToken) = 0;
};
```

Limits enforced before trusting data:

- max entry count (start with 100 000, tune from proof matrix);
- max total name bytes;
- max single path length;
- explicit `cancelToken` checked during traversal;
- no extraction and no filesystem object creation.

Unsafe path components (`..`, absolute roots, drive prefixes, equivalent traversal forms): **ignore the entry for top-level classification**; if no safe effective entries remain, outcome is `Corrupt` or `Readable` with empty safe set → `PackedClump` per contract. Document this rule in `archive-library-decision.md` and test mixed safe/unsafe catalogs.


### 3.3 Library selection

Run proof matrix from dispute §6 on candidates (libarchive primary). Record pin in `CMakeLists.txt`, licence in `docs/verification/archive-library-decision.md`, and techspec row when pinned.

Delete `readZipCentralDirectory()` from `ArchiveClassifier.cpp`. Implement `LibarchiveCatalogReader` (or chosen backend) behind the interface.


### 3.4 Classification contract

FS conformance rules (Go code is a regression reference only, not the normative baseline):

| Catalog read outcome | `PackingType` |
|----------------------|---------------|
| `Readable`, one effective top-level file | `PackedFile` |
| `Readable`, one effective top-level folder | `PackedFolder` |
| `Readable`, otherwise | `PackedClump` |
| `EncryptedOrUnavailable`, `UnsupportedFormat`, `Corrupt`, `ResourceLimit`, `Cancelled`, `IoFailure` | `PackedClump` fallback |

`.rar` must attempt catalog read; `PackedClump` only on defined failure paths.


### 3.5 Phase 3 tests

Fixture archives under `win-cpp-qt/tests/fixtures/archives/` covering dispute matrix (plain ZIP, ZIP folder, multi-entry, empty, encrypted, corrupt, RAR, RAR5, multipart, traversal names, large catalog metadata only).

Each fixture asserts expected `PackingType`.


### 3.6 Phase 3 exit criteria

- findings 26 and 27 closed;
- decision record committed;
- scanner depends only on `IArchiveCatalogReader`, not libarchive types.


---

## Phase 4 — Treemap projection and layout

### 4.1 Goals

Exclude invalid sizes before layout, implement minimum-dimension clumping with termination guarantees, replace or rename the layout algorithm honestly, and eliminate signed overflow in area math.


### 4.2 Projection (`treemap/TreemapProjection.cpp`)

Implement nine-step deterministic algorithm from dispute §5:

1. direct children of context folder;
2. drop zero, negative, and non-finite sizes;
3. threshold clump candidates;
4. sort by descending size, tie-break normalized full path;
5. decide if clump tile required;
6. retain at most `maxTiles - 1` regular entries when clump required;
7. else retain at most `maxTiles`;
8. compute clump aggregate size and share;
9. pass flat vector to layout.

Define clump interaction: no Dive, Explore, or Open as a single filesystem object; cursor `default` or `not-allowed`; empty or absent context menu unless FS clarifies.


### 4.3 Minimum-dimension merge cycle

After initial projection, before final layout:

1. lay out preliminary proportional rectangles;
2. convert configured `minTileWidth` / `minTileHeight` `TSize` values to Qt **logical** pixels for the treemap viewport (same coordinate system as widget geometry; do not compare raw physical pixels or double-apply device pixel ratio);
3. detect tiles below minima in that logical space;
4. merge undersized represented values into the single clump;
4. recompute areas;
5. repeat until stable or no merge possible.

Termination rules (reviewer qualification):

- handle `maxTiles == 1`;
- handle all-regular undersized;
- handle clump itself below minimum when viewport too small;
- detect repeated identical merge sets;
- each iteration strictly reduces regular tile count or stops.

Implement in `TreemapProjection.cpp` (merge decisions) plus `TreemapLayout.cpp` (geometry).


### 4.4 Layout algorithm (`treemap/TreemapLayout.cpp`)

Replace binary partition with Bruls–Huizing–van Wijk squarify. Use `win-go/internal/layout/squarify.go` only as an implementation reference. Acceptance is against FS layout properties and tests, not rectangle-for-rectangle parity with Go.

Remove `max64(it.size, 1)` weight invention.

Use `double` weights normalized to sum 1.0 for layout ratios, or `uint64_t` with overflow checks on products.


### 4.5 Phase 4 tests

| Test | Proves |
|------|--------|
| `zero_size_excluded` | zero-byte file absent from tiles |
| `min_tile_clump_merge` | undersized tiles merge into clump |
| `max_tiles_one` | single tile path terminates |
| `area_sum_conserved` | tile areas sum to viewport within tolerance |
| `no_overlap_no_gaps` | rectangles partition parent |
| `layout_deterministic` | same input → same rectangles |
| `aspect_ratio_fixture` | records max/percentile aspect ratio for FS "should" tracking, not a hard 5:1 gate |


### 4.6 Phase 4 exit criteria

- findings 28, 29, 37, 38 closed;
- `squarify()` name matches algorithm or is renamed with tests.


---

## Phase 5 — Descriptor timestamps

### 5.1 Goals

Oldest file uses creation time; newest file uses last-write time; absent creation propagates as `std::optional`.


### 5.2 Implementation

In `ScanWorker.cpp` file nodes, populate from `DirEntry` captured at enumeration:

- `oldestFile`: creation time when valid, else `std::nullopt` (never substitute last-write for creation);
- `newestFile`: last-write time when valid.

Add `platform/FileTimes.{h,cpp}` to convert `FILETIME` → `QDateTime` when Qt helpers are insufficient.

Update `recomputeAggregates()` to min/max only present optionals.


### 5.3 Phase 5 tests

Fixture tree with known creation and modification times; assert folder aggregates.


### 5.4 Phase 5 exit criteria

- finding 30 closed;
- techspec note on creation-time fallback if any.


---

## Phase 6 — Configuration

### 6.1 Goals

One typed configuration document model, one `TSize` representation, strict validation, passthrough unknown lines, duplicate rejection.


### 6.2 `TSize` type

Add `config/TSize.h`:

```cpp
enum class TSizeUnit { Px, Pt, Mm, Cm, In };

struct TSize {
    double magnitude = 0.0;
    TSizeUnit unit = TSizeUnit::Pt;
};

struct ParseStatus { bool ok = false; QString error; };
```

Validation rules:

- reject NaN, infinity, and negative values where the field disallows them;
- allow zero only where FS allows zero;
- preserve original `magnitude` and `unit` in the document model for round-trip;
- compare normalized values with documented tolerance only for semantic dirty-check, not binary `double` equality.

Shared functions in `config/TSizeParse.cpp`:

- parse from string with all five suffixes and decimals;
- serialize preserving unit unless canonicalization is explicitly selected;
- convert to Qt logical pixels at the render/layout boundary only, per unit:
  - `px` → logical pixel per documented FS/Qt mapping;
  - `pt`, `mm`, `cm`, `in` → physical conversion using the effective DPI of the surface that lays out or paints, without multiplying widget logical coordinates by device pixel ratio again.

Add cross-monitor tests for configured `pt`, `mm`, and `px` values.

Replace `ConfigStore::parsePt()` and duplicate parsing in `SettingsSchema.cpp`.


### 6.3 Document model

Add `config/ConfigDocument.{h,cpp}` as an **ordered line model**:

- each line is a known key assignment, unknown passthrough, comment, or blank;
- associations between comments and the following key are preserved;
- load: reject duplicate known keys before any mutation;
- save: rewrite only recognized keys in place; preserve original key spelling, attached comments, line endings, and final newline where possible;
- unknown and malformed lines remain byte-for-byte unchanged unless a known key on the same line is rewritten;
- validate-then-assign for colors (`QColor` only on success).

Fix zero padding: `if (int n = parsePt(val); n)` → explicit parse status.

Fix percentage: unitless must satisfy `0 < v < 1`; `%` suffix divides by 100.


### 6.4 Phase 6 tests

| Fixture file | Expected |
|--------------|----------|
| `3.5mm`, `12px`, `1cm`, `0.5in` | round-trip |
| `0pt` padding | loads as zero |
| `12.5` clump threshold | rejected |
| `12.5%` | accepted |
| duplicate `treemap.maxtiles` | error |
| unknown `treemap.customflag=1` | preserved after save |
| invalid `#GGGGGG` color | default retained |


### 6.5 Phase 6 exit criteria

- findings 31–36 closed;
- Settings dialog uses same parse/serialize path as file load.


---

## Phase 7 — Version, runtime, Settings evidence

### 7.1 Version unity (finding 44)

Single source: extend `versioninfo.json` schema as canonical four-component version.

Generate from one script invoked by `build.ps1`:

- CMake `PROJECT_VERSION` with tweak component;
- `resources/app.rc`;
- `AppVersion.cpp` readback target;
- installer metadata stub;
- `bin/win/current/build-meta.json`.

Add `verify-version.ps1` comparing numeric quad across PE strings, About, and JSON.


### 7.2 Runtime linkage audit (finding 45)

Run `objdump -p` on `EraseAndRewrite.exe` and each deployed Qt DLL. Store output in `docs/verification/mingw-runtime-deps.txt`.

Decision:

- **prefer** one MinGW runtime linkage model compatible with the Qt distribution and every C++ dependency (including the archive library);
- if mixed static exe + dynamic Qt runtimes remain after inspection, require a written ABI boundary inventory (Qt callbacks, STL objects, exceptions, allocators, locales, archive reader interfaces) in `docs/verification/mingw-runtime-deps.txt`. A two-line “no cross-DLL delete” rule alone is insufficient.


### 7.3 Settings grid evidence (finding 42)

Before SG sign-off, manual and automated checks on current `QTableView` + `setIndexWidget`:

- Tab order through all 32 rows forward and backward;
- focus survives scroll;
- accessibility names on Parameter and Value cells;
- resize keeps column alignment;
- widgets survive full dialog lifetime.

If any check fails, replace with `QGridLayout` in `QScrollArea` per dispute §15.


### 7.4 Dialog geometry (finding 43)

Remove hard 620×520 caps in `SettingsDialog.cpp`. Use `QSizePolicy`, `adjustSize()`, and clamp to available screen geometry from `QScreen`.


### 7.5 Full regression suite (finding 46)

Minimum automated coverage:

- cancellation during deep scan;
- denied vs empty directory;
- reparse descriptor semantics;
- update navigation and publish;
- target deleted during Open;
- context deleted during Update;
- zero-size exclusion;
- clump and min-tile combinations;
- archive classification matrix;
- config malformed/duplicate/unknown cases;
- cross-monitor DPI move (manual with screenshot log);
- About/PE version equality.


### 7.6 Phase 7 exit criteria

- findings 42–46 closed with evidence; **no techspec waiver** for scanner, archive, projection, transaction, or version-consistency defects;
- finding 43 and optional quality risks may be owner-waived only when they do not contradict FS;
- impl §15 compliance table all green;
- fresh FS-to-code review requested in dispute.md;
- each phase includes code inspection that obsolete paths (handwritten ZIP parser, string heuristics, fake timeout claims) are removed.


---

## File change map

The table below lists primary files touched per phase.

| Phase | Files |
|-------|-------|
| 1 | `scan/ScanWorker.*`, `scan/ScanTypes.h`, `platform/WinDirEnum.*`, `platform/DirEntry.h`, `model/FolderDescriptor.*`, `app/MainWindow.*`, `scan/SubtreeMerge.cpp`, `win-cpp-qt/tests/` |
| 2 | `app/MainWindow.*`, `app/Session.*` |
| 3 | `scan/ArchiveClassifier.*`, `scan/IArchiveCatalogReader.h`, `scan/LibarchiveCatalogReader.*`, `CMakeLists.txt` |
| 4 | `treemap/TreemapProjection.*`, `treemap/TreemapLayout.*`, `treemap/TreemapWidget.cpp` |
| 5 | `scan/ScanWorker.cpp`, `platform/FileTimes.*`, `model/FolderDescriptor.cpp` |
| 6 | `config/TSize.*`, `config/ConfigDocument.*`, `config/ConfigStore.*`, `ui/SettingsSchema.*` |
| 7 | `build.ps1`, `versioninfo.json`, `CMakeLists.txt`, `ui/SettingsDialog.*`, `test-run.ps1`, `docs/verification/*` |


---

## What not to do

- Do not edit `funcspec.md` from agent workflows.
- Do not add UI polish tasks ahead of phase 4.
- Do not claim bounded directory I/O without a real enforcement mechanism.
- Do not close dispute findings in prose only; link tests or verification artifacts.
- Do not declare the Go target retired until this plan completes and SG manual gate passes.


---

## Tracking

Update [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §15 as each finding closes.

Add a short entry to dispute.md only when a phase completes and evidence exists; do not amend reviewer sections.

When all phases complete, request a fresh FS-to-code review and owner sign-off on declaring `win-cpp-qt/` the active Windows delivery line.

---

## Reviewer comments on fix plan (2026-07-11)

### Overall verdict

The plan is substantially sound and follows the accepted priority order. It is good enough to guide the correction work, but several details must be tightened before implementation starts. Otherwise the team can follow the plan literally and still reproduce some of the same defects under different names.

### 1. Phase ordering is too absolute

The statement that later phases must not start until all prior phase acceptance tests pass is unnecessarily rigid. Some preparation can safely proceed in parallel, especially archive-library proof work, version-source generation, test-harness setup, and configuration parser unit tests.

Keep the dependency gates where they are real:

- Phase 2 publication work depends on Phase 1 typed results and identity fields.
- Phase 4 minimum-size clumping depends on a stable projection contract.
- Release sign-off depends on all phases.

Do not prohibit independent proof-of-capability and test-fixture work merely because an earlier implementation phase is still open.

### 2. `ScanResult` must not default to success

The proposed type initializes `ScanOutcome outcome = ScanOutcome::Success`. That is a dangerous default for a result that may be partially constructed or returned from an exceptional path.

Use an explicit invalid/unset state or require construction through named factories. A missing assignment must fail closed, not silently become success.

Also, `FolderDescriptor tree` should be optional or variant-bound to outcomes that actually carry a descriptor. Cancelled and failed results must not contain an accidentally default-constructed tree that later code could publish.

### 3. `unreadEntryCount` conflates unreadable entries and reparse skips

Section 1.4 says to increment `unreadEntryCount` for unreadable directories and reparse skips “as appropriate.” These are not the same condition:

- unreadable means traversal was attempted and failed;
- reparse target not traversed is intentional policy, not an access failure.

Use separate structured diagnostics or counters. Do not let support logs, tests, or later UI work report reparse entries as unreadable.

### 4. Directory enumeration should return a native entry type, not reconstruct `QFileInfo`

`WIN32_FIND_DATAW` already contains the attributes, file size, and timestamps needed by the scanner. Converting each entry back into `QFileInfo` can trigger additional filesystem queries, lose the exact native error boundary, and undermine the point of replacing `QDir` enumeration.

Prefer a thin immutable `DirEntry` containing:

- name and full path;
- attributes and reparse tag where available;
- 64-bit size;
- creation and last-write timestamps;
- directory/reparse flags.

Construct higher-level descriptor nodes from this native snapshot. Use `QFileInfo` only where an operation genuinely requires it.

### 5. Stale-result rejection must occur before every externally visible mutation

Section 1.6 says stale results cause no status, chrome, or descriptor change, which is correct. Make the implementation rule stronger: identity validation must happen at the first line of the completion callback, before clearing the active worker, changing progress state, hiding Stop, showing errors, or updating diagnostics attached to the current session.

Worker/thread resource cleanup may occur independently, but stale completion must be observationally inert for the active session.

### 6. Phase 1 tests are incomplete for the stated scanner contract

Add automated or controlled fixtures for:

- root access denied versus inner access denied;
- root deleted before enumeration;
- cancellation before first entry and after several entries;
- stale result with matching `scanId` but wrong `targetSessionId`;
- stale result with correct session but wrong base descriptor version;
- native handle closure on cancellation and failure;
- a directory reparse point whose target is non-empty, proving target contents are excluded.

The manual denied-ACL fixture is useful, but it cannot be the only proof of unreadable-directory semantics.

### 7. Phase 2 publish wording is not atomic enough

The sequence says to replace `publishedTree` and clear `pendingUpdateSnapshot`, then rebuild the treemap. Treat the entire publication as one session-state transaction. No event handler should observe a new tree with an old descriptor version, old context resolution, or stale action state.

Prepare the complete new session snapshot first, including incremented descriptor version and resolved context, then publish it in one GUI-thread operation and rebuild from that snapshot.

Also define what happens when the current context path disappeared during the update. The plan should name the FS-defined fallback or error path rather than merely saying “resolve contextPath.”

### 8. Archive API must return typed outcomes, not `bool` plus error string

The proposed `IArchiveCatalogReader::readCatalog(..., QString* error)` repeats the same design error already rejected in scanner outcomes. Classification needs to distinguish at least:

- readable catalog;
- encrypted/unavailable catalog;
- unsupported format/variant;
- corrupt catalog;
- resource-limit breach;
- cancelled;
- I/O failure.

Return a typed result with structured diagnostics. Presentation strings do not belong in the archive reader.

The cancellation token must be an explicit input to the interface, not an implied global checked every N entries.

### 9. Archive path handling should not say “reject” without defining classification behavior

The plan says to reject traversal-style names. Since the scanner is not extracting, “reject” is ambiguous. Define whether one unsafe entry makes the catalog unavailable and therefore `PackedClump`, or whether unsafe entries are ignored for effective top-level classification.

Choose one deterministic rule and test mixed catalogs containing both safe and unsafe names. Do not let backend-specific behavior decide this.

### 10. Do not call the Go implementation a behavioral baseline

Section 4.4 says to port from the Go implementation “as the behavioral baseline.” The dispute explicitly replaced behavioral parity with FS conformance, using Go only for regression comparison where useful.

Use the Go code as an implementation reference, not the normative baseline. The new algorithm must be accepted against FS properties and tests even if its rectangle order differs from Go.

### 11. Minimum-tile dimensions are in Qt logical coordinates, not raw device pixels

Section 4.3 says to detect undersized tiles in device pixels. The FS `TSize` values and Qt widget geometry should be compared in one documented logical coordinate system. In a Qt 6 widget application, layout geometry is normally device-independent.

Convert configured `TSize` values to the same logical units used by the treemap viewport, then compare external rectangles there. Raw physical-pixel comparison will make clumping change incorrectly across device-pixel ratios.

### 12. The `TSize` DPI rule is currently wrong for Qt layout

Section 6.2 says conversion to device-independent pixels uses screen DPI. Clarify each unit separately:

- `px` should map to the chosen FS/Qt logical-pixel interpretation;
- `pt`, `mm`, `cm`, and `in` require a documented physical/logical conversion policy;
- Qt widget coordinates remain device-independent and must not be multiplied again by device pixel ratio.

Do not use a generic “screen DPI” conversion that changes layout unpredictably between monitors or double-applies scaling. Add cross-monitor tests for configured `pt`, `mm`, and `px` values.

### 13. `double` is acceptable internally only with explicit validation and equality rules

The proposed `TSize` uses `double magnitude`. Require finite, non-negative values where the corresponding field allows zero, and reject NaN/infinity before storing the typed configuration.

Define comparison and serialization behavior. Exact binary equality is unsuitable for dirty-state detection after unit conversion. Prefer preserving the original typed magnitude/unit and comparing normalized values with a documented tolerance only where semantic comparison is needed.

### 14. Passthrough configuration preservation needs a precise rewrite algorithm

“Rewrite known keys in place or append section” is too loose. Define:

- whether original spelling and whitespace of known keys are preserved;
- how comments attached to known keys are retained;
- where missing known keys are inserted;
- whether line endings and final newline are preserved;
- how duplicate known keys fail without rewriting anything;
- whether unknown malformed lines remain byte-for-byte unchanged.

The safest design is an ordered token/document model, not separate maps and vectors that can lose association between comments and fields.

### 15. Phase 7 cannot waive release blockers with a techspec row

The exit criterion “findings 42–46 closed or explicitly waived with techspec row” is too broad. A technical specification cannot waive a defect that contradicts the FS, and it cannot substitute for required runtime evidence.

In particular:

- finding 44 requires version consistency;
- finding 46 is the verification gate;
- scanner, archive, projection, and transaction tests cannot be waived by prose.

Only optional quality risks that do not contradict FS may be accepted explicitly by the owner. Replace the blanket waiver clause with per-finding closure criteria and owner approval where genuinely applicable.

### 16. Runtime boundary rules are not a satisfactory preferred solution

Section 7.2 suggests documenting “no cross-DLL `new`/`delete`, no exceptions across boundaries” if mixed runtime instances remain. That is a mitigation, not proof that the Qt and plugin/library boundaries are safe.

Prefer a single compatible MinGW runtime model. If mixed runtime linkage remains, require a concrete ABI boundary inventory covering Qt callbacks, STL objects, exceptions, allocators, locales, and third-party archive-library interfaces. A two-line rule is insufficient.

### 17. Test framework must be selected, not left as “GoogleTest or Qt Test”

The plan should make one test-framework decision before Phase 1 implementation. For a Qt application, Qt Test is the simplest default unless existing infrastructure strongly favors GoogleTest.

Leaving the choice open weakens file layout, CI integration, fixture design, and ownership. Record one selection and its runner command.

### 18. Exit criteria must require fresh code inspection, not only green tests

Tests can prove selected cases but cannot prove that obsolete branches, string heuristics, handwritten archive parsing, or unused configuration paths were actually removed.

Each phase exit should include:

- tests passing;
- relevant governing documents updated;
- implementation diff reviewed against the finding;
- no old contradictory path remains reachable;
- evidence links recorded.

### Final disposition

The fix plan is accepted as the working roadmap after the corrections above are incorporated. Its overall priority order is correct. The main required changes are typed failure-safe APIs, clearer transaction publication, a native directory-entry model, correct Qt/TSize coordinate semantics, precise config passthrough rules, and non-waivable closure criteria for release blockers.

Do not start bulk implementation from the current text without resolving items 2, 3, 8, 10, 11, 12, and 15. Those affect core data models and acceptance rules and would otherwise cause avoidable rework.

---

## Plan revision — developer incorporation (2026-07-11)

This section records how the body of this document was updated after **Reviewer comments on fix plan (2026-07-11)** above. The reviewer section is left unchanged.

### Disposition

The reviewer comments are accepted. The corrections below are incorporated into the phase sections preceding the reviewer appendix. Bulk Phase 1 implementation may start once items 2, 3, 8, 10, 11, 12, and 15 from the reviewer list are reflected in code scaffolding — they are now reflected in this plan text.

### Incorporation map

The table below maps each reviewer comment to the plan section updated.

| Reviewer item | Incorporated change |
|---------------|---------------------|
| 1. Phase ordering too absolute | Priority overview now lists hard gates vs parallel prep work |
| 2. `ScanResult` must not default to success | `ScanOutcome::Invalid`, factory construction, `std::optional` tree |
| 3. Separate unreadable vs reparse counters | `ScanDiagnostics` with two counters |
| 4. Native `DirEntry`, not `QFileInfo` | `platform/DirEntry.h`, enumeration from `WIN32_FIND_DATAW` |
| 5. Stale result before any mutation | §1.6 requires validation at first line of `onScanFinished` |
| 6. Expanded Phase 1 tests | root denied, root deleted, stale session/version, handle closure, reparse target |
| 7. Atomic publish | §2.3 single snapshot publish; FS #004 when context missing |
| 8. Typed archive outcomes | `CatalogReadResult`, explicit `cancelToken` parameter |
| 9. Unsafe archive path rule | ignore unsafe entries for classification; test mixed catalogs |
| 10. Go not normative baseline | §3.4 and §4.4 reference Go for regression only |
| 11. Min tile in logical coordinates | §4.3 compares in Qt logical space |
| 12. `TSize` DPI policy per unit | §6.2 unit-specific conversion; cross-monitor tests |
| 13. `double` validation rules | §6.2 finite/non-negative rules; no binary dirty equality |
| 14. Precise passthrough rewrite | §6.3 ordered line model with byte-preservation rules |
| 15. No blanket waiver | §7.6 release blockers non-waivable |
| 16. Runtime ABI inventory | §7.2 prefers single runtime; inventory if mixed |
| 17. Test framework | Qt Test + `ctest` recorded in §1.2 |
| 18. Code inspection per phase | §1.9 and §7.6 require diff review and obsolete-path removal |

### Pre-implementation checklist

Before merging the first Phase 1 code PR:

1. land `scan/ScanTypes.h` with `Invalid` outcome and optional tree;
2. land `platform/DirEntry.h` stub used by `WinDirEnum`;
3. land `IArchiveCatalogReader.h` with typed `CatalogReadResult` (may be unused until Phase 3);
4. land Qt Test harness and one failing fixture (`denied_inner_dir_not_empty_folder`);
5. update impl §15 to note plan revision commit.

### Next step

Execute Phase 1 per revised §1. Record evidence under `docs/verification/` and close dispute findings only when tests and inspection pass.

