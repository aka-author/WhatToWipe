# Reviewer Reply to the Developer Response

## Basis

This document continues the review of:

- `techspec-win-cpp-qt.md`;
- `arch-win-cpp-qt.md`.

`funcspec.md` remains axiomatic. The purpose of this reply is not to reopen settled requirements, but to determine whether the developer’s proposed corrections actually satisfy them.

## General response

The developer response is constructive and accepts the substance of the review. Most findings can now be closed once the promised changes are made in the technical specification and architecture.

Acceptance in `dispute.md`, however, is not itself a correction. Every accepted finding remains open until the governing documents contain an implementable replacement and the obsolete text has been removed. In several places the response proposes a direction that is better than the original text but still needs adjustment.

The most important remaining points are:

- local-volume validation must reject network volumes without accidentally rejecting other valid local volume types;
- legacy configuration import must not replace the exact first-start sequence required by FS;
- inaccessible inner entries must not introduce a new user-visible “incomplete” state or warning requirement;
- reparse-point handling is not solved merely by deciding to skip everything;
- the current product must be named WhatToWipe in deployment metadata as well as UI/configuration unless another normative product requirement says otherwise;
- treemap layout quality must not be converted from an FS “should” into an undocumented hard rejection threshold;
- shell failures must map to the exact FS outcomes, and file-open failures must not be assigned folder error codes.

## Reply by finding

### 1. Network paths

**Developer verdict:** Accept.

**Reviewer reply:** Agreed, with one correction to the proposed validation rule.

The response proposes accepting only `DRIVE_FIXED` and `DRIVE_REMOVABLE`. That is narrower than the FS definition. The FS excludes network locations, but it does not say that every other locally accessible volume type is invalid. For example, a locally mounted optical volume or RAM-backed volume may still satisfy the FS definition of a volume and may be readable.

The implementation should validate the actual volume that contains the selected folder and reject network-backed roots, not maintain an arbitrary allowlist unless the FS or techspec deliberately introduces one.

**Recommended Windows rule**

1. Resolve the volume root for the selected folder.
2. Determine its Windows drive type.
3. Reject `DRIVE_REMOTE`.
4. Reject unresolved, nonexistent, or inaccessible roots through `#001`.
5. Permit other local volume types when the selected folder is readable and the required capacity/free-space data can be obtained.
6. If a particular local volume type is intentionally unsupported, add an explicit additive techspec requirement and justification rather than hiding the restriction in implementation code.

Mapped network drives must be rejected as network locations even though they have drive letters.

**Status:** Accepted in principle; proposed fix needs the adjustment above.

### 2. Legacy configuration import

**Developer verdict:** Accept.

**Reviewer reply:** The proposed one-time import is reasonable, but its sequence still needs to follow the FS literally.

The response says that when the FS file is absent, the program may read the legacy file and write the resulting merged values into a newly created FS file. The FS states a more specific sequence: create the FS file and write the built-in default configuration into it.

The safest compliant migration is therefore:

1. detect that the FS file is absent;
2. create the FS directory;
3. create the FS file;
4. write the built-in defaults to it;
5. finish the mandatory FS initialization successfully;
6. optionally detect the legacy file;
7. parse and validate legacy values into a separate candidate configuration;
8. merge only valid recognized values over the defaults;
9. atomically save the imported candidate to the FS file;
10. use only the FS file from that point onward.

This preserves the exact FS initialization and still allows migration. It also gives a clean recovery state: if legacy parsing or the second save fails, a valid default FS configuration already exists.

An even simpler alternative is to omit automatic migration completely and provide no legacy behavior. That is fully compliant and may be preferable if there are few existing installations.

**Status:** Accepted, subject to an FS-first initialization sequence.

### 3. Updating the context folder while navigation remains available

**Developer verdict:** Accept.

**Reviewer reply:** Agreed. The proposed versioned subtree transaction is the correct direction.

The architecture should make the following data explicit:

- `scanId`;
- `scanKind` (`OpenTarget` or `UpdateContext`);
- captured `scanRootPath`;
- captured base descriptor version;
- current published target descriptor;
- current context path, stored independently of the scan root.

A successful update should be merged only if it still applies to the intended target session. A scan result from a previous target-folder session must be discarded even if its numeric scan ID is otherwise current.

The merge operation should be a pure function:

```text
mergeSubtree(oldTargetDescriptor, scanRootPath, newSubtree)
    -> newTargetDescriptor | merge failure
```

Navigation during scanning reads only the old published descriptor. After publication, the current context path is resolved against the new descriptor. No widget should retain raw pointers into the replaced tree.

One nuance: the FS permits navigation during Update, but it does not permit starting another scan simultaneously. The UI state rules should continue to prevent a second Open or Update while the first scan is active.

**Status:** Accepted; close after the architecture contains the transaction and session-version rules.

### 4. Complete folder hierarchy descriptor

**Developer verdict:** Accept.

**Reviewer reply:** Agreed, with one model-boundary clarification.

The folder hierarchy descriptor contains file system objects. A clump is a treemap representation object, not necessarily a file system object in that descriptor. Therefore:

- files and folders belong to the scanned descriptor model;
- clumps belong to the treemap projection model;
- archive packing type belongs to descriptor entries;
- projected clumps calculate their own aggregate packing category as required for display.

The architecture should not force clumps into the persistent descriptor merely because tiles need them.

For every descriptor node, define the FS fields directly. Avoid a loose “equivalent to Go” statement. Also define empty timestamp behavior. For example, an empty folder has no oldest or newest file; this should be represented explicitly, such as `std::optional<QDateTime>`, rather than by a magic epoch value.

Nested file/folder counts also need an exact interpretation. The natural implementation is recursive descendant counts, because the FS says “number of nested” objects, but the architecture should state this rather than leave each developer to guess.

**Status:** Accepted with clarified separation between descriptor and projection models.

### 5. Treemap projection and clumping

**Developer verdict:** Accept.

**Reviewer reply:** Agreed. The architecture must specify the order, not merely mention that the Go code contains similar logic.

A deterministic projection algorithm that satisfies the current FS is:

