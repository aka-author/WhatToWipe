# Erase & Rewrite: Technical Specification (Windows, C++, Qt)

## 1. Purpose and Precedence

[funcspec.md](./funcspec.md) (FS) is the only normative source for product behavior, user-visible text, use cases, and acceptance criteria. This document adds Windows-, C++-, Qt-, and build-level points that FS does not spell out. It must not repeat or rephrase individual FS rules already covered there.

If anything here could be read as conflicting with FS, FS wins and this document must be corrected.

Out of scope here: component layout, class names, and algorithms (see [arch-win-cpp-qt.md](./arch-win-cpp-qt.md), informative; [impl-win-cpp-qt.md](./impl-win-cpp-qt.md), as-built).

This target supersedes [techspec-win-go.md](./techspec-win-go.md) for new Windows delivery work once the `win-cpp-qt/` module is the active shipping line. The Go target remains in the repository until the Qt build is verified against FS and this techspec.


## 2. How to Read This Document

Words like "must", "must not", and "should" carry their usual English force: "must" means the implementation is not compliant if it fails the point; "should" is a strong recommendation. This file does not use the RFC 2119 keyword capitalization style.


## 3. Normative References

| ID | Document |
|----|-----------|
| FS | [funcspec.md](./funcspec.md) (functional specification) |
| IMPL | [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) (as-built implementation map) |
| GO-ARCH | [arch-win-go.md](./arch-win-go.md) (informative; prior target lessons) |
| GRID-GUIDE | [../guides/win/choosing-a-lib-for-a-grid.md](../guides/win/choosing-a-lib-for-a-grid.md) (informative; Go-era grid evaluation) |


## 4. Conformance to FS

| ID | Requirement |
|----|-------------|
| TS-01 | The implementation must satisfy FS in full. Verification (manual or automated) must be traceable to FS sections; do not copy FS wording into this document as a substitute for reading FS. |
| TS-02 | FS Settings Form rules (real grid, in-cell editors, no detached editor panel) are mandatory acceptance gates for this target. The Go + walk delivery failed these gates; that failure is a primary reason for this target. |


## 5. Deliverable and Platform

| ID | Requirement |
|----|-------------|
| PL-01 | The shipping artifact must be a 64-bit Windows GUI executable suitable for interactive desktop use (not a console-only program as the only UI). |
| PL-02 | The program must support Windows 10 and Windows 11 (64-bit x64). It should run on newer Windows releases where the same desktop and shell assumptions still hold. It must not depend on optional Windows features beyond what a typical desktop install provides for file management and choosing folders. |
| PL-03 | User-visible paths must use normal Windows forms (drive letters, UNC where applicable) wherever FS asks to show a path. |
| PL-04 | The implementation must be written in C++17 or newer and must use Qt 6 (LTS line). The exact Qt minor version and compiler toolchain must be pinned or recorded in the repository. The shipping build uses Qt 6.10.3 `mingw_64` with MinGW 13.1.0 via `win-cpp-qt/toolchain-qt-mingw.cmake` and `win-cpp-qt/CMakeLists.txt`. |
| PL-05 | The release must not require a separate browser runtime (for example WebView2) or a Node-based frontend build for core product UI. Settings and main shell must be native Qt widgets. |
| PL-06 | Qt deployment must match the link model recorded in [legalspec.md](./legalspec.md) and `docs/verification/licensing-decision-record.md`. The shipping static build embeds Qt in `EraseAndRewrite.exe` with no separate Qt DLL deploy folder; a dynamic build must still ship all runtime DLLs and platform plugins required on a clean Windows machine via `windeployqt` and installer rules. |
| PL-07 | Launcher command-line windows are strictly forbidden in every case. The program must start natively as a Windows GUI process: the shipping executable must be linked for the Windows GUI subsystem (`SUBSYSTEM:WINDOWS`), must not allocate or show a console window on startup, and must not be launched via a wrapper batch file, script, or other intermediate console host. Shortcuts, the installer, and Explorer must start `EraseAndRewrite.exe` directly. |


## 6. Windows Executable and Version Resource

| ID | Requirement |
|----|-------------|
| WR-01 | The release executable must be a normal Windows PE that can carry the metadata Windows Explorer shows under Properties → Details, so the FS rules that tie About to file version can be met in the usual way. |
| WR-02 | The build must embed a `VERSIONINFO` resource (via Qt `.rc` / Windows resource file or equivalent) with string file info populated so Explorer's Details view is meaningful. At minimum `FileVersion` and `ProductVersion` must match how the About dialog reads them back. Other common fields (`CompanyName`, `FileDescription`, `LegalCopyright`, `OriginalFilename`, `ProductName`) should be filled for a shipping binary. Exact About text remains defined only in FS. |
| WR-03 | Release builds must avoid aggressive stripping profiles that remove standard PE metadata expected by Windows tooling and endpoint scanners. |
| WR-04 | Release executables should be Authenticode-signed. If signing is not available for a specific build, that build must be explicitly marked as unsigned in release notes or artifacts metadata. |
| WR-05 | The release copy/deployment step must tolerate transient file locks caused by antivirus scanning (for example with bounded retry), rather than failing immediately on first access-denied or sharing-violation errors. |

