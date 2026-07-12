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
| Corresponding Source method | GPLv3 section 6d (network server) — **draft** |
| Source URL | **TBD** — must be public, no authentication |
| Immutable tag/commit | **TBD** — must match shipped binary |
| Source retention | While binaries offered + minimum period per chosen §6 method |
| Build reproduction verified | **No** |


## Component inventory (mandatory per release)

Each row must be complete before approval. One license basis per component.

| Component | Version | Link | License basis | Modified | Notices shipped | Source / relinking |
|-----------|---------|------|---------------|----------|-----------------|-------------------|
| Erase & Rewrite (Program) | see build-meta | n/a | GPLv3+ | no | `LICENSE`, `INSTALL-LICENSE.txt` | see Program table above |
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

Required only if any Qt row uses LGPLv3 + static linking.

| Field | Value |
|-------|-------|
| Mechanism chosen | **TBD** |
| Deliverables in installer / offer | **TBD** |
| Verified relink procedure documented | **No** |


## Installer (Inno Setup)

| Field | Value |
|-------|-------|
| Script | `codebase/installer/Erase & Rewrite.iss` |
| License notice file | `INSTALL-LICENSE.txt` (`LicenseFile`) |
| Installed `{app}` license files | **TBD** — `LICENSE`, `THIRD-PARTY-NOTICES` minimum per LS-50 |
| Excluded | `*.date` build markers |


## Evidence locations

| Artifact | Path |
|----------|------|
| Installer license notice | `codebase/installer/INSTALL-LICENSE.txt` |
| Legal spec | `codebase/docs/specs/legalspec.md` |
| Technical link model | `codebase/docs/specs/impl-win-cpp-qt.md` §13 |
| Import table check | `codebase/win-cpp-qt/test-run.ps1` (static mode) |


## Open actions before approval

1. Principal copyright holder selects **one** license basis per Qt component (GPL-Qt, LGPL static with LS-82 kit, or commercial).
2. Add `LICENSE` (full GPLv3 text) to `codebase/` repository root.
3. Fill source URL, tag/commit, and reproduction test (LS-04, checklist §6).
4. Ship `LICENSE` and `THIRD-PARTY-NOTICES` in installer `{app}`.
5. Implement About dialog Appropriate Legal Notices (LS-31).
6. Update PE `LegalCopyright` pipeline.
7. Complete LGPL relinking kit if LGPL static path is chosen.