1. take the descriptor entries immediately inside the context folder;
2. exclude values that are zero, negative, or non-finite as required by the treemap representation rules;
3. place every entry below `treemap.clumpThreshold` of the context-folder total into the clump candidate;
4. sort the remaining entries by descending size, with a documented deterministic tie-breaker such as normalized full path;
5. determine whether a clump tile is required because either the threshold group is non-empty or the remaining regular entries exceed the available tile count;
6. when a clump is required, retain at most `treemap.maxTiles - 1` regular entries and merge every other entry into one clump;
7. when no clump is required, retain at most `treemap.maxTiles` regular entries;
8. calculate the clump size and volume share as the sum of all represented members;
9. pass the resulting flat vector to the layout engine.

This produces one clump tile, not separate threshold and overflow clumps.

The architecture must also define interaction for a clump. Since a clump is not one file or folder, it cannot support Dive, Explore, or Open as though it were one file system object. The cursor and context menu therefore need an explicit FS-consistent rule. If the FS does not define that behavior sufficiently, flag it for clarification instead of inventing a folder action.

`treemap.minTileWidth` and `treemap.minTileHeight` also need a defined role. They must not silently distort proportional area. A reasonable solution is to use them as a projection/layout feasibility threshold: values that cannot receive a usable tile may join the clump before final layout, provided the resulting behavior is made deterministic and remains consistent with the FS residual-tile rule.

**Status:** Accepted; projection order and clump interaction remain to be specified.

### 6. Archive classification

**Developer verdict:** Accept; libarchive proposed as a candidate.

**Reviewer reply:** Agreed that a library decision is required. “Candidate: libarchive” is not yet enough for the techspec.

Before selecting it, make a small proof of capability against the exact required cases:

- ordinary ZIP with one file;
- ordinary ZIP with one top-level folder;
- ZIP with several top-level entries;
- empty ZIP;
- encrypted ZIP;
- corrupt ZIP;
- ordinary RAR and RAR5 equivalents;
- multipart RAR;
- archive with path traversal names;
- archive with a very large catalog but no extraction.

The classifier should operate on catalog entries only. It should normalize separators and ignore archive container metadata entries that do not represent user files or folders.

A practical classification rule is:

- exactly one effective top-level file and no top-level folder → packed file;
- exactly one effective top-level folder containing every effective entry → packed folder;
- every other readable catalog → packed clump;
- unavailable catalog, encrypted catalog that cannot be listed, unsupported variant, corrupt catalog, timeout, or resource-limit breach → packed clump, as required by FS.

Possible implementation choices include:

- one library supporting both ZIP and RAR catalogs;
- separate small libraries for ZIP and RAR;
- a narrowly wrapped external archive library behind an `IArchiveCatalogReader` interface.

The interface should prevent the rest of the scanner from depending on library-specific entry types. Pin the selected version in CMake and record its license obligations together with Qt licensing.

**Status:** Accepted; remains open until a tested library and classification contract are selected.

### 7. Inner permission failures

**Developer verdict:** Accept with nuance; proposes a non-blocking status-bar count or warning.

**Reviewer reply:** I do not accept the proposed user-visible warning as a technical-specification requirement.

The FS explicitly declares organic completion with inaccessible inner entries to be success. It also specifies that when the treemap is complete, the status bar displays the context-folder path. Requiring an appended warning or count introduces new visible product behavior and risks obscuring the mandated status.

The technical need is narrower:

- do not represent an unreadable subtree as a known empty subtree;
- record the failure internally in scan diagnostics;
- finish successfully if the scan otherwise completes organically;
- publish a complete treemap according to the FS definition of success;
- do not raise `#002`;
- do not label the result incomplete.

If product stakeholders want users to see unread-path counts, that belongs in `funcspec.md`. Until then, the architecture may retain diagnostics for logs, tests, or support, but the techspec should not require a new status-bar message.

An alternative that does not change the final UI state is a transient progress/status message during scanning, followed by restoration of the mandatory context path when scanning completes. Even that should be treated as optional unless the FS is amended.

**Status:** Original finding remains open. Remove the mandatory user-visible notice from IO-02.

### 8. Reparse points

**Developer verdict:** Accept; proposes skipping all directory reparse points.

**Reviewer reply:** The path-string approach must indeed be removed. The proposed blanket skip is safe against cycles, but it is not automatically sufficient for FS compliance.

The FS requires recursive scanning of inner file system objects. A directory reparse point is still an object in the selected hierarchy. The documents must define what information is collected for that entry and why its target contents are or are not treated as inner objects.

There are three defensible designs:

**A. Do not traverse directory reparse points**

- represent the reparse entry itself in the descriptor;
- record that its target was not traversed;
- do not classify it as an ordinary known-empty folder;
- define its size and tree role using a documented Windows-specific interpretation;
- state that linked target contents are not treated as physically nested contents of the selected hierarchy.

This is safest, but the size/tree-role interpretation may require an FS clarification.

**B. Follow only same-volume links**

- resolve the target;
- require the target to remain on the current volume;
- identify visited directories by volume serial number plus file ID;
- prevent duplicate traversal and cycles;
- define whether repeated links contribute size once or per directory entry.

This is more complete but much more complicated.

**C. Follow no reparse targets and exclude them from the treemap projection**

This is simplest technically but is the hardest to reconcile with the requirement to scan all inner file system objects. It should not be selected without explicit FS support.

My recommendation is A for the first release, accompanied by a precise platform interpretation in the techspec and a verification case. Do not merely say “skip and record reason”; define the descriptor values produced for the skipped entry.

**Status:** Accepted that the old policy is invalid; replacement policy still requires design.

### 9. Qt 6 high DPI

**Developer verdict:** Accept.

**Reviewer reply:** Agreed.

Use Qt 6 defaults, device-independent coordinates, `QIcon`/SVG or proper multi-resolution raster resources, and invalidate all geometry/font caches when the effective screen or DPI changes. Avoid obsolete Qt 5 application attributes.

The verification plan should include moving the live window between monitors with different scaling, not only launching separate screenshots at several scale settings.

**Status:** Accepted.

### 10. Compiler toolchain

**Developer verdict:** Accept.

**Reviewer reply:** Agreed. The architecture should not silently mandate MSVC.

The decision record should compare at least:

- MinGW-w64 GCC;
- LLVM/MinGW or clang-cl, if considered;
- MSVC only if a concrete dependency or delivery constraint justifies it.

Relevant criteria are availability of the required Qt 6 LTS build, ABI compatibility with selected archive libraries, Windows resource generation, debugging, CI reproducibility, installer packaging, and binary size.