Exact About strings and version format stay only in FS.


## 7. Typography and DPI

| ID | Requirement |
|----|-------------|
| DP-01 | Where FS gives treemap sizes in points, the implementation must convert points to device pixels using the effective DPI of the window or surface that paints the treemap (Qt device pixel ratio and font point sizes), so layout on high-DPI displays matches what FS intends. |
| DP-02 | When the window moves between monitors with different scale factors, or when the system DPI changes, the treemap must reflow and typography must recompute from the new effective DPI. A short delay is acceptable; a stuck layout at the old scale is not. |
| DP-03 | Qt high-DPI attributes (`AA_EnableHighDpiScaling` / Qt 6 default behavior, `AA_UseHighDpiPixmaps` where icons are raster) must be enabled explicitly in application startup documentation so verification is repeatable. |

Metric definitions and font face stay only in FS.


## 8. Scanning, Paths, and Windows I/O

These rows add platform behavior FS does not enumerate. They must still produce outcomes compatible with FS.

| ID | Requirement |
|----|-------------|
| IO-01 | Long paths: the release build must use a testable, documented mechanism. Acceptable approaches include an application manifest with `longPathAware` set true (preferred), and/or `\\?\`-style opening that tests assert. Verification notes must state which mechanism ships. If a path still cannot be opened, surface it like any other read error without crashing. |
| IO-02 | Access denied and other per-entry failures must not silently drop a subtree as empty unless FS explicitly allows that. When the scan finishes with any such failures, the user must see a summary that the run was incomplete (at minimum a count of paths that could not be read). Silence is not allowed. |
| IO-03 | Reparse points (junctions, symlinks, mount points): the scanner must not loop forever. Either follow them with cycle detection or skip them with a recorded reason; pick one strategy, document it for testers, and stay consistent with FS size and navigation semantics. |
| IO-04 | Network paths: if the share goes away or stops responding, the scan must end in a controlled failure path (FS negative outcomes), not an unbounded hang. I/O on UNC or mapped drives must use bounded waits; default ceiling 30 seconds per stuck operation or path group unless documented otherwise. |
| IO-05 | Volume snapshot or transient file locking is OS-defined; the implementation should retry or skip with a recorded error rather than blocking the UI thread. |

How you obtain "total capacity of the drive" for FS drive share must use one consistent Windows API interpretation (for example `GetDiskFreeSpaceExW` total bytes on the volume that holds the scan root) and must be documented for verification.


## 9. Responsiveness During Scan and Layout

| ID | Requirement |
|----|-------------|
| RS-01 | Stop and Esc must remain usable while scanning; the Qt GUI thread must not be wedged for so long that the user cannot invoke them. "Indefinite hang" is a defect. |
| RS-02 | Heavy treemap layout over very large child lists must not run unbounded work on the GUI thread during a single paint; cache layout until data, size, or DPI changes, or offload work to a worker thread and swap results on the GUI thread, so menus and Stop stay reachable. |
| RS-03 | Filesystem scanning must run off the Qt GUI thread (for example `QThread` worker or `QtConcurrent` with explicit cancellation). Progress signals to the status bar must be throttled per FS `scanning.updateInterval`. |


## 10. Memory at Scale

| ID | Requirement |
|----|-------------|
| MEM-01 | There is no FS requirement to stream an unbounded tree. The implementation should detect excessive memory use or pathological trees and fail or warn in a controlled way rather than crashing the process, while still honoring FS outcomes (for example unset treemap when no valid complete data exists). |


## 11. Accessibility Baseline

| ID | Requirement |
|----|-------------|
| A11Y-01 | Keyboard: any FS-required keys (for example Backspace for Up) must work when FS says the command is available; the treemap surface should accept focus so keyboard-driven use is possible alongside mouse. |
| A11Y-02 | Visual: respect system high-contrast themes where Qt allows it; do not hard-code colors that break high contrast for chrome mandated by FS. Treemap tile coloring still follows FS configuration colors. |

Screen reader names or live regions for custom-drawn treemap tiles are recommended for desktop quality but are not normative rows in this document unless FS adds them.


## 12. Update and Cancel

| ID | Requirement |
|----|-------------|
| UX-01 | When the user starts Update from an already complete treemap and the scan is then cancelled or fails, the program should restore the previous complete snapshot (data and view) instead of leaving the user on a blank or unset state, unless FS text for that branch requires unset. If FS is read to require unset, FS wins and this row must be waived or revised. |


## 13. Settings Grid (Qt-specific, normative)

The Go + walk target failed FS Settings Form acceptance: overlay editors on `SysListView32`, non-editable cells at runtime, and property-sheet substitutes were all rejected under FS automatic rejection criteria. This target must satisfy FS Settings Form using Qt grid facilities.

| ID | Requirement |
|----|-------------|
| SG-01 | The settings UI must use one shared tabular layout for all treemap configuration rows. Acceptable Qt realizations: `QTableView` with permanent cell widgets via `setIndexWidget`, or an equivalent Qt approach where every value cell hosts a visible editor for the full lifetime of the dialog. |
| SG-02 | The implementation must not use click-to-spawn or click-to-move editor overlays. Default `QTableView` delegate editing-on-activation alone is non-compliant unless editors are made permanently visible (for example via `setIndexWidget` or always-open delegate widgets). |
| SG-03 | The implementation must not use a detached editor area (bottom panel, side panel, floating editor, or external editor host). |
| SG-04 | The grid must expose one row per user-editable treemap parameter from FS Treemap Configuration Parameters, with stable shared column boundaries across all rows. |
| SG-05 | Color rows must keep hex value entry, live swatch, and picker control visible in the same row (composite cell widget is acceptable). `QColorDialog` is the preferred picker. |
| SG-06 | `treemap.tileFontName` must use an editable combo (`QComboBox` with `setEditable(true)`) populated from installed system fonts (`QFontDatabase`), with manual entry allowed. |
| SG-07 | Numeric, percentage, TSize, string, and color fields must validate per FS constraints before Apply/OK. Invalid Apply/OK must block save, show a clear inline error (not only modal boxes for field validation), and must not write partial config. |
| SG-08 | Dialog actions must match FS: Apply, OK, Cancel, Reset Treemap Defaults; atomic save (all valid or none). |
| SG-09 | Before coding the settings UI, the implementer must record a short control-strategy declaration in `docs/verification/` (or equivalent) stating why the chosen Qt widgets constitute a FS *real grid* and not a *grid-like* substitute, per FS Implementation gate before coding. |
| SG-10 | Settings grid acceptance must be verified on a clean Windows VM with manual FS checklist items; automated UI tests are recommended but do not replace the manual gate for SG rows. |


## 14. Configuration File Compatibility

| ID | Requirement |
|----|-------------|
| CF-01 | Config key names, value text format, and semantics must remain compatible with the existing `win-go` implementation and FS Program Setup Configuration (same `treemap.*` keys, TSize/Percentage/Color token rules). |
| CF-02 | FS defines config folder `%LocalAppData%\Erase & Rewrite` and file `Erase & Rewrite.config.txt`. The Qt build must use those paths for new installs. |
| CF-03 | For upgrade from interim Qt builds, the program must detect and load `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt` when the FS path is absent, without requiring the user to migrate manually. On successful load from the legacy path, the next save writes the FS path. |
| CF-04 | Built-in defaults embedded in the executable must match FS default values for all treemap parameters. |


## 15. Build, Dependencies, and Licensing

| ID | Requirement |
|----|-------------|
| BD-01 | Build system must be CMake (minimum 3.21) with out-of-source build directories; build products must not be committed inside `codebase/`. |
| BD-02 | Release binaries must land under `<ProjectRoot>/bin/win/current/` following the same project convention as `win-go/build.ps1`, unless the build script documents a deliberate change. |
| BD-03 | Qt modules required at minimum: `Qt6::Widgets`, `Qt6::Gui`, `Qt6::Core`, `Qt6::Svg` (toolbar icons). Optional: `Qt6::Concurrent` for scan workers. |
| BD-04 | Third-party Qt licensing obligations must be documented per [legalspec.md](./legalspec.md) §3 and the license decision record in `docs/verification/`. Deployment must match the recorded link model (static monolithic exe or documented dynamic layout). |
| BD-05 | C++ dependencies beyond Qt must be kept minimal and vendored or fetched via CMake with pinned versions recorded in-repo. |


## 16. Verification of Additive Requirements

| ID | Requirement |
|----|-------------|
| VR-01 | Each requirement row in §5 through §15 must have a written test plan entry, an automated test, or an explicit waiver before release. |
| VR-02 | Any waiver against a "must" in this document must be explicit, with a short reason and a plan to get back to compliance. |
| VR-03 | SG rows must be verified against every FS Settings Form automatic rejection criterion before the Qt target is declared the active Windows delivery line. |

Product acceptance is still defined only by FS and whatever verification you attach to FS itself.

### Suggested verification methods (informative)

PL-01/PL-02/PL-07 via PE subsystem checks (no `SUBSYSTEM:CONSOLE`, no startup console window) and clean VM smoke from installer shortcuts; DP-01/DP-02 with golden measurements at 100%, 125%, 150% scale; WR-01/WR-02 by dumping `VERSIONINFO` strings and comparing to About output; IO fixtures for long paths, denied ACLs, cyclic junctions, stalled UNC; RS-01 large-directory manual pass with Stop responsiveness; MEM-01 stress fixture; A11Y manual keyboard and high-contrast passes; UX-01 Update-then-cancel snapshot restore; SG manual pass through full FS Settings definition-of-done checklist; CF round-trip tests against sample configs from `win-go/dist/`.

For reviewers: keep one short note in `docs/verification/` recording shipped choices for IO-01, IO-03, drive-total API, config path migration (CF-03), Qt settings control strategy (SG-09), and logging, so VR-01 evidence is easy to find. The as-built compliance table in [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §15 tracks open gaps (for example IO-02, WR-04).
