# WhatToWipe: Technical Specification (Windows, Go)

## 1. Purpose and Precedence

[funcspec.md](./funcspec.md) (FS) is the only normative source for product behavior, user-visible text, use cases, and acceptance criteria. This document only adds Windows-, Go-, and build-level points that FS does not spell out. It should not repeat or rephrase individual FS rules that are already covered there.

If anything here could be read as conflicting with FS, FS wins and this document should be corrected.

Out of scope here: component layout, libraries, and algorithms (see design notes or [arch-win-go.md](./arch-win-go.md), which is informative only).


## 2. How to Read This Document

Words like “must”, “must not”, and “should” carry their usual English force: “must” means the implementation is not compliant if it fails the point; “should” is a strong recommendation. This file does not use the RFC 2119 keyword capitalization style.


## 3. Normative References

| ID | Document |
|----|-----------|
| FS | [funcspec.md](./funcspec.md) (functional specification) |


## 4. Conformance to FS

| ID | Requirement |
|----|-------------|
| TS-01 | The implementation must satisfy FS in full. Verification (manual or automated) must be traceable to FS sections; do not copy FS wording into this document as a substitute for reading FS. |


## 5. Deliverable and Platform

| ID | Requirement |
|----|-------------|
| PL-01 | The shipping artifact must be a 64-bit Windows executable suitable for interactive desktop use (not a console-only program as the only UI). |
| PL-02 | The program must support Windows 10 and Windows 11 (64-bit amd64). It should run on newer Windows releases where the same desktop and shell assumptions still hold. It must not depend on an optional Windows feature beyond what a typical desktop install provides for file management and choosing folders. |
| PL-03 | User-visible paths must use normal Windows forms (drive letters, UNC where applicable) wherever FS asks to show a path. |
| PL-04 | The implementation must be written in Go and must build with a supported Go toolchain version pinned or recorded in the repository (for example in `go.mod`). |


## 6. Windows Executable and Version Resource

| ID | Requirement |
|----|-------------|
| WR-01 | The release executable must be a normal Windows PE that can carry the metadata Windows Explorer shows under Properties → Details, so the FS rules that tie About to file version can be met in the usual way. |
| WR-02 | The build must embed a `VERSIONINFO` resource with string file info populated so Explorer’s Details view is meaningful. At minimum the `FileVersion` and `ProductVersion` string fields must be set in a way that matches how you read them back for the FS About string; other common fields (`CompanyName`, `FileDescription`, `LegalCopyright`, `OriginalFilename`, `ProductName`) should be filled for a shipping binary so support staff can identify the build. The exact About text remains defined only in FS. |
| WR-03 | Release builds must avoid aggressive stripping profiles that remove standard PE metadata expected by Windows tooling and endpoint scanners. If size optimization is used, it must preserve normal executable metadata and diagnostics needed for support triage. |
| WR-04 | Release executables should be Authenticode-signed. If signing is not available for a specific build, that build must be explicitly marked as unsigned in release notes or artifacts metadata. |
| WR-05 | The release copy/deployment step must tolerate transient file locks caused by antivirus scanning (for example with bounded retry), rather than failing immediately on first access-denied or sharing-violation errors. |

Exact About strings and version format stay only in FS.


## 7. Typography and DPI

| ID | Requirement |
|----|-------------|
| DP-01 | Where FS gives treemap sizes in points, the implementation must convert points to device pixels using the effective DPI of the window or surface that paints the treemap, so layout on high-DPI displays matches what FS intends. |
| DP-02 | When the window moves between monitors with different scale factors, or when the system DPI changes, the treemap must reflow and typography must recompute from the new effective DPI. A short delay is acceptable; a stuck layout at the old scale is not. |

Metric definitions and font face stay only in FS.


## 8. Scanning, Paths, and Windows I/O

These rows add platform behavior FS does not enumerate. They must still produce outcomes compatible with FS (scan success or failure, unset treemap on failure paths, and so on).