A reasonable default for a non-Microsoft toolchain is Qt 6 with MinGW-w64, provided every selected dependency is available for the same ABI. Mixing MSVC-built and MinGW-built C++ libraries is not acceptable.

**Status:** Accepted; close after a toolchain record is added.

### 11. Product name

**Developer verdict:** Accept with nuance; proposes retaining “Erase & Rewrite” as deployment branding in PE metadata and installer filenames.

**Reviewer reply:** I do not accept that distinction under the current normative set.

The FS defines the product as WhatToWipe and explicitly requires the About dialog to show WhatToWipe. Nothing in the FS or current techspec establishes a second deployment brand. PE `ProductName`, `FileDescription`, installer display name, Start-menu entry, uninstall entry, and executable naming are user-visible product identity. Calling those “deployment branding” does not remove the inconsistency.

The current target should therefore use WhatToWipe consistently for:

- document titles;
- About text;
- configuration folder and filename;
- PE `ProductName` and `FileDescription`;
- installer display name;
- Start-menu and uninstall entries;
- default executable and artifact names, unless a separate normative naming requirement exists.

The old name may appear only in migration detection code, historical notes, or tests that read legacy artifacts.

If the product owner genuinely wants “Erase & Rewrite” as the shipping brand, amend `funcspec.md` or add another explicit normative product-naming document with clearly defined precedence. Architecture notes cannot create that dual identity themselves.

**Status:** Original finding remains open. Use WhatToWipe consistently unless the normative product definition changes.

### 12. Go implementation as baseline

**Developer verdict:** Accept.

**Reviewer reply:** Agreed.

The phrase “behavioral parity” should be replaced by “FS conformance, with regression comparison where useful.” Parity with a known non-compliant implementation is not a release criterion.

A porting table may remain, but each row should identify:

- reusable implementation concept;
- applicable FS sections;
- known Go deviations that must not be copied;
- verification required for the Qt replacement.

**Status:** Accepted.

### 13. Error handling subsystem

**Developer verdict:** Accept.

**Reviewer reply:** Agreed.

Keep domain outcomes separate from presentation. For example:

```text
Domain result -> FS error/interruption classification -> alert presenter
```

The scanner should not directly construct `QMessageBox` instances. It should return a typed result such as success, voluntary interruption, technical interruption, or root unavailable. The GUI layer maps that result to the exact FS use-case branch and alert.

The custom icons required by FS should be application resources. Do not assume standard Qt message-box icons happen to match the specified colored rectangles.

**Status:** Accepted.

### 14. Cancel with dirty settings

**Developer verdict:** Accept; remove confirmation.

**Reviewer reply:** Agreed. Cancel closes without applying pending edits. No extra confirmation should be introduced by architecture.

**Status:** Accepted.

### 15. Settings-grid implementation

**Developer verdict:** Accept; move concrete choice out of techspec until prototype verification.

**Reviewer reply:** Agreed.

For this fixed, modest-sized settings form, a single `QGridLayout` inside a `QScrollArea` may be simpler and more reliable than `QTableView::setIndexWidget`:

- one layout owns all rows;
- column boundaries are shared globally;
- editors are permanent widgets in cells;
- color editor, swatch, and picker can occupy separate shared columns;
- focus order is explicit;
- no model reset can destroy cell widgets;
- accessibility is straightforward.

This is a real grid under the FS definition, provided there is exactly one shared grid and no independent row layouts.

`QTableView` remains viable if the prototype proves permanent editors, scrolling, focus, resizing, accessibility, and stable ownership. It should not be selected merely because its class name contains “Table.”

The required pre-coding declaration should name the chosen control after the prototype, not list multiple alternatives.

**Status:** Accepted.

### 16. Configuration transaction

**Developer verdict:** Accept.

**Reviewer reply:** Agreed. In Qt, `QSaveFile` is the natural implementation candidate for temporary-file write plus commit/replace behavior.

The transaction boundary should be:

1. create typed candidate from editor values;
2. validate field constraints;
3. validate cross-field and whole-object constraints;
4. serialize the entire configuration;
5. persist through `QSaveFile` or an equivalently safe same-directory replacement mechanism;
6. only after successful commit, publish the new immutable effective configuration;
7. rebuild/repaint the treemap;
8. on any failure, retain the previous disk file and effective in-memory configuration.

Unknown and duplicate keys in an existing file need an explicit policy. A conservative policy is:

- unknown keys preserved when safely possible or reported and ignored;
- duplicate known keys rejected as invalid rather than resolved by accidental last-write-wins behavior;
- malformed known values replaced only through the FS-defined default/recovery policy, which must be documented.

Do not claim transactional behavior if the implementation first mutates the live configuration and then attempts disk persistence.

**Status:** Accepted.

### 17. Qt licensing

**Developer verdict:** Accept.

**Reviewer reply:** Agreed.

The simplest open-source deployment model is normally dynamic linking against LGPL Qt libraries, with replaceable Qt DLLs and the required license notices/texts. Object-file relinking obligations become especially relevant to static linking and should not be presented as a universal dynamic-deployment requirement.

The decision record must state:

- commercial or LGPL model;
- dynamic or static linking;
- whether Qt itself is modified;
- exact Qt modules shipped;
- corresponding notices, source-offer or relinking obligations where applicable;
- installer placement of license texts;
- archive-library licensing as a separate item.

Do not postpone this until packaging is complete, because the linking model affects the build from the beginning.

**Status:** Accepted.

### 18. Squarified layout and aspect ratios

**Developer verdict:** Accept; proposes layout acceptance criteria based on FS aspect-ratio guidance.

**Reviewer reply:** Agreed that the algorithm name is not proof, but do not accidentally strengthen the FS.

The FS says the algorithm **should** minimize extreme aspect ratios and gives `5:1` as an example. That is not currently a hard universal prohibition against every tile above `5:1`; some data distributions and container dimensions make such tiles unavoidable.

Use measurable quality tests instead of a false binary gate. For example:

- compare the chosen implementation against a reference squarified algorithm on fixed datasets;
- record maximum and percentile aspect ratios;
- verify proportional area within rounding tolerance;
- verify deterministic output;
- verify no overlaps or gaps outside pixel-rounding rules;
- investigate avoidable extreme rectangles.

A hard `5:1` rejection rule would require an additive normative techspec requirement and feasibility analysis.

**Status:** Accepted with this qualification.

### 19. Settings row count

**Developer verdict:** Accept.

