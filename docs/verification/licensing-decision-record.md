# Licensing decision record — Erase & Rewrite (Windows Qt)

Status: **draft — requires owner sign-off before release**

Last updated: 2026-07-12

This file satisfies LEGALSPEC LS-70 and LS-72. It is not approved until the principal copyright holder marks it **approved**.


## Program (first-party)

| Field | Value |
|-------|-------|
| Product | Erase & Rewrite |
| Principal copyright | Mikhail Ostrogorskiy |
| Program license | GNU General Public License v3.0 or later (GPLv3+) |
| Corresponding Source method | GPLv3 section 6d (public Internet release) — **draft** |
| Source URL | **TBD** — stable URL, no authentication, maintained per §6d |
| Immutable tag/commit | **TBD** — must match shipped binary |
| Source retention | While binaries offered, per GPLv3 section 6d |
| Build reproduction verified | **No** |


## Component inventory (mandatory per release)

Each row must be complete before approval. One license basis per component.

| Component | Version | Link | License basis | Modified | Notices shipped | Source / relinking |
|-----------|---------|------|---------------|----------|-----------------|-------------------|
| Erase & Rewrite (Program) | see build-meta | n/a | GPLv3+ | no | `COPYING`, `LICENSE-NOTICE`, `INSTALL-LICENSE.txt` | see Program table above |
| Qt6Core | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| Qt6Gui | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| Qt6Widgets | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| Qt6Svg | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| QWindowsIntegrationPlugin | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| QSvgPlugin | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| QICOPlugin | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| QJpegPlugin | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| QGifPlugin | **TBD** | static | **TBD** | no | **TBD** | **TBD** |
| MinGW runtime (libgcc/libstdc++) | **TBD** | static | **TBD** | no | **TBD** | n/a |

Technical version and module facts for the active line: [impl-win-cpp-qt.md](../specs/impl-win-cpp-qt.md) §13 (not repeated here).


## LGPL static relinking kit (LS-82)

Required only if any Qt row uses LGPLv3 + static linking as the **recorded license basis**. Choosing GPLv3 or commercial Qt instead is an **alternative basis** (LS-83), not an LGPL compliance mechanism.

| Field | Value |
|-------|-------|
| Mechanism chosen | **TBD** |
| Delivery method | Per recorded GPLv3 §6 method (§6d equivalent access beside binary for Internet releases) |
| Verified relink procedure documented | **No** |


## Installer (Inno Setup)

| Field | Value |
|-------|-------|
| Script | `codebase/installer/Erase & Rewrite.iss` |
| Legal notice file | `INSTALL-LICENSE.txt` (`InfoBeforeFile` — no acceptance gate) |
| Installed `{app}` license files | **TBD** — `COPYING`, `LICENSE-NOTICE`, `THIRD-PARTY-NOTICES` minimum per LS-50 |
| Excluded | `*.date` build markers |


## Evidence locations

| Artifact | Path |
|----------|------|
| Installer license notice | `codebase/installer/INSTALL-LICENSE.txt` |
| Legal spec | `codebase/docs/specs/legalspec.md` |
| Technical link model | `codebase/docs/specs/impl-win-cpp-qt.md` §13 |
| Import table check | `codebase/win-cpp-qt/test-run.ps1` (static mode) |


## Open actions before approval

1. Principal copyright holder selects **one** license basis per Qt component (GPL-Qt, LGPL static with LS-82 kit, or commercial per LS-83).
2. Add `COPYING` (full GPLv3 text only) and `LICENSE-NOTICE` to `codebase/` repository root.
3. Fill stable source URL, tag/commit, and reproduction test (LS-04 §6d, checklist §6).
4. Ship `COPYING`, `LICENSE-NOTICE`, and `THIRD-PARTY-NOTICES` in installer `{app}`.
5. Implement About dialog Appropriate Legal Notices (LS-31).
6. Update PE `LegalCopyright` pipeline.
7. Complete LGPL relinking kit if LGPL static path is chosen.
