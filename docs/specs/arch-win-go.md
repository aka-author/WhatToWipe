# Erase & Rewrite: Architecture (Windows, Go)

Document type: this is an informal architecture description for implementers. It is not a specification.

Normative stack:

1. [funcspec.md](./funcspec.md) (FS) defines all product behavior, UI text, and acceptance tests.
2. [techspec-win-go.md](./techspec-win-go.md) adds Windows, Go, PE, DPI, I/O, responsiveness, memory, and accessibility baselines that FS does not spell out.
3. This file suggests how to structure code and workflows so the first two stay satisfied. If anything here disagrees with FS or the technical specification, fix this file.

## 1. Scope

The architecture targets one interactive Windows amd64 executable built with Go, as described in techspec Р’В§5. It assumes a single process with a UI thread and background work for scanning, not a client-server split.

## 2. Binding FS and techspec into design

- FS tells you what the program must do; it does not name Win32 APIs, goroutines, or layout algorithms.
- Techspec tells you deliverable shape, PE fields, DPI changes, long paths, reparse handling, timeouts, cancellation responsiveness, memory guardrails, accessibility, Update-cancel behavior, and verification rules (techspec Р’В§5РІР‚вЂњР’В§13). Design choices should cite those rows when you verify a build.

Keep a short internal map from FS sections to packages or files. That is hygiene, not part of this document.

## 3. Subsystems

Logical roles only; package names are up to you.

### Application shell

Main window, menu bar, status bar, treemap host, and a **command strip** above the treemap. Dispatches commands. Behavior still comes only from FS.

#### Command strip (not `ToolbarWindow32`)

The product does **not** use the Win32 rebar **`ToolbarWindow32`** band for Open / Up / Explore / UpdateРІР‚вЂњStop / Total / Free. Reasons: Total must read as **static text** (`Label`), Free as a **real push button**, and the icon commands need predictable layout with the treemapРІР‚вЂќwithout fighting toolbar button semantics (mixed text-only and image-only items).

Implementation shape (under `win-go/internal/ui`, `walk` declarative shell):

- **`MainWindow`** is built **without** declarative `ToolBar` items. The vendored `walk` `declarative` `MainWindow` path **disposes** the default empty toolbar control so the client area is **menu + content only** (no blank strip).
- The **first child** of the main client `VBox` is a horizontal **`Composite`**: **`ToolButton`**s for Open, Up, Explore, and Update/Stop (bitmaps from the existing art pipeline), then a **`Label`** for Total, then a **`PushButton`** for Free, then a spacer. The treemap **`CustomWidget`** follows with stretch.
- **Menu** actions remain the source of truth for shortcuts and Inspect/File entries; **`setScanChrome`** keeps menu **`Action`** enabled/visible state in sync with the strip widgets (and drives the scan button image/tooltip between Update and Stop).

Vendored `walk` (under `win-go/third_party/walk`): besides the empty-toolbar disposal above, **`ToolButton.CreateLayoutItem`** was adjusted so layout height/width respect **bitmap size** and declarative **`MinSize`**, avoiding vertical clipping of 24Р“вЂ”24-at-96-DPI-style glyphs in an `HBox`.

If FS wording still says РІР‚СљtoolbarРІР‚Сњ for those elements, treat that as the **command affordances** in this strip; the FS table is the behavioral contract, not the Win32 control class.

### Session

Target path, navigation stack within the scanned tree, scanning flag, cancel handle, and flags needed to reflect FS treemap states complete and unset. Do not invent alternate meanings for those states.

### Scanner

Filesystem walk from the FS-defined scan root, off the UI thread, feeding progress paths for the status bar and a finished tree or a failure. Cooperative cancellation: check cancel often enough that Stop feels prompt (see techspec RS-01).

Cancellation semantics: when the user stops or an error terminates the scan, abandon the in-flight tree for that attempt, apply FS negative outcomes (including unset treemap where FS says so), and do not paint a mix of old and new trees in one frame. For Update cancelled or failed while a complete view existed, follow techspec UX-01 (restore prior snapshot when FS allows). Where FS is silent or ambiguous, document the behavior you ship and align tests until FS is clarified.

### Treemap model