**Reviewer reply:** Agreed. Derive rows from one typed parameter schema that is traceable to the FS table. Tests should compare the schema’s user-editable key set against the expected FS key set so a newly added parameter cannot silently disappear from the form.

**Status:** Accepted.

### 20. Shell-opening API and error mapping

**Developer verdict:** Accept; recommends `ShellExecuteExW` for folders and files with mapping to `#003/#004`.

**Reviewer reply:** Selecting `ShellExecuteExW` is reasonable, but the proposed error mapping is too broad.

The FS mappings are specific:

- `#003` applies when a folder cannot be opened in File Explorer;
- `#004` applies when a folder of interest does not exist;
- executable files have **Open** disabled according to `treemap.win.exeFiles`;
- non-executable file Open uses the associated application;
- the FS does not assign `#003` or `#004` to every failure to open a file.

Therefore:

**Folder Explore**

1. check existence through the FS Checking a Folder of Interest use case;
2. missing folder → `#004`;
3. existing folder but Explorer launch fails → `#003`;
4. success → keep the busy pointer for the FS-required two seconds.

**File Open**

1. determine executability from `treemap.win.exeFiles` before enabling the command;
2. executable → command disabled, no shell launch;
3. non-executable → invoke the associated application;
4. if the file vanished, follow whatever FS result is defined for this use case; do not reuse folder-only `#004` without an FS basis;
5. association/launch failure must remain a negative file-open result unless the FS adds a dedicated error.

The wrapper should return a typed outcome rather than a Boolean so the use-case layer can apply the correct branch.

**Status:** Accepted API direction; error mapping must be corrected.

## Additional observations arising from the response

### A. “All critical findings accepted” does not mean coding may start

The response correctly says the critical items should be fixed before coding. Keep that gate. In particular, archive classification, subtree merging, descriptor fields, projection order, and reparse semantics are implementation-shaping decisions. Deferring them would cause incompatible data models and rework.

### B. Verification notes must not become a shadow specification

Several responses propose recording final choices under `docs/verification/`. That is suitable for evidence and decision records, but behavior required for implementation belongs in the techspec or architecture. A verification note may record which permitted option was selected; it should not be the only place where essential runtime semantics are defined.

### C. Use requirement identifiers for new normative rows

When correcting the techspec, add or revise identifiable requirements rather than burying obligations in prose. The following areas deserve explicit rows:

- local-volume validation;
- reparse policy;
- archive-catalog technology and limits;
- Qt 6 DPI behavior;
- configuration transaction;
- selected licensing/linking model evidence.

Architecture mechanisms need not all become normative, but every normative platform restriction should remain traceable.

## Final disposition

| # | Finding | Reviewer disposition after response |
|---|---------|---------------------------------------|
| 1 | Network paths | Accepted direction; broaden local-volume rule |
| 2 | Legacy config | Accepted direction; create defaults before import |
| 3 | Update subtree | Accepted |
| 4 | Descriptor completeness | Accepted; keep clumps in projection model |
| 5 | Treemap projection | Accepted; order and interactions still required |
| 6 | Archive classification | Accepted; library proof and contract still required |
| 7 | Permission failures | Developer nuance rejected; no mandatory new warning |
| 8 | Reparse policy | Old policy rejected; replacement still unresolved |
| 9 | Qt 6 DPI | Accepted |
| 10 | Toolchain | Accepted; record decision, prefer non-MS option absent need |
| 11 | Product name | Developer nuance rejected; use WhatToWipe consistently |
| 12 | Go baseline | Accepted |
| 13 | Error subsystem | Accepted |
| 14 | Cancel confirmation | Accepted; remove it |
| 15 | Settings grid | Accepted; single `QGridLayout` is strong alternative |
| 16 | Config transaction | Accepted; `QSaveFile` suggested |
| 17 | Licensing | Accepted |
| 18 | Layout quality | Accepted; do not turn “should” into absolute `5:1` gate |
| 19 | Row count | Accepted |
| 20 | Shell API | API accepted; error mapping needs correction |

## Required next step

Revise `techspec-win-cpp-qt.md` and `arch-win-cpp-qt.md` themselves. After those edits, perform a fresh consistency review against `funcspec.md`. Do not treat this exchange as a substitute for correcting the governing documents.

---

## Developer reply to reviewer (2026-07-11)

This section responds to **Reviewer Reply to the Developer Response** above. Prior sections of this file are left unchanged. Acceptance here remains conditional on landing the promised text in `techspec-win-cpp-qt.md` and `arch-win-cpp-qt.md`.

### General reply

Agreed: `dispute.md` is not a specification. The governing documents must be revised; this thread records agreement only.

On the six remaining points from the general response:

1. **Local-volume validation** — Accept the reviewer's correction. Reject `DRIVE_REMOTE` only; do not use a fixed/removable allowlist unless an additive techspec row is added later.
2. **Legacy import** — Accept the FS-first ten-step sequence (create defaults, then optional import merge).
3. **Permission failures** — Withdraw the status-bar count proposal. IO-02 will not require any new user-visible warning after scan success.
4. **Reparse points** — Accept that "skip all" is insufficient without descriptor semantics. Adopt design **A** for first release (see finding 8 below).
5. **Product name** — Withdraw the deployment-branding distinction. WhatToWipe everywhere normative identity applies, including PE and installer, unless FS is amended.
6. **Layout quality** — Accept: FS "should" on aspect ratio is not a hard `5:1` rejection gate in techspec.

### Reply by finding

#### 1. Network paths — **Accept reviewer's adjustment**

`DRIVE_FIXED` + `DRIVE_REMOVABLE` only was too narrow. Revised rule: resolve volume root, reject `DRIVE_REMOTE` and mapped network drives, reject inaccessible roots via `#001`, permit other local volume types when readable and capacity data is obtainable. Any intentional exclusion of a local type gets an explicit techspec row.

#### 2. Legacy configuration — **Accept FS-first sequence**

Adopt the reviewer's steps 1–10 verbatim. Built-in defaults are written to the FS file before any legacy merge attempt. If import fails, a valid default FS file already exists. Automatic migration may still be omitted entirely if product owner prefers; that remains a valid compliant option.

#### 3. Update subtree transaction — **Accept**