| ID | Requirement |
|----|-------------|
| IO-01 | Long paths: the release build must use a testable, documented mechanism. Acceptable approaches include an application manifest with `longPathAware` set true (preferred for packaged apps), or another documented combination of manifest, documented registry dependency, and/or `\\?\`-style opening that your tests assert. Verification notes must state which mechanism ships so IO-01 can be tested without guesswork. If a path still cannot be opened, surface it like any other read error without crashing. |
| IO-02 | Access denied and other per-entry failures must not silently drop a subtree as empty unless FS explicitly allows that. When the scan finishes with any such failures, the user must see a summary that the run was incomplete (at minimum a count of paths that could not be read; a short list or detail dialog is allowed). Prefer the status bar after FS no longer requires the live scanning path there; otherwise use a short dialog. Silence is not allowed. |
| IO-03 | Reparse points (junctions, symlinks, mount points): the scanner must not loop forever. You must either follow them with cycle detection (visited volume + file ID or canonical path set) or skip them with a recorded reason; pick one strategy, document it for testers, and stay consistent with FS size and navigation semantics. |
| IO-04 | Network paths: if the share goes away or stops responding, the scan must end in a controlled failure path (FS negative outcomes), not an unbounded hang. I/O on UNC or mapped drives must use bounded waits (per-operation or watchdog); use a default ceiling of 30 seconds per stuck operation or path group unless you document a different default and how to configure it. |
| IO-05 | Volume snapshot or transient file locking is OS-defined; the implementation should retry or skip with a recorded error rather than blocking the UI thread. |

How you obtain “total capacity of the drive” for FS drive share is not spelled in FS; you must use one consistent Windows API interpretation (for example total bytes on the volume that holds the scan root) and document it for verification.


## 9. Responsiveness During Scan and Layout

| ID | Requirement |
|----|-------------|
| RS-01 | Stop and Esc must remain usable while scanning; the UI thread must not be wedged for so long that the user cannot invoke them. There is no fixed millisecond budget here, but “indefinite hang” is a defect. |
| RS-02 | Heavy treemap layout over very large child lists should not run unbounded work on the UI thread during a single paint; cache layout until data, size, or DPI changes, or split work, so menus and Stop stay reachable. |


## 10. Memory at Scale

| ID | Requirement |
|----|-------------|
| MEM-01 | There is no FS requirement to stream an unbounded tree. The implementation should detect excessive memory use or pathological trees and fail or warn in a controlled way rather than crashing the process, while still honoring FS outcomes (for example unset treemap when no valid complete data exists). |


## 11. Accessibility Baseline

| ID | Requirement |
|----|-------------|
| A11Y-01 | Keyboard: any FS-required keys (for example Backspace for Up) must work when FS says the command is available; the treemap surface should accept focus so keyboard-driven use is possible alongside mouse. |
| A11Y-02 | Visual: respect system high-contrast themes where the GUI stack allows it; do not hard-code colors that break high contrast for chrome mandated by FS. Treemap node versus leaf coloring still follows FS. |

Screen reader names or live regions for custom-drawn treemap tiles are recommended for desktop quality but are not normative rows in this document unless FS adds them.


## 12. Update and Cancel

| ID | Requirement |
|----|-------------|
| UX-01 | When the user starts Update from an already complete treemap and the scan is then cancelled or fails, the program should restore the previous complete snapshot (data and view) instead of leaving the user on a blank or unset state, unless FS text for that branch requires unset. If FS is read to require unset, FS wins and this row must be waived or revised. |


## 13. Verification of Additive Requirements

| ID | Requirement |
|----|-------------|
| VR-01 | Each requirement row in §5 through §12 must have a written test plan entry, an automated test, or an explicit waiver before release. |
| VR-02 | Any waiver against a “must” in this document must be explicit, with a short reason and a plan to get back to compliance. |

Product acceptance is still defined only by FS and whatever verification you attach to FS itself.

### Suggested verification methods (informative)

These are examples, not extra requirements: PL-01/PL-02 via PE subsystem and GUI subsystem checks or clean VM smoke; DP-01/DP-02 with golden measurements or screenshots at 100%, 125%, 150% scale and after moving the window between displays; WR-01/WR-02 by dumping `VERSIONINFO` strings and comparing to About output; IO fixtures for long paths, denied ACLs, cyclic junctions, stalled UNC with timeout; RS-01 subjective large-directory manual pass; MEM-01 stress fixture; A11Y manual keyboard and high-contrast passes; UX-01 by starting Update on a complete view, cancelling, and asserting the prior snapshot is shown when FS allows.

For reviewers: keep one short note in the repo (path up to you) that records the shipped choices for IO-01 long-path mechanism, IO-03 reparse policy, drive-total API for share math, and logging per architecture §7, so VR-01 evidence is easy to find.
