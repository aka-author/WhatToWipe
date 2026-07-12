# Licensing decision record — Erase & Rewrite (Windows Qt)

Status: **draft — requires owner sign-off before release**

Last updated: 2026-07-12

This file satisfies LEGALSPEC LS-70. It is informative until marked **approved** by the copyright holder.


## Program

| Field | Value |
|-------|-------|
| Product | Erase & Rewrite |
| Copyright | Mikhail Ostrogorskiy |
| Program license | GNU General Public License v3.0 or later (GPLv3+) |
| Source offer | Git repository `codebase/` at release tag / `build-meta.json` commit |


## Qt (third-party)

| Field | Value |
|-------|-------|
| Version | 6.10.3 |
| Toolchain | MinGW 13.1.0 |
| Modules | Qt6Core, Qt6Gui, Qt6Widgets, Qt6Svg |
| Plugins (static) | QWindowsIntegrationPlugin, QSvgPlugin, QICOPlugin, QJpegPlugin, QGifPlugin |
| Link model | **Static** — libraries and plugins linked into `EraseAndRewrite.exe` |
| Qt kit path | `C:\cpp\qt\6.10.3\mingw_64_static` (not in Git) |
| Qt sources | `C:\cpp\qt-src\6.10.3\Src\` (qtbase, qtsvg; not in Git) |
| Qt modified | No (upstream sources as built) |
| **Qt license path chosen** | **TBD — owner must record: GPL-Qt option vs LGPL static compliance vs commercial** |


## MinGW runtime

| Field | Value |
|-------|-------|
| Link model | Static (`libgcc`, `libstdc++`) into executable |
| Evidence | `test-run.ps1` objdump check; see build 1.0.0.0019 |


## Installer (Inno Setup)

| Field | Value |
|-------|-------|
| Script | `codebase/installer/Erase & Rewrite.iss` |
| License page | `EULA.txt` |
| Installed files (static model) | `EraseAndRewrite.exe`, license/notice texts (see LEGALSPEC LS-50 — **notice files TBD in installer payload**) |
| Excluded | `*.date` build markers |


## Evidence locations

| Artifact | Path |
|----------|------|
| EULA | `codebase/installer/EULA.txt` |
| Legal spec | `codebase/docs/specs/legalspec.md` |
| Technical link model | `codebase/docs/specs/impl-win-cpp-qt.md` §13 |
| Import table check | `codebase/win-cpp-qt/test-run.ps1` (static mode) |


## Open actions before release sign-off

1. Owner selects and records Qt license path (GPL-Qt vs LGPL static vs commercial).
2. Add `LICENSE` (GPLv3 full text) to `codebase/` root if missing.
3. Align `EULA.txt` Qt section with static link model and chosen Qt path.
4. Ship `LICENSE` and `THIRD-PARTY-NOTICES` (or equivalent) inside installer `{app}`.
5. Add license notice or link in About dialog (LEGALSPEC LS-31).
6. Update PE `LegalCopyright` in `app.rc` / `versioninfo.json` pipeline.