Will specify in architecture: `scanId`, `scanKind` (`OpenTarget` | `UpdateContext`), `scanRootPath`, base descriptor version, published target descriptor, context path independent of scan root. `mergeSubtree(old, scanRoot, newSubtree) -> new | failure`. Discard scan results from a prior target session. No second Open/Update while scanning. Navigation reads old descriptor until atomic publish; no raw pointers into replaced trees.

#### 4. Descriptor vs projection — **Accept**

Clumps live in `TreemapProjection`, not the persistent folder hierarchy descriptor. Descriptor holds files and folders with all FS fields. Timestamps as `std::optional<QDateTime>`; empty folders have no oldest/newest. Nested file/folder counts are recursive descendant counts, stated explicitly in architecture.

#### 5. Treemap projection — **Accept**

Will adopt the reviewer's nine-step deterministic algorithm in architecture. One combined clump tile (threshold + overflow), not separate clumps. Will define clump interaction: no Dive/Explore/Open as a single file system object; cursor `default` or `not-allowed` per FS; context menu empty or absent for clump unless FS is clarified. `minTileWidth` / `minTileHeight` used as projection feasibility thresholds before layout, with deterministic merge into clump when a tile cannot be usable — not silent area distortion. Flag clump menu behaviour for FS clarification if still ambiguous after draft.

#### 6. Archive classification — **Accept**

libarchive remains a candidate, not a selection. Will run the reviewer's proof matrix (ZIP/RAR variants, encrypted, corrupt, multipart, traversal names, large catalog) before pinning in techspec. Classification contract: packed file / packed folder / packed clump / fallback packed clump per reviewer's rules. Scanner depends on `IArchiveCatalogReader`, not libarchive types directly. Version and licence recorded in CMake and licensing decision record.

#### 7. Permission failures — **Accept reviewer rejection of my nuance**

Withdraw status-bar warning or count as a techspec requirement. IO-02 will require: record unread entries internally; do not represent unread subtrees as empty; organic completion remains success; complete treemap published; no `#002`; no "incomplete" labelling. Diagnostics may exist for logs/tests/support only. Transient scan-path status during scan is already FS-permitted; final status shows context path only.

#### 8. Reparse points — **Accept; adopt design A**

Reject path-string cycle detection. First-release policy: **do not traverse directory reparse points**; represent the reparse entry in the descriptor; record that target was not traversed; define size and tree role under a documented Windows interpretation in techspec; linked target contents are not nested contents of the selected hierarchy. Verification case required. Design B deferred unless FS/product requires it later.

#### 9. Qt 6 high DPI — **Accept**

As stated. Verification includes live window move between monitors at different scale.

#### 10. Toolchain — **Accept**

No silent MSVC mandate. Decision record will compare MinGW-w64 GCC, clang-cl if considered, and MSVC only with stated justification. Default assumption: Qt 6 + MinGW-w64 if all dependencies (including archive library) share ABI. No mixed MSVC/MinGW linking.

#### 11. Product name — **Accept reviewer rejection of my nuance**

Withdraw "Erase & Rewrite" as deployment branding in normative target docs. WhatToWipe consistently for About, config, PE `ProductName`/`FileDescription`, installer display name, Start menu, uninstall entry, and default executable naming. Legacy name only in migration detection, historical notes, and legacy-artifact tests. If shipping brand must differ, that requires FS amendment — not architecture alone.

#### 12. Go baseline — **Accept**

Replace "behavioral parity" with "FS conformance, with regression comparison where useful." Porting table rows: reusable concept, FS sections, known Go deviations not to copy, Qt verification.

#### 13. Error handling — **Accept**

Domain result → FS classification → alert presenter. Scanner returns typed outcomes; GUI maps to FS branches. Custom FS-specified alert icons as application resources.

#### 14. Cancel confirmation — **Accept**

No dirty confirmation on Settings Cancel.

#### 15. Settings grid — **Accept**

Techspec stays at FS-compliance properties. `QGridLayout` in `QScrollArea` is the leading prototype candidate per reviewer (shared columns, permanent editors, color columns, no model-reset risk). `QTableView` remains fallback only if prototype fails. Pre-coding declaration names one chosen control after prototype pass.

#### 16. Configuration transaction — **Accept**

Seven-step boundary as reviewer states; `QSaveFile` as implementation candidate. Unknown keys: preserve when safe or ignore with documented policy. Duplicate known keys: reject. No live-config mutation before successful disk commit.

#### 17. Qt licensing — **Accept**

Decision record before build: commercial vs LGPL, dynamic vs static, modified Qt or not, modules shipped, notices/source-offer/relinking as applicable, installer placement, archive-library licence separate. Default open-source path: dynamic LGPL Qt unless record says otherwise.

#### 18. Layout quality — **Accept qualification**

Measurable quality tests (max/percentile aspect ratio, area proportion tolerance, determinism, no gaps/overlaps) — not a universal hard `5:1` rejection without additive techspec.

#### 19. Settings row schema — **Accept**

One typed schema traceable to FS; tests compare editable key set to FS table.

#### 20. Shell API and errors — **Accept corrected mapping**

`ShellExecuteExW` with typed outcomes. Folder Explore: existence check → `#004` if missing, `#003` if Explorer fails, busy pointer 2s on success. File Open: executability from `treemap.win.exeFiles` disables command; non-executable uses association; do not apply folder `#004`/`#003` to file open without FS basis; launch failure stays negative file-open result unless FS adds a code.

### Reply to additional observations

**A. Coding gate** — Agreed. Archive library, subtree merge, descriptor fields, projection order, and reparse semantics must be in techspec/arch before `win-cpp-qt/` starts.

**B. Verification vs specification** — Agreed. Required runtime semantics live in techspec or architecture. `docs/verification/` holds evidence, decision records, and which permitted option was selected — not the sole home of implementable rules.

**C. Requirement identifiers** — Agreed. Corrections will add or revise identifiable techspec rows for local-volume validation, reparse policy, archive technology, Qt 6 DPI, configuration transaction, and licensing/linking evidence.

### Developer disposition after reviewer reply

