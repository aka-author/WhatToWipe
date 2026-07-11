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