In-memory structure built by the scanner, sufficient for FS fields (sizes, drive shares, node versus leaf among children) and for listing direct children of the current context. If a folder was not fully classified because the scan stopped early, use an internal РІР‚СљincompleteРІР‚Сњ flag or drop the partial attempt rather than guessing node or leaf. РІР‚СљIncompleteРІР‚Сњ is not an FS folder class; it is only an implementation marker until FS defines something stricter.

### Treemap layout

Pure function from child metrics and a pixel rectangle to tile rectangles. Areas among siblings follow FS drive share proportions. Cache layout until navigation, data, window size, or DPI changes (techspec RS-02, DP-02). Squarified layout in the active `win-go/` module is one option.

### Treemap view

Paint, labels, tooltips, hit testing. Measure fonts with the same DPI you use to paint (techspec DP-01). Fancy versus shabby and strings still come only from FS.

#### Label fit algorithm (current implementation)

The current label-fit flow is intentionally simple and bounded:

1. Try the full heading in detailed form (heading + details) with a binary search on font size between `treemap.headingMinFontSize` and `treemap.headingMaxFontSize`.
2. If full heading does not fit even at minimal font, try shortening at minimal font (detailed form), checking candidates from least shortened to most shortened.
3. If that fails, try full heading at minimal font in brief form (heading only).
4. If full brief still does not fit, try shortening at minimal font in brief form, again from least shortened to most shortened.
5. If no variant fits, render `treemap.labelDummy`.

Binary search for font size always returns the largest fitting integer size in the configured range. Shortening keeps the placeholder centered, and candidate headings are checked in quality order so the first fit is the least-shortened variant.

### Shell helpers

Folder picker and Explorer launch for Explore. Meet FS dialog constraints. If `ShellExecute` or equivalent fails, show a clear error to the user (FS does not define the string; avoid silent failure).

## 4. Concurrency and data ownership

Recommended model: the scanner builds a new tree in private memory while the UI shows the previous snapshot or an empty or busy state. When a scan completes successfully, swap a single pointer or replace an immutable value on the UI thread so readers never see torn trees. The layout engine reads only the snapshot for the current frame. If you use locks instead, document the lock order and keep paint paths short.

Drive share denominator: FS refers to total capacity of the drive. In practice, resolve the volume that contains the scan root and use one Windows API (for example `GetDiskFreeSpaceExW` total bytes) consistently; document that choice next to tests (see techspec IO note under Р’В§8).

## 5. Reparse points and size correctness

Following every reparse point can double-count junctions and can cycle on bad links. Skipping everything can under-report. Pick one policy, implement cycle detection if you follow links, and document it for IO-03 verification. The sample codeРІР‚в„ўs behavior is a baseline, not a norm.

## 6. Memory and very large trees

FS does not require streaming. For huge directories, prefer honest failure or a user-visible warning (techspec MEM-01) over runaway allocation. Virtualized treemap data is a future option if FS evolves.

## 7. Logging and support

FS does not require logging. For desktop support, optional debug logging (file or Windows event) helps with scan and shell failures. Typical switches: a debug build tag, an environment variable (for example `ERASE_REWRITE_DEBUG_LOG` pointing to a file path), or a documented registry valueРІР‚вЂќpick one approach, document it for support staff, and avoid logging more path detail than the UI already exposes to the user.

## 8. Reference stack in this repository

The shipped `win-go` UI uses **vendored `walk`** (under `win-go/third_party/walk`) with a CustomWidget for the treemap and `Synchronize` for cross-thread UI updates. Another Win32 binding is fine if threading, DPI, and PE rules still hold.

## 9. Build and release

Produce a PE with icon and `VERSIONINFO` per techspec Р’В§6. Optional signing and `-trimpath` remain engineering choices.

## 10. Testing hints

FS: tests and manual scripts keyed to FS headings. Techspec: cover Р’В§5РІР‚вЂњР’В§12 rows per techspec Р’В§13 (see the informative examples there), including the UX-01 Update-then-cancel case when FS allows snapshot restore. Architecture risks (cancellation swap, reparse policy, drive total API, network timeout) deserve targeted tests even when FS is silent.

## 11. Non-goals for this architecture note

Other operating systems, remote-only storage models, localization beyond FS, and long-lived configuration stores stay out of scope unless FS or the roadmap expands.