| # | Topic | Developer disposition |
|---|-------|----------------------|
| 1 | Network paths | Accept; reject `DRIVE_REMOTE` only |
| 2 | Legacy config | Accept; defaults before import |
| 3 | Update subtree | Accept |
| 4 | Descriptor | Accept; clumps in projection |
| 5 | Projection | Accept; nine-step order + clump interaction TBD/clarify |
| 6 | Archives | Accept; proof matrix before pin |
| 7 | IO-02 | Accept; no mandatory post-scan warning |
| 8 | Reparse | Accept; design A for v1 |
| 9 | DPI | Accept |
| 10 | Toolchain | Accept; MinGW default absent MSVC need |
| 11 | Name | Accept; WhatToWipe everywhere |
| 12 | Go baseline | Accept |
| 13 | Errors | Accept |
| 14 | Cancel | Accept |
| 15 | Settings grid | Accept; `QGridLayout` leading candidate |
| 16 | Config txn | Accept |
| 17 | Licensing | Accept |
| 18 | Layout | Accept; no false hard 5:1 gate |
| 19 | Schema | Accept |
| 20 | Shell | Accept; corrected error map |

### Next step (developer)

Revise `techspec-win-cpp-qt.md` and `arch-win-cpp-qt.md` to reflect this thread. Then request a fresh FS consistency review. `win-cpp-qt/` implementation remains blocked until that revision lands.

---

## 21. Product name — FS amended to Erase & Rewrite (2026-07-11)

**Owner decision:** The shipping product name is **Erase & Rewrite** everywhere normative identity applies: FS, About dialog, main window title, PE `ProductName`/`FileDescription`, configuration folder/file, and Qt implementation strings.

**Executable filename:** `EraseAndRewrite.exe` (no spaces or `&` in the PE file name).

**Legacy:** Interim Qt builds used `WhatToWipe` / `WhatToWipe.config.txt`; that path remains for one-time import only. Historical dispute entries above that mandated WhatToWipe are superseded by this FS amendment.

**Qt `&` escaping:** Display strings use `Erase & Rewrite`; menu/action labels that include the product name must use `Erase && Rewrite` where Qt treats `&` as a mnemonic marker.

**Status:** Accepted. Implemented in `funcspec.md`, `techspec-win-cpp-qt.md`, `arch-win-cpp-qt.md`, and `win-cpp-qt/`.

---

## Strict C++/Qt implementation review (2026-07-11)

### Scope and verdict

This section reviews the implementation currently present under `win-cpp-qt/` on the `dev` branch. Earlier lines in this file are intentionally untouched.

The implementation has a recognizable structure and several good choices: Qt widgets stay on the GUI thread, scan results are handed back as value objects, configuration writes use `QSaveFile`, shell operations return typed outcomes, and the Settings form uses permanent in-cell widgets. However, the current code is not release-ready and is not yet a reliable implementation of the FS. Several defects are release blockers because they produce wrong data, violate explicit use-case behavior, or make cancellation and timeout guarantees illusory.

### Release blockers

#### 22. Update navigation is disabled although the FS explicitly permits it

`MainWindow::onUp()` and `MainWindow::onDive()` both return immediately while `m_session.scanning` is true. `updateChrome()` also disables **Up** during every scan.

This contradicts the FS rule that navigation is allowed while *Updating the Context Folder* is in progress. The architecture discussion correctly required navigation to continue against the old published descriptor until the updated subtree is atomically published, but the implementation blocks it.

**Required correction**

- Distinguish `OpenTarget` from `UpdateContext` in command availability.
- During `OpenTarget`, navigation remains unavailable because there is no complete published treemap.
- During `UpdateContext`, keep **Up**, folder-tile Dive, Explore, and other FS-available navigation actions enabled against the old published tree.
- Continue to disable a second Open or Update operation.
- After the update is published, resolve the then-current context path against the new tree.

#### 23. The directory-read timeout and cancellation mechanism can still block forever

`ScanWorker::readDirBounded()` launches `QDir::entryInfoList()` through `std::async`. On cancellation or timeout it returns without calling `future.get()`. For a future created by `std::async(std::launch::async, ...)`, destruction of the last future may wait for the asynchronous task to finish.

Therefore the apparent 30-second timeout is not a real bound. The function can still block at return while the directory operation remains stuck. The same problem affects cancellation.

It also creates one new native thread for every directory enumeration. A large tree can create a very large number of short-lived threads, with needless scheduler and stack overhead.

**Required correction**

Do not wrap each `QDir` enumeration in a disposable `std::async` task.

Preferred alternatives:

1. Use native Windows enumeration with `FindFirstFileExW` / `FindNextFileW`, explicit cancellation checks between entries, and a documented policy for operations that the OS itself does not make cancellable.
2. Use one dedicated scanner thread and accept that an individual local filesystem call is not forcibly cancellable, while keeping network roots excluded. Document the real cancellation boundary honestly.
3. If a separate I/O worker is retained, it must have controlled lifetime and must never leave an unjoinable or implicitly joining task behind.

Do not claim a bounded wait unless the implementation can actually enforce it.

#### 24. Unreadable directories are silently represented as empty directories

`QDir::entryInfoList()` returns an empty list both for a genuinely empty directory and for several failure cases. `readDirBounded()` returns only a vector plus a timeout flag; it does not return an access/error status. `scanDir()` therefore has no reliable way to distinguish:

- empty directory;
- access denied;
- sharing failure;
- invalid path;
- transient I/O failure.

Such a node is then recomputed as `TreeRole::EmptyFolder`. This violates the settled rule that an unreadable subtree must not be represented as a known empty subtree.

**Required correction**

Return a typed directory-enumeration result, for example:

```cpp
struct DirectoryReadResult {
    QVector<QFileInfo> entries;
    DirectoryReadStatus status;
    DWORD nativeError;
};
```

The scanner must retain diagnostic metadata for unreadable entries, keep organic completion successful where FS requires it, and avoid asserting `EmptyFolder` when emptiness was not established.

#### 25. Reparse points are explicitly misclassified as empty folders

For a directory reparse point, `ScanWorker` sets:

```cpp
reparse.reparseSkipped = true;
reparse.treeRole = TreeRole::EmptyFolder;
```

Then `recomputeAggregates()` recomputes every child with no collected children/files as `EmptyFolder` anyway. This is exactly the representation that the earlier design discussion rejected. “Target not traversed” does not mean “known empty.”

The value `fi.size()` for a directory reparse point is also not a meaningful recursive folder size and cannot be treated as the size of the linked contents.

**Required correction**

Add an explicit descriptor state for untraversed reparse entries or otherwise model scan completeness separately from `TreeRole`. Do not use `EmptyFolder` as an error/skip sentinel. Ensure `recomputeAggregates()` preserves the selected reparse semantics rather than overwriting them.

#### 26. The ZIP central-directory parser reads the wrong fields

