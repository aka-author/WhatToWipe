# Erase & Rewrite: Legal and Licensing Specification


## About This Document

### Identification

Document ID: LEGALSPEC.

Genre: normative legal and licensing specification for the product and its repository.

Scope: Erase & Rewrite (all delivery targets unless a target techspec explicitly documents a narrower waiver).


### Purpose

This document states the licensing model the product must follow, the notices and texts users must receive, how third-party components (especially Qt) must be handled in builds and releases, how the source repository must stay compliant, and what the Windows installer must present at installation time.

It does not replace FS rules for About-dialog product text. It adds obligations FS does not spell out.


### Target audience

Copyright holder, release engineers, contributors, reviewers, and agents preparing installers or dependency changes.


### Precedence

Interpretation order for licensing questions:

1. Applicable law and the text of the GNU General Public License version 3 (or later) as applied to first-party program code.
2. This document (LEGALSPEC).
3. [funcspec.md](./funcspec.md) (FS) for user-visible About content that FS defines.
4. Target techspec rows that cite licensing (for example BD-04 in [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md)).
5. [dispute.md](./dispute.md) and verification notes — informative history only; they do not override LEGALSPEC.

If LEGALSPEC disagrees with a techspec licensing row, LEGALSPEC wins and the techspec row must be updated.


### Prose standard

