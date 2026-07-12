# Erase & Rewrite: Legal and Licensing Specification


## About This Document

### Identification

Document ID: LEGALSPEC.

Genre: normative legal and licensing specification for the product and its repository.

Scope: Erase & Rewrite (all delivery targets unless a target techspec documents a **narrower applicability statement** that does not reduce obligations imposed by applicable licenses — for example because a component or distribution method is absent on that target).


### Purpose

This document states the licensing model the Program must follow, the notices and texts users must receive, how third-party components must be handled in builds and releases, how the source repository must stay compliant, and what the Windows installer must present at installation time.

It does not replace FS rules for About-dialog product text. It adds obligations FS does not spell out.


### Target audience

Principal copyright holder, release engineers, contributors, reviewers, and agents preparing installers or dependency changes.


### Precedence

Interpretation order for licensing questions:

1. **Applicable law** and the **actual license terms** governing each distributed component — including GPLv3, LGPLv3, Qt license terms chosen for each module, GCC runtime exceptions, and other third-party licenses. No project document overrides these terms.
2. This document (LEGALSPEC).
3. [funcspec.md](./funcspec.md) (FS) for user-visible About content that FS defines.
4. Target techspec rows that cite licensing (for example BD-04 in [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md)).
5. [dispute.md](./dispute.md) and verification notes — informative history only.

If LEGALSPEC disagrees with a techspec licensing row, LEGALSPEC wins and the techspec row must be updated. A techspec row may state that a LEGALSPEC requirement is **not applicable** to a target (with reason); it must not purport to waive an obligation imposed by an applicable license.


### Prose standard