`readZipCentralDirectory()` is not merely incomplete; its field offsets are wrong.

After reading the four-byte central-file-header signature, the code seeks 18 bytes and then reads a two-byte value called `externalAttr`. In the ZIP central-directory structure:

- file-name length is at offset 28 from the header start;
- extra-field length is at offset 30;
- comment length is at offset 32;
- external file attributes are four bytes at offset 38;
- the local-header offset is at offset 42;
- file-name bytes begin at offset 46.

The current code reads unrelated bytes as lengths and attributes, then advances to the wrong location before reading the name. Ordinary ZIP files are likely to be rejected or misclassified.

Additional defects include no ZIP64 support, no general-purpose flag/encoding handling, no central-directory bounds checks, no resource limits, and no proof against hostile lengths.

**Required correction**

Delete this handwritten parser unless it is replaced by a fully tested, bounds-checked ZIP parser. Use the previously agreed archive-reader abstraction backed by a pinned library that passes the ZIP/RAR proof matrix.

#### 27. RAR classification is not implemented

Every `.rar` file is returned as `PackedClump` without attempting to read an available catalog. The FS requires the packing type to be detected when the archive catalog is available.

This is a direct functional failure, not a permissible fallback.

**Required correction**

Implement RAR and RAR5 catalog inspection through the selected archive library. `PackedClump` is the fallback only when catalog inspection is unavailable or fails under the defined rules.

#### 28. Minimum tile dimensions are calculated and then ignored

`MainWindow::rebuildTreemap()` computes `minW` and `minH` from configuration and passes them to `squarify()`. `TreemapLayout::squarify()` immediately discards both values with `Q_UNUSED`.

Consequently `treemap.minTileWidth` and `treemap.minTileHeight` have no effect. The required residual/clump behavior for undersized allocations is absent.

**Required correction**

Implement the FS residual-tile cycle:

1. project threshold/max-count clumps;
2. create a preliminary proportional layout;
3. detect allocations below the configured external-rectangle minima;
4. merge those represented values into the single clump;
5. recompute until stable or until no further legal merge is possible.

Area proportions must be recalculated after every merge. Do not enlarge small rectangles independently, because that destroys quantitative area encoding.

#### 29. Zero-size values are included and assigned positive area

The FS excludes zero, negative, and non-finite values. `TreemapProjection` does not remove zero-size entries. `TreemapLayout` then replaces every size with `max(size, 1)`, so a zero-size object receives positive area.

This is an explicit contradiction of the treemap representation rules.

**Required correction**

Filter non-positive values in the projection stage. The layout engine must not invent positive weights for invalid values.

#### 30. File timestamps do not implement the descriptor semantics

For every file, the scanner assigns `fi.lastModified()` to both `oldestFile` and `newestFile`. The FS requires:

- creation date/time of the oldest file;
- last-update date/time of the newest file.

The oldest timestamp must be based on file creation/birth time, not modification time. Assigning one modification timestamp to both fields produces incorrect folder aggregates.

**Required correction**

Use the Windows creation timestamp (`QFileInfo::birthTime()` where reliable, or native file information) for the oldest-file calculation and last-modified time for the newest-file calculation. Define fallback behavior when creation time is unavailable.

### Configuration and settings defects

#### 31. File loading supports only a subset of FS TSize syntax

`ConfigStore::parsePt()` accepts only integer `pt` values. The Settings parser accepts `pt` and `mm`, but not `px`, `cm`, or `in`. The FS defines all five suffixes and allows decimal magnitudes.

Examples such as `3.5mm`, `12px`, `1cm`, and `0.5in` are therefore rejected, truncated, or silently replaced by defaults depending on the entry path.

**Required correction**

Create one shared TSize parser used by file loading, Settings validation, serialization, comparison, and rendering. Preserve the unit or convert through one documented high-precision internal representation. Do not maintain separate parsers with different accepted syntax.

#### 32. Zero padding cannot be loaded from the configuration file

FS allows tile padding values `>= 0`. `parsePt()` returns zero for both valid `0pt` and invalid input, and each padding assignment uses `if (int n = parsePt(val); n)`, which ignores zero.

A valid `0pt` setting therefore cannot be loaded.

**Required correction**

Return parse success separately from the parsed numeric value. Never use zero as both a valid value and an error sentinel.

#### 33. Percentage parsing accepts invalid unitless values

The FS says a unitless Percentage must be strictly between zero and one. `parsePercent()` treats a unitless value greater than one as if it were a percentage and divides by 100. Thus `12.5` is accepted as `0.125`, although only `12.5%` or `0.125` are valid FS forms.

**Required correction**

- `%` suffix: parse percentage points and divide by 100.
- no suffix: require `0 < value < 1` exactly.
- reject all other forms.

#### 34. Invalid colors overwrite defaults with invalid `QColor` values

`applyTreemapLines()` assigns the result of `parseHex()` directly. When parsing fails, `parseHex()` returns an invalid `QColor`, which replaces the valid default. Other malformed fields are generally ignored instead.

This inconsistency can propagate invalid drawing colors into the treemap.

**Required correction**

Parse into a candidate, validate success, and only then assign. File loading needs the same typed all-field validation discipline as the Settings dialog.

#### 35. Duplicate keys are silently resolved by last-write-wins

The settled configuration design required duplicate known keys to be rejected. `applyTreemapLines()` loops through every line and overwrites the same field repeatedly.

**Required correction**

Track recognized keys while parsing. A second occurrence of a known key must produce a typed configuration error and invoke the documented recovery policy.

#### 36. Unknown configuration keys are destroyed on the next save

The parser ignores unknown keys, and `serializeTreemap()` writes only known keys. Therefore an Apply/OK operation rewrites the file and permanently removes every unknown extension, comment, and unsupported field.

The earlier agreed policy was to preserve unknown keys when safely possible or explicitly document another policy. The current implementation does neither.

**Suggestion**

Keep a parsed document model containing known fields plus passthrough lines, or explicitly adopt and document a destructive canonicalization policy. Silent loss is the worst option.

### Architecture and C++ risks

#### 37. The function called `squarify()` is not a squarified treemap algorithm

The implementation recursively bisects the value vector near half of the total and alternates split orientation according to the current rectangle. That is a binary partition treemap, not the standard squarified algorithm the name and architecture imply.

The algorithm may be acceptable only if measured against the FS quality goals, but the current name is misleading and no evidence is present.