This document follows [XMSTP](https://github.com/aka-author/xmstp) (GRTD, GPIT, ICIT, en-words).


### How to read this document

Words like "must", "must not", and "should" carry their usual English force. "Must" means the release or repository is not compliant if the point is violated. "Should" is a strong recommendation that becomes "must" once a release candidate is tagged.


## Terms

*Program:* the Erase & Rewrite application and its first-party source code authored for this product.

*Combined work:* the Program plus third-party libraries linked into the shipping binary or distributed beside it.

*Shipping artifact:* what an end user receives from an installer, archive, or store listing (executable, license texts, notices).

*Orthodox GPL:* GNU General Public License version 3 (or, at the copyright holder's option, any later version published by the FSF), applied in the canonical form without proprietary exceptions to first-party code.

*License decision record:* the file [../verification/licensing-decision-record.md](../verification/licensing-decision-record.md) (or its successor) that states the chosen Qt license path, link model, and evidence locations.


## 1. Cornerstone — Orthodox GPLv3

| ID | Requirement |
|----|-------------|
| LS-01 | The Program must be free software licensed under the **GNU General Public License version 3** (GPLv3), or (at the copyright holder's option) **any later version** of the GPL published by the Free Software Foundation. This is the canonical, orthodox copyleft license for the product. |
| LS-02 | First-party source code must not be distributed under terms that contradict GPLv3 copyleft (for example proprietary EULAs that deny the right to modify and redistribute the Program under GPL). |
| LS-03 | The copyright holder must be stated consistently as **Mikhail Ostrogorskiy** (or the successor name recorded in `LICENSE` and `EULA.txt`) until a formal assignment changes it. |
| LS-04 | Every binary distribution of the Program must be accompanied by a practical way for recipients to obtain the **corresponding source** of the Program under GPL — typically the public Git repository URL plus a tag or commit identifier matching the build. |
| LS-05 | GPLv3 section 7 additional terms are not used unless explicitly recorded in `LICENSE` with legal review. The default is no additional permissions or restrictions beyond GPLv3. |
| LS-06 | Patents: contributors grant rights consistent with GPLv3 section 11. Do not add separate patent licenses that weaken copyleft for first-party code. |


## 2. General requirements — product, notices, and documents

### 2.1 Repository license file

| ID | Requirement |
|----|-------------|
| LS-10 | The repository root under `codebase/` must contain a `LICENSE` or `COPYING` file with the **full text of GPLv3** (or a clear pointer to GPLv3 with the copyright line). |
| LS-11 | First-party source files should carry a short SPDX or copyright header referencing GPLv3-or-later. New files added to `win-cpp-qt/` and `win-go/` must follow the same header convention once established in the tree. |


### 2.2 Windows executable metadata

| ID | Requirement |
|----|-------------|
| LS-20 | PE `VERSIONINFO` string `LegalCopyright` must not be misleading. It must either name the copyright holder and reference GPLv3, or point to shipped license texts (for example "See LICENSE in installation folder"). The placeholder "See repository license" is acceptable only until the next release replaces it with an install-relative or URL form recorded in the license decision record. |
| LS-21 | `ProductName`, `FileDescription`, and `OriginalFilename` must match the shipping product identity (Erase & Rewrite / `EraseAndRewrite.exe`) and must not imply a different license regime. |


### 2.3 About dialog

| ID | Requirement |
|----|-------------|
| LS-30 | The About dialog must satisfy FS *Displaying the Program Information* (product name and dotted version identical to PE File version). |
| LS-31 | The About dialog should include a concise **license notice** and a control or text that tells users the Program is under GPLv3 and where to read the full license (for example "Help → About" supplement, a "License…" link, or text pointing to `{app}\LICENSE` after install). A release must not ship without any in-program path to license information. |
| LS-32 | About text must not claim the Program is proprietary, trialware, or "licensed separately" from GPLv3. |


### 2.4 End-user license agreement (EULA)

| ID | Requirement |
|----|-------------|
| LS-40 | `codebase/installer/EULA.txt` is the authoritative installer license file. It must state that the **Program** is under GPLv3-or-later and must include or incorporate the GPLv3 text (as today). |
| LS-41 | The EULA must not require the user to waive GPLv3 rights for the Program. Installation acceptance means acknowledgment of GPL terms for the Program, not surrender of them. |
| LS-42 | Third-party sections in the EULA (Qt and future libraries) must be clearly separated from the Program GPL section and must describe only the third-party component's license. |
| LS-43 | EULA changes require a one-line entry in the license decision record with date and reason. |


### 2.5 Documents delivered to the user

| ID | Requirement |
|----|-------------|
| LS-50 | Every shipping installer must place in the application directory at least: (1) full GPLv3 text for the Program, (2) third-party license texts required by the link model in §3, (3) a short `NOTICE` or `THIRD-PARTY-NOTICES` file listing components and versions when more than one third-party license applies. |
| LS-51 | `versioninfo.json` and `build-meta.json` are build metadata, not license documents. They may record version and commit for traceability but do not satisfy LS-50. |
| LS-52 | README or in-app help may summarize licensing but must not contradict `LICENSE` or `EULA.txt`. |


### 2.6 Documents stored in the repository

| ID | Requirement |
|----|-------------|
| LS-60 | This file (`legalspec.md`) is normative for licensing process. |
| LS-61 | [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §13 records the technical link model; it must stay consistent with the license decision record. |
| LS-62 | Legal verification evidence (dependency listings, Qt module list, `objdump` summaries) lives under `docs/verification/` and is cited from the license decision record. |


## 3. Third-party components

### 3.1 Decision record (mandatory before each release)

| ID | Requirement |
|----|-------------|
| LS-70 | Before tagging a release, maintain an up-to-date **license decision record** under `docs/verification/` stating: (a) Qt version and modules, (b) dynamic vs static link model, (c) whether Qt is used under **GPL** or **LGPL** (or commercial Qt license reference), (d) whether Qt sources were modified, (e) what notices and source offers ship with the installer, (f) any other linked libraries and their licenses. |
| LS-71 | Changing Qt version, modules, or static/dynamic link model requires updating the decision record **before** the first public build with that change. |


### 3.2 Qt (current shipping stack)

The Windows Qt delivery uses **Qt 6.10.3** with modules **Core, Gui, Widgets, Svg** and statically linked platform/image plugins (see [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §13).

| ID | Requirement |
|----|-------------|
| LS-80 | Qt is a third-party component. Its license is **not automatically** the same document as the Program's GPLv3, unless the project explicitly adopts Qt under the **GPL v3 option** offered by The Qt Company for open-source use and records that choice in the license decision record. |
| LS-81 | **Canonical open-source path for this product:** treat the combined distributed binary as a **GPLv3 work** and use Qt under terms compatible with that (GPL-licensed Qt use, or LGPL compliance with static-link obligations — see LS-82). The chosen path must be written in the decision record; do not ship without one. |
| LS-82 | If Qt is used under **LGPL v3** (dynamic or static), the release must satisfy LGPL v3 distribution requirements applicable to the actual link model — including license text, copyright notices, and (for static linking) the means for the user to relink against modified Qt, as required by LGPL. |
| LS-83 | If Qt is used under a **commercial Qt license**, the decision record must cite the agreement and the release must not claim GPLv3 covers Qt itself. |
| LS-84 | Installer and `{app}` folder must ship **Qt copyright and license notices** required by the chosen Qt path. Do not rely on "Qt is on the user's machine" when Qt is statically linked. |
| LS-85 | Record exact Qt **plugin** and **module** list in the decision record whenever `qt_import_plugins` or linked libraries change. |
| LS-86 | MinGW runtime statically linked into the executable must be accounted for in third-party notices (GCC runtime exception / GPL components as applicable). |


### 3.3 Other and future dependencies

| ID | Requirement |
|----|-------------|
| LS-90 | Any future C++ library (for example an archive reader) must have its license recorded in the decision record and in `THIRD-PARTY-NOTICES` before it ships in a release binary. |
| LS-91 | Vendored or sample third-party trees (`samples/`, icon fonts, walk fork) stay separated from the shipping binary. Do not copy vendored code into the Program without a license file and LEGALSPEC compliance review. |
| LS-92 | CI and developer tools (CMake, Inno Setup, aqt, Go toolchain) are not redistributed as part of the Program unless an installer explicitly bundles them; they are outside LS-50. |


## 4. Repository conduct and maintenance

| ID | Requirement |
|----|-------------|
| LS-100 | Do not commit secrets, private keys, or commercial license keys into the repository. |
| LS-101 | Do not commit Qt **source** or **static Qt kit** binaries into `codebase/`. External paths (for example `C:\cpp\qt-src`, `mingw_64_static`) are documented in impl/arch; only scripts and records live in Git. |
| LS-102 | When accepting contributions, ensure the contributor grants rights compatible with GPLv3 for first-party code. Use a contributor process recorded in `README.md` or `CONTRIBUTING.md` when the project opens to external patches. |
| LS-103 | `git` history must remain intact for GPL source-offer traceability. Do not distribute release binaries without a recorded commit hash in `build-meta.json` or release notes. |
| LS-104 | Agents and automation may update implementation docs and verification notes; they must not weaken LEGALSPEC without explicit owner approval in a `Mutabor` message or direct edit by the copyright holder. |
| LS-105 | [dispute.md](./dispute.md) tracks review debate; close licensing gaps by updating LEGALSPEC, the decision record, and shipping texts — not by marking dispute threads alone. |
| LS-106 | Periodic audit (each release): confirm `objdump` or equivalent import table matches the documented link model; confirm installer file list matches static or dynamic model; confirm no unexpected DLLs ship. |


## 5. Installer and installation notices

The Windows installer is built from `codebase/installer/Erase & Rewrite.iss` (Inno Setup).

| ID | Requirement |
|----|-------------|
| LS-110 | The installer **must** use `LicenseFile=EULA.txt` and present the license page in the wizard before installation proceeds. Silent installs must still be covered by organizational policy that accepts GPL; document silent-install assumptions in the decision record if used. |
| LS-111 | The user must see, before completing setup, that the **Program** is offered under GPLv3 and that installation constitutes use under those terms for the Program (see LS-40–LS-41). |
| LS-112 | The installer must install all files required by LS-50 into `{app}` (Program license, third-party notices, and the executable). For the static Qt build, `{app}` contains the monolithic `EraseAndRewrite.exe` plus license/notice text files — not a separate Qt DLL tree unless the dynamic link model is restored and recorded. |
| LS-113 | Build markers (`*.date`) and developer-only metadata must not be installed. The `[Files]` exclude rules in the ISS script must stay aligned with this (current: `Excludes: "*.date"`). |
| LS-114 | Start Menu and optional desktop shortcuts must launch `EraseAndRewrite.exe` directly (no wrapper script), consistent with techspec PL-07. |
| LS-115 | **Add/Remove Programs** must show the correct product name (Erase & Rewrite), version matching the built executable, and uninstall must remove `{app}` files the installer placed, except user data under `%LocalAppData%` that FS treats as user configuration. |
| LS-116 | Post-install run (`[Run]` launch offer) is optional UX; it must not skip or replace the license page. |
| LS-117 | If the installer is rebuilt for a release, `SourceDir` must be `bin/win/current` (or documented equivalent) with binaries and notice files that match the license decision record for that version. |
| LS-118 | Unsigned installers must be noted in release metadata (see techspec WR-04). Signing does not change GPL obligations. |
| LS-119 | Translated installers are not required today. If added, every translation must include the same legal meaning for GPL and third-party sections. |


## 6. Verification checklist (release gate)

Before declaring a Windows release compliant with LEGALSPEC:

1. `LICENSE` / `COPYING` present in repo; tag matches build.
2. `EULA.txt` matches link model and third-party list.
3. License decision record updated for this version.
4. Installer output contains GPLv3 + required third-party texts in `{app}`.
5. About dialog satisfies FS and LS-31.
6. PE `LegalCopyright` reviewed (LS-20).
7. Static build: no unexpected `Qt6*.dll` beside exe unless dynamic model documented.
8. Commit hash and version recorded in `build-meta.json` or release notes.

Record checklist completion in `docs/verification/` or release notes.


## 7. Document maintenance

When link models, installer layout, or third-party components change:

1. Update this file if normative rules change.
2. Update the license decision record.
3. Update `EULA.txt`, installer `[Files]` rules, and shipped notice files.
4. Update [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) BD-04 and [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §13 cross-references.

Do not edit [funcspec.md](./funcspec.md) for licensing alone; propose FS changes to the owner when user-visible About obligations need to expand.