This document follows [XMSTP](https://github.com/aka-author/xmstp) (GRTD, GPIT, ICIT, en-words).


### How to read this document

Words like "must", "must not", and "should" carry their usual English force. "Must" means the release or repository is not compliant if the point is violated. "Should" is a strong recommendation that becomes "must" once a release candidate is tagged.


## Terms

*Program:* the Erase & Rewrite application and its first-party source code authored for this product.

*Linked component:* a third-party library or plugin whose object code is incorporated into the shipping executable through static or dynamic linking.

*Separately distributed component:* a third-party work shipped in the same distribution package but not treated as part of the Program's corresponding source (for example a distinct tool or script placed beside the executable without forming a covered combined work under GPLv3). LEGALSPEC does not decide legal "combination" from packaging alone; the license decision record must state the basis when unclear.

*Distribution package:* what an end user receives in one delivery (installer output, archive, or store download), including the Program binary, license texts, notices, and any separately distributed components.

*Principal copyright holder:* Mikhail Ostrogorskiy, or a successor recorded in `LICENSE-NOTICE` and release notices, for the Program as a whole.

*Contribution copyright:* copyright retained by each contributor in their own contributions unless a written assignment says otherwise.

*Orthodox GPL:* GNU General Public License version 3 (or, at the principal copyright holder's option, any later version published by the FSF), applied in canonical form to first-party Program code without proprietary exceptions.

*Contributor registry:* [../CONTRIBUTORS.md](../CONTRIBUTORS.md) — the canonical list of contributor display names and short descriptions of their inputs to the Program.

*License decision record:* [../verification/licensing-decision-record.md](../verification/licensing-decision-record.md) (or its successor) — the per-release inventory of components, license paths, link models, and evidence.


## 1. Cornerstone — Orthodox GPLv3

| ID | Requirement |
|----|-------------|
| LS-01 | The Program must be free software licensed under the **GNU General Public License version 3** (GPLv3), or (at the principal copyright holder's option) **any later version** of the GPL published by the Free Software Foundation. |
| LS-02 | First-party source code must not be distributed under terms that contradict GPLv3 copyleft (for example notices that deny the right to modify and redistribute the Program under GPL). |
| LS-03 | **Copyright attribution** must distinguish: (a) the **principal copyright holder** named in `LICENSE-NOTICE`, installer notices, and PE metadata; (b) **per-contribution copyright** retained by contributors unless assigned in writing; (c) **third-party copyright** preserved in `THIRD-PARTY-NOTICES` and component license texts. Accepting a GPLv3-compatible contribution does not by itself transfer copyright to the principal holder. |
| LS-04 | Every **binary distribution** of the Program must satisfy GPLv3 **section 6** for **Corresponding Source** of the Program. **Public Internet releases** must use GPLv3 **section 6d**: the binary download location must provide **equivalent access** to the exact Corresponding Source at no further charge, with **clear directions beside the binary**. Source access must remain available for as long as required to satisfy section 6d. At minimum the distributor must: (1) publish a **stable URL maintained for the required period** alongside the binary download (release page, installer metadata, or equivalent) that works **without authentication** for the public; (2) identify an **immutable** matching **tag or commit** that contains the complete Corresponding Source for that exact binary; (3) include in that source tree all material required by GPLv3 section 1 — including complete build and installation scripts, configuration needed to produce the binary from source, and `COPYING`; (4) record the chosen section 6 method in the license decision record. **Physical-media distribution** (if ever used) may instead follow GPLv3 **section 6b** (written source offer valid for at least three years and as long as spare parts or customer support are offered for that product model); that path must be explicitly recorded and must not be described as the ordinary Internet-release method. A vague "practical way" or undocumented repository tip is not sufficient. |
| LS-05 | GPLv3 **section 7** additional permissions or requirements may be used only when legally reviewed. They must **not** modify the text of `COPYING`. They must appear in a separate file (for example `ADDITIONAL-TERMS`) and/or in clearly identified source-file notices that state where additional terms apply, as GPLv3 section 7 requires. The default is no section 7 terms beyond GPLv3. |
| LS-06 | Patents: contributors grant rights consistent with GPLv3 section 11. Do not add separate patent licenses that weaken copyleft for first-party code. |


## 2. General requirements — product, notices, and documents

### 2.1 Repository license file

| ID | Requirement |
|----|-------------|
| LS-10 | The repository root under `codebase/` must contain `COPYING` with the **complete, unmodified text of GPLv3 only** — no copyright notice inserted into or presented as part of that text. Program copyright and the GPLv3-or-later application notice for the Program must appear in `LICENSE-NOTICE`, source-file headers, or another clearly separate section or file. A URL pointer alone does **not** satisfy the requirement to ship the full GPL text. |
| LS-11 | First-party source files should carry a short SPDX or copyright header referencing GPLv3-or-later. New files in `win-cpp-qt/` and `win-go/` must follow the same convention once established. |


### 2.2 Windows executable metadata

| ID | Requirement |
|----|-------------|
| LS-20 | PE `VERSIONINFO` string `LegalCopyright` must not be misleading. It must name the principal copyright holder and/or point to shipped license texts (for example "See COPYING and LICENSE-NOTICE in installation folder"). |
| LS-21 | `ProductName`, `FileDescription`, and `OriginalFilename` must match the shipping product identity (Erase & Rewrite / `EraseAndRewrite.exe`) and must not imply a proprietary license regime. |


### 2.3 About dialog

| ID | Requirement |
|----|-------------|
| LS-30 | The About dialog must satisfy FS *Displaying the Program Information* (product name and dotted version identical to PE File version). |
| LS-31 | The About dialog **must** display **Appropriate Legal Notices** for the Program as GPLv3 uses that term: copyright notice, statement that there is no warranty, statement that recipients may convey the Program under GPLv3, and indication of how to view the full license (for example a control opening `{app}\COPYING` or an in-dialog notice with the same substance). A release must not ship without this in-program path. |
| LS-32 | About text must not claim the Program is proprietary, trialware, or subject to restrictions beyond GPLv3. |


### 2.4 Installer license notice file

| ID | Requirement |
|----|-------------|
| LS-40 | `codebase/installer/INSTALL-LICENSE.txt` is the authoritative **installer legal notice** file referenced by Inno Setup `InfoBeforeFile` (see LS-120). It must include the Program copyright notice, GPLv3-or-later statement for the Program, and the **full GPLv3 text** (or the full text duplicated in `{app}\COPYING` with the installer file clearly pointing to it). Do not label this file an "EULA" in normative documents; it is a **license and notice** display, not a proprietary end-user contract. |
| LS-41 | The installer notice must **not** state that the user must accept contractual restrictions **beyond GPLv3** as a condition of **receiving or running** the Program (GPLv3 section 9). It may inform the user of license terms; it must not present GPLv3 as a click-through permission gate for mere installation or execution. |
| LS-42 | Third-party sections (Qt and future libraries) must be clearly separated from the Program GPL section and describe only that component's license and notices. |
| LS-43 | Changes to `INSTALL-LICENSE.txt` require a one-line entry in the license decision record with date and reason. |


### 2.5 Documents delivered to the user

| ID | Requirement |
|----|-------------|
| LS-50 | Every shipping installer must place in `{app}` at least: (1) `COPYING` with the full unmodified GPLv3 text for the Program, (2) `LICENSE-NOTICE` (or equivalent) with Program copyright and GPLv3-or-later application notice, (3) third-party license texts required by the license decision record, (4) `THIRD-PARTY-NOTICES` (or equivalent) listing every linked and separately distributed component with version and license. |
| LS-51 | `versioninfo.json` and `build-meta.json` are build metadata, not license documents. They may record version and commit for traceability but do not satisfy LS-04 or LS-50 alone. |
| LS-52 | README or in-app help may summarize licensing but must not contradict `COPYING`, `LICENSE-NOTICE`, or `INSTALL-LICENSE.txt`. |


### 2.6 Documents stored in the repository

| ID | Requirement |
|----|-------------|
| LS-60 | This file (`legalspec.md`) is normative for licensing process. |
| LS-61 | [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §13 records the **current** technical link model; it must stay consistent with the license decision record for the active release line. |
| LS-62 | Legal verification evidence lives under `docs/verification/` and is cited from the license decision record. |


## 3. Third-party components

### 3.1 Decision record (mandatory before each release)

| ID | Requirement |
|----|-------------|
| LS-70 | Before each release, maintain an approved **license decision record** under `docs/verification/`. For **every** linked component and **every** separately distributed component in the distribution package, the record must list: exact name and version; static or dynamic link model; exact license option relied upon (GPL, LGPL, commercial, other); whether modified; copyright and attribution text shipped; Corresponding Source or source-offer location; and (when LGPL static linking applies) relinking deliverables per LS-82. |
| LS-71 | Changing any third-party version, module, plugin set, or link model requires updating the decision record **before** the first public build with that change. |
| LS-72 | Each release must choose **exactly one** license basis per Qt module and plugin shipped. Mixed undocumented bases are not permitted. |


### 3.2 Qt

Normative requirements apply to whatever Qt version and module set the active release uses. The **exact** version, modules, plugins, and link model are recorded only in the license decision record and [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) — not in this file.

| ID | Requirement |
|----|-------------|
| LS-80 | Qt is a linked component governed by **its own** license terms unless the release record documents use of Qt under the **GPL v3 option** for open-source development, in which case Qt's obligations align with that choice. |
| LS-81 | For each release, the decision record must state the **single** chosen basis for each shipped Qt module and plugin: (a) Qt under **GPLv3** (open-source option), (b) Qt under **LGPLv3** with full compliance for the actual link model (LS-82), or (c) **commercial Qt license** with agreement reference (LS-84). Do not ship without a completed row for every Qt artifact in the binary. |
| LS-82 | **LGPLv3 static linking — compliance when LGPL is the recorded basis.** If any Qt component is recorded under LGPLv3 and linked statically, the distribution package must include a **verified relinking kit**, not merely **an** abstract promise. At least one of: (1) relinkable object files (`.o`/`.a`) for the Program's own code plus documented, reproducible link instructions sufficient to substitute a modified compatible Qt library; (2) another mechanism that demonstrably allows installation and use of a modified version of the linked Qt library per LGPLv3 section 4 and the combined-work rules. The distribution must also ship LGPL-required notices and installation information for that component. The chosen mechanism must be named in the decision record and present in the installer payload or documented offer. |
| LS-83 | **Alternative license basis — not LGPL compliance.** Using Qt under GPLv3 (open-source option) or under a commercial Qt license is **not** an LGPL static-linking compliance mechanism. Before distribution on that basis, the license decision record must record the change of license basis for each affected Qt component away from LGPL (per LS-81), and shipping notices must reflect the chosen basis (LS-84–LS-85). |
| LS-84 | **Commercial Qt:** the decision record must cite the agreement identifier; shipping notices must not claim GPLv3 covers Qt itself. |
| LS-85 | The distribution package must ship **Qt copyright and license notices** required by the chosen path. Static linking does not remove this obligation. |
| LS-86 | MinGW runtime components linked into the executable must appear in `THIRD-PARTY-NOTICES` with applicable license or exception text (for example GCC runtime library exception where it applies). |


### 3.3 Other and future dependencies

| ID | Requirement |
|----|-------------|
| LS-90 | Any future linked library (for example an archive reader) must have a complete decision-record row and `THIRD-PARTY-NOTICES` entry before it ships. |
| LS-91 | Vendored or sample trees (`samples/`, icon packs) stay separated from the shipping binary unless reviewed and listed in the decision record. |
| LS-92 | CI and developer tools (CMake, Inno Setup, aqt, Go toolchain) are not redistributed in the distribution package unless explicitly bundled; they are outside LS-50. |


## 4. Repository conduct and maintenance

| ID | Requirement |
|----|-------------|
| LS-100 | Do not commit secrets, private keys, or commercial license keys. |
| LS-101 | Do not commit Qt source trees or prebuilt Qt kits into `codebase/`. External build paths are documented in impl/arch; scripts and records live in Git. |
| LS-102 | When accepting outside contributions, follow §4.1 (contributor obligations and registry). Preserve **contribution copyright** (LS-03). Document merge policy in `CONTRIBUTING.md` when the project accepts external patches. |
| LS-103 | Preserve `git` history for source-offer traceability. Every public binary release must record the matching commit hash in `build-meta.json`, release notes, or the decision record. |
| LS-104 | Agents must not weaken LEGALSPEC without explicit owner approval (`Mutabor` message or direct edit by the principal copyright holder). |
| LS-105 | Close licensing gaps by updating LEGALSPEC, the decision record, and shipping texts — not by marking [dispute.md](./dispute.md) threads alone. |
| LS-106 | Each release audit: import table matches recorded link model; installer file list matches decision record; every binary component appears in `THIRD-PARTY-NOTICES`; attribution strings are preserved; [CONTRIBUTORS.md](../CONTRIBUTORS.md) is current for merged contributions in that release line; installer notice page verified as non-acceptance (`InfoBeforeFile`, LS-120). |


### 4.1 Contributors — obligations and registry

| ID | Requirement |
|----|-------------|
| LS-107 | Everyone who submits a **merged** change to first-party Program code or to normative product documentation in `docs/specs/` (other than FS, which is owner-edited) is a **contributor**. Before merge, the contributor must agree that the submission is licensed under **GPLv3-or-later** (or under a written copyright assignment if the project later adopts one). Submission implies the patent grants GPLv3 section 11 describes for contributors, to the extent the submission is merged. |
| LS-108 | The agreement in LS-107 must be **recorded** using at least one accepted mechanism before merge: (a) Developer Certificate of Origin (DCO) sign-off in the commit message (`Signed-off-by`); (b) pull-request checkbox or explicit in-thread statement that the submission is licensed under GPLv3-or-later; (c) signed contributor agreement on file; or (d) other recorded written approval referenced in the merge record or `docs/verification/`. Verbal or assumed consent is not sufficient. |
| LS-109 | The contributor must have the legal right to submit the material (original work, permission from upstream, or a compatible license). Do not submit proprietary, confidential, or third-party code without explicit permission and registry notes. |
| LS-110 | The repository must maintain [docs/CONTRIBUTORS.md](../CONTRIBUTORS.md). **Before or when** a non-trivial external contribution is merged, add or update an entry with: (1) **display name** — the contributor's real name **or** an anonymous pseudonym they choose; (2) a **short description** of what they contributed (area, feature, or document — not a full changelog). Optional: first/last contribution date or commit range. |
| LS-111 | **Display names** must not be offensive, discriminatory, harassing, impersonating, or otherwise unacceptable. The principal copyright holder may require a contributor to use their real name or choose a different pseudonym before merge. Anonymous contribution is allowed when the pseudonym meets this rule. |
| LS-112 | The principal copyright holder and long-standing maintainers must also appear in `CONTRIBUTORS.md` when their work is not solely attributed only in `LICENSE-NOTICE` — at minimum the principal holder entry must exist. |
| LS-113 | `CONTRIBUTORS.md` is part of the **Corresponding Source** (LS-04). It must be kept accurate; do not remove historical entries without legal reason — add corrections or successor notes instead. |


## 5. Installer and installation notices

The Windows installer is built from `codebase/installer/Erase & Rewrite.iss` (Inno Setup).

| ID | Requirement |
|----|-------------|
| LS-120 | The installer must display the GPL notice **before** file installation **without requiring acceptance** of additional contractual terms as a condition of installing or running the Program (GPLv3 section 9). Use Inno Setup `InfoBeforeFile=INSTALL-LICENSE.txt` — **not** `LicenseFile` — so the wizard shows a notice page with **Next** only (no "I accept" / "I do not accept" gate). The observable requirement is verifiable in a test install: proceeding must not be blocked on accepting GPLv3 as a contract. |
| LS-121 | The installer must **display or provide** the GPL notice and full Program license text before or during installation. Third-party sections must follow LS-42. |
| LS-122 | The installer must install all files required by LS-50 into `{app}`. The exact file set follows the link model in the decision record (monolithic executable and/or documented DLL layout). |
| LS-123 | Build markers (`*.date`) and developer-only metadata must not be installed (`Excludes: "*.date"` or equivalent). |
| LS-124 | Shortcuts must launch `EraseAndRewrite.exe` directly (techspec PL-07). |
| LS-125 | Add/Remove Programs must show the correct product name and version; uninstall removes installer-placed `{app}` files except user configuration under `%LocalAppData%` per FS. |
| LS-126 | Optional post-install launch must not replace the license notice page. |
| LS-127 | `SourceDir` for release builds must match the decision record (typically `bin/win/current` plus prepared notice files). |
| LS-128 | Unsigned installers must be noted in release metadata (techspec WR-04). |
| LS-129 | Translated installers, if added, must preserve legal meaning of GPL and third-party sections. |


## 6. Verification checklist (release gate)

Before declaring a Windows release compliant with LEGALSPEC, verify and record evidence under `docs/verification/`:

1. `COPYING` in repo contains **full unmodified GPLv3 only**; `LICENSE-NOTICE` (or equivalent) carries Program copyright and GPLv3-or-later application statement; release tag matches `build-meta.json` / release notes.
2. **Stable source URL** in release materials works **without authentication**, provides equivalent Corresponding Source access beside the binary per GPLv3 **section 6d**, and points to the **immutable** tag/commit.
3. **Corresponding Source** at that revision includes everything needed to reproduce the shipped Program binary (scripts, toolchain pins, configuration).
4. **(Project reproducibility — not a direct GPLv3 requirement.)** A test build from that source produces a functionally equivalent binary (or documented, justified delta).
5. `INSTALL-LICENSE.txt` is shown via `InfoBeforeFile` without an acceptance gate (LS-120); `{app}\COPYING` and `{app}\LICENSE-NOTICE` match the Program license; installer text complies with LS-41.
6. License decision record **approved** with one row per distributed component (LS-70, LS-72).
7. `THIRD-PARTY-NOTICES` matches every binary component; copyright and attribution preserved.
8. Qt obligations fulfilled for the **chosen single basis** per module (notices; **relinking kit** if LGPL static per LS-82; **basis change recorded** if GPLv3 or commercial per LS-83).
9. About dialog satisfies FS and LS-31 (Appropriate Legal Notices).
10. PE `LegalCopyright` reviewed (LS-20).
11. `objdump` or equivalent confirms link model (no unexpected Qt DLLs when static is recorded).
12. Installer `{app}` file list matches the decision record.
13. [CONTRIBUTORS.md](../CONTRIBUTORS.md) lists every contributor merged in the release line (LS-110–LS-113); LS-108 evidence exists for external merges.


## 7. Document maintenance

When link models, installer layout, or third-party components change:

1. Update this file if normative rules change.
2. Update and re-approve the license decision record.
3. Update `INSTALL-LICENSE.txt`, installer `[Files]` rules, and shipped notice files.
4. Update [techspec-win-cpp-qt.md](./techspec-win-cpp-qt.md) BD-04 and [impl-win-cpp-qt.md](./impl-win-cpp-qt.md) §13.
5. Update [CONTRIBUTORS.md](../CONTRIBUTORS.md) when merging external contributions (§4.1).

Do not edit [funcspec.md](./funcspec.md) for licensing alone; propose FS changes to the owner when user-visible About obligations need to expand.