**Required correction**

Either implement an actual squarified treemap or rename the algorithm accurately and provide aspect-ratio, determinism, area, gap, and overlap tests.

#### 38. Integer area arithmetic can overflow

Several totals and products use signed `qint64`, including:

```cpp
leftSum * 2
area.width() * aSum
```

A descriptor can aggregate very large logical sizes. Signed overflow is undefined behavior in C++. Even if current physical volumes rarely reach the limit, corrupted metadata or future storage sizes should not make layout arithmetic undefined.

**Suggestion**

Use checked unsigned arithmetic or normalized floating/long-double weights for layout ratios. Add overflow tests.

#### 39. The update transaction captures identifiers but does not actually validate them in the result callback

`startScan()` captures a local `scanId` only for thread-pointer cleanup. `onScanFinished()` receives no scan ID, no target-session ID, and no base descriptor version. It reads the current mutable session fields instead.

Today Open/Update are disabled during a scan, which reduces exposure, but this does not implement the versioned transaction promised by the architecture and becomes unsafe as soon as Update navigation and richer session behavior are restored.

**Required correction**

Include `scanId`, target-session ID, scan kind, scan root, and base descriptor version in the result object or capture them in a connection-specific lambda. Validate them before applying any result.

#### 40. `rootUnavailable` and `technicalFailure` are inferred from tree shape and English text

The worker determines outcomes using conditions such as:

```cpp
tree.children.empty() && tree.files.empty() && !tree.skipReason.isEmpty()
tree.skipReason.contains("timed out")
```

Domain outcomes must not depend on whether an English diagnostic contains a substring. An unreadable root, an empty root, a cancelled root, and a timed-out root require explicit statuses.

**Required correction**

Return a typed `ScanResult` with an enum and structured diagnostics. Keep presentation text outside the scanner.

#### 41. Sorting is not deterministic for equal-size files and folders

The scanner sorts files and child folders only by descending size. Equal-size objects can appear in implementation-dependent order. Projection later adds a path tie-breaker only for retained candidates, but descriptor ordering and other consumers remain unstable.

The FS allows implementation-defined tie ordering, but deterministic ordering is preferable for repeatable tests and stable UI.

**Suggestion**

Use normalized full path as a secondary key consistently.

#### 42. The Settings grid implementation is heavy and fragile

The form creates four index widgets for every row on a `QTableView` whose model supplies no display data and no editable flags. This can pass the literal permanent-editor requirement, but it pays for a model/view control while bypassing most of the model/view machinery.

Risks include focus traversal surprises, accessibility gaps, editor ownership complexity, repaint cost, and loss of widgets if the model is reset. For the fixed schema, the previously discussed single `QGridLayout` in a `QScrollArea` remains simpler and easier to prove correct.

This is not an automatic rejection if manual acceptance passes, but the implementation needs explicit keyboard, resize, scroll, accessibility, and clean-VM evidence.

#### 43. Hard-coded dialog size caps are likely to fail DPI and localization scenarios

The Settings dialog clamps width to 620 and height to 520 device-independent units. Long translated labels, larger system fonts, accessibility text scaling, and high-DPI environments may require more space. Arbitrary maximums are not derived from the FS.

**Suggestion**

Use layout size hints, available-screen bounds, and persisted geometry constrained to the current screen. Avoid fixed global caps unless usability testing justifies them.

### Build and release risks

#### 44. The CMake target still uses the old internal project/target name without an explicit mapping note

The current FS amendment makes **Erase & Rewrite** the product and `EraseAndRewrite.exe` the required executable, so the target name itself is acceptable. However, the build must prove that PE metadata, About version, installer identity, and artifact naming all derive from one version/product source. CMake currently declares `VERSION 1.0.0`, while the FS About format requires four numeric components.

**Required correction**

Define one four-component version source and generate CMake, `VERSIONINFO`, About output, installer metadata, and artifact metadata from it. Add an automated equality check.

#### 45. Static MinGW runtime flags need deployment and licensing verification

The target statically links libgcc, libstdc++, and winpthread while dynamically linking Qt. This may be a valid deployment choice, but the comment claiming that deployed `libstdc++-6.dll` “only serves Qt6*.dll” is not a substitute for binary dependency inspection.

**Suggestion**

Run `objdump -p`, `ntldd`, or equivalent on the final executable and every Qt DLL. Record the actual dependency set and applicable runtime-license notices. Verify exception propagation and allocator ownership do not cross incompatible runtime boundaries through third-party C++ libraries.

### Test debt

#### 46. No evidence is visible for the required implementation-shaping tests

The code reviewed here needs, at minimum:

- cancellation during deep scan;
- genuinely stuck or slow enumeration behavior;
- denied inner directory versus empty directory;
- directory reparse point descriptor semantics;
- Update navigation and atomic subtree publication;
- target deletion during Open and context deletion during Update;
- zero-size exclusion;
- maxTiles + threshold + minimum-dimension clumping combinations;
- area conservation and no overlap/gap tests;
- ZIP/RAR classification matrix;
- malformed, duplicate, unknown, and mixed-unit configuration files;
- cross-monitor DPI movement;
- permanent Settings editor keyboard traversal;
- About/PE four-part version equality.

Until those tests or equivalent verification artifacts exist, the implementation should be treated as an early prototype, not a candidate shipping line.

### Priority order

1. Replace the scan I/O/status design: real typed outcomes, honest cancellation behavior, unreadable-directory distinction.
2. Restore navigation during Update and implement result identity/version checks.
3. Replace archive parsing with a proven ZIP/RAR catalog reader.
4. Correct projection/layout: exclude zero values, implement minimum-size clumping, and verify area behavior.
5. Correct descriptor timestamps and reparse semantics.
6. Unify and harden configuration parsing for every FS type and transaction rule.
7. Complete version, deployment, licensing, DPI, and Settings acceptance evidence.

### Final implementation verdict

The repository has progressed beyond a throwaway spike, but it has not yet reached a trustworthy architectural baseline. The most dangerous defects are not cosmetic: the timeout is not bounded, unreadable directories become empty folders, archive classification is broken/incomplete, Update navigation violates FS, minimum tile constraints are ignored, and zero-size entries receive area.

Do not polish the UI around these defects. Fix the scanner, data semantics, archive reader, and treemap projection/layout first. Then run a fresh FS-to-code verification pass before declaring the Qt implementation active.
