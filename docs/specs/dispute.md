# Review of the Windows C++/Qt Technical Specification and Architecture

## Review basis

This review covers:

- `techspec-win-cpp-qt.md`;
- `arch-win-cpp-qt.md`.

`funcspec.md` is treated as axiomatic. This review does not dispute, reinterpret, weaken, or replace any functional requirement. Any conflict between the reviewed documents and `funcspec.md` must be resolved by correcting the reviewed documents.

## Overall verdict

The proposed move to C++ and Qt is technically reasonable, and the documents correctly identify the Settings Form as the decisive migration constraint. The proposed GUI-thread/worker-thread split, immutable scan-result handoff, native Qt Widgets shell, CMake build, and explicit verification gates are generally sound.

The documents are not yet ready to govern implementation. Several provisions conflict directly with `funcspec.md`, and the architecture leaves important functional mechanisms unspecified. The most serious gaps concern network paths, configuration initialization, context-folder updates during navigation, the folder hierarchy descriptor, treemap clumping, and archive classification.

The implementation should not begin until the critical findings below are resolved in the documents.

## Critical findings

### 1. Network-path support contradicts the functional definition of a volume

**Affected provisions**

- `techspec-win-cpp-qt.md`: PL-03 and IO-04.
- `arch-win-cpp-qt.md`: folder selection and scanner design do not exclude network locations.

`funcspec.md` defines a volume as a local storage unit and explicitly excludes network locations. Nevertheless:

- PL-03 permits UNC paths “where applicable”;
- IO-04 defines behavior for unavailable network shares and mapped drives;
- the architecture proposes `QFileDialog::getExistingDirectory` without a required local-volume validation step.

This is not an additive platform rule. It expands the functional scope beyond the FS.

**Required correction**

- Remove UNC and network-share support from PL-03 and IO-04.
- Require validation that the selected scan root belongs to a supported local volume.
- Treat a network location as an invalid scan root and handle it through the FS-defined `#001` path.
- State explicitly whether mapped network drives are rejected by detecting the underlying drive type rather than relying on the presence of a drive letter.

### 2. Legacy configuration loading contradicts first-start configuration creation

**Affected provisions**

- `techspec-win-cpp-qt.md`: CF-03.
- `arch-win-cpp-qt.md`: Config load order.

`funcspec.md` requires the program, when `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt` does not exist, to create that file and write the built-in defaults into it.

CF-03 instead requires the program to load `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt` when the FS path is absent. The architecture repeats this as the second load source. Under that design, the mandatory FS-path file may not be created on first start.

**Required correction**

Remove CF-03 and the corresponding architecture load order, or redefine migration so that it cannot replace the FS-mandated first-start sequence. A compliant migration may import values into the newly created FS file, but the resulting behavior must still satisfy all of the following:

1. the FS configuration folder is used;
2. the FS configuration file is created when absent;
3. the built-in configuration is the defined initialization basis;
4. any imported values are validated under current FS rules before they replace defaults.

The migration mechanism must not preserve the legacy path as an alternative active configuration location.

### 3. The update architecture does not implement “scan the context folder while navigation remains available”

**Affected provisions**

- `arch-win-cpp-qt.md`: Session, Scanner, Concurrency and data ownership.

The FS requires Update to scan the current context folder. It also requires the target-folder descriptor to be updated and explicitly permits navigation while scanning is in progress.

The architecture says that the scanner walks the “context/target folder,” emits a new `FolderTree`, and that `MainWindow` replaces the current snapshot on success. This is insufficient and potentially wrong:

- it does not define the exact scan root for Update;
- it does not define how a subtree result is merged into the target-folder descriptor;
- it does not define what happens if the user navigates to another folder during the scan;
- it does not define how the current context is preserved after the updated subtree is installed;
- it does not define how path deletion or replacement during the scan affects the current navigation state;
- replacing the complete target snapshot with a tree rooted at the context folder would destroy the target hierarchy above that folder.

**Required correction**

Define Update as a versioned subtree transaction:

1. capture the update root as the context folder at command invocation;
2. scan that folder independently of later navigation;
3. retain the currently displayed immutable target snapshot while scanning;
4. on success, replace only the corresponding subtree in a new target-folder descriptor;
5. publish the new target descriptor atomically on the GUI thread;
6. resolve the user’s then-current context path against the new descriptor;
7. apply the FS-defined missing-folder behavior if that context no longer exists;
8. on interruption, keep both treemap data and displayed treemap unchanged.

The architecture must also define scan identifiers so that an obsolete worker cannot overwrite a newer target descriptor.

### 4. The proposed model does not contain the complete FS folder hierarchy descriptor

**Affected provision**

- `arch-win-cpp-qt.md`: Treemap model.

The architecture lists only sizes, volume shares, packing type, tree role, and enough data to display direct children. The FS requires the descriptor to provide, for every file system object recursively:

- name;
- full path;
- size;
- volume share;
- number of nested folders;
- number of nested files;
- creation date and time of the oldest file;
- last update date and time of the newest file;
- type;
- list of nested file system objects;
- packing type;
- tree role.

The phrase “`FolderNode` equivalent” and reliance on the Go model are not sufficient. The Go model is not normative and may not contain the required fields.

**Required correction**

Define the C++ domain model explicitly. At minimum, the architecture must identify:

- the node type and all FS-required fields;
- the representation of files, folders, and clumps;
- timestamp semantics and propagation rules;
- nested file/folder count semantics;
- archive classification storage;
- error or unreadable-entry metadata that does not alter FS tree-role and packing-type values;
- ownership and immutability rules for published descriptors.

### 5. Treemap construction omits mandatory clumping and tile-selection rules

**Affected provisions**

- `arch-win-cpp-qt.md`: Treemap model and Treemap layout.
- `techspec-win-cpp-qt.md`: no additive clarification is provided.

The architecture jumps from direct children to a squarified layout. It does not define the mandatory descriptor-to-treemap projection required by the FS:

- exclusion of non-positive or non-finite values;
- `treemap.maxTiles` limit;
- selection of the largest `maxTiles - 1` entries;
- residual clump construction;
- `treemap.clumpThreshold` processing;
- combination of both clumping rules;
- clump size, packing type, color class, label, tooltip, and context-menu behavior;
- stable handling of ties;
- `treemap.minTileWidth` and `treemap.minTileHeight`.

Without a defined projection stage, the layout cannot be implemented consistently.

**Required correction**

Introduce a separate `TreemapProjection` subsystem between the descriptor and layout. It must convert the direct children of the context folder into the exact vector of represented values required by the FS. The architecture must define the processing order for threshold clumping and maximum-tile clumping so that the result is deterministic and testable.

### 6. Archive classification has no implementable design

**Affected provisions**

- `arch-win-cpp-qt.md`: `ArchiveClassifier` and Scanner.
- `techspec-win-cpp-qt.md`: BD-03 and BD-05.

The architecture states that `.zip` and `.rar` catalogs will be examined, but no mechanism is selected. Qt Core, Gui, and Widgets do not provide general ZIP and RAR catalog classification. The documents do not specify:

- the archive library or libraries;
- supported archive variants;
- behavior for encrypted archives;
- behavior for corrupt or multipart archives;
- catalog-only operation versus extraction;
- filename encoding;
- cancellation and timeout behavior;
- resource limits for hostile or extremely large catalogs;
- the exact algorithm that distinguishes packed file, packed folder, and packed clump.

This is a required FS capability, not an optional implementation detail.

**Required correction**

Select and pin the archive-reading technology before coding. Define a catalog-only classifier with bounded resource consumption and explicit classification rules. The scanner must not extract archive contents merely to classify an archive.

### 7. IO-02 assigns the wrong outcome to permission failures

**Affected provision**

- `techspec-win-cpp-qt.md`: IO-02.

The FS explicitly states that failure to collect data on some inner files or folders because of insufficient permissions is still a successful scan when scanning finishes organically.

IO-02 requires the user to be told that the run was “incomplete.” That wording changes the FS outcome. A successful scan with inaccessible entries may include a warning or count only if it does not cause the program to classify the result, treemap, or use case as incomplete or negative.

**Required correction**

Rewrite IO-02 so that:

- inaccessible inner entries are recorded and not silently represented as empty;
- the scan remains successful when it finishes organically, exactly as required by FS;
- any user-facing notice does not call the scan or treemap incomplete and does not replace FS-defined alerts;
- root-folder access failure remains `#001`;
- a technical interruption remains `#002`.

### 8. Reparse-point policy is not technically adequate

**Affected provisions**

- `techspec-win-cpp-qt.md`: IO-03.
- `arch-win-cpp-qt.md`: Scanner.

The architecture proposes the same “visited canonical path set” used by the Go target. A normalized path string is not a reliable identity for a Windows file-system object. Junctions and symbolic links can expose the same directory through different paths, and mount points can cross volumes.

**Required correction**

Choose one complete policy:

- skip all directory reparse points and record the reason; or
- follow selected reparse-point types using cycle detection based on stable Windows file identity, such as volume serial number plus file ID.

The policy must also define how followed mount points affect current-volume capacity and volume-share calculations. A path-string set alone must not be presented as sufficient cycle detection.

## Significant findings

### 9. The technical specification contains an obsolete Qt 5 high-DPI requirement

**Affected provision**

- `techspec-win-cpp-qt.md`: DP-03.

DP-03 requires explicit use or documentation of `AA_EnableHighDpiScaling` and `AA_UseHighDpiPixmaps`. In Qt 6, high-DPI scaling is enabled by default, Qt 6 is Per-Monitor DPI Aware V2 on Windows by default, and the Qt documentation states that the old scaling flag is not needed.

**Required correction**

Replace DP-03 with a Qt 6-specific requirement:

- do not disable Qt 6 high-DPI behavior;
- use device-independent widget coordinates;
- provide multi-resolution image assets through `QIcon` or SVG resources;
- recompute cached treemap geometry and typography when the effective screen or DPI changes;
- verify behavior at the FS-required scales and across monitors.

### 10. The architecture arbitrarily fixes MSVC although the techspec does not

**Affected provisions**

- `techspec-win-cpp-qt.md`: PL-04 permits a recorded compiler toolchain.
- `arch-win-cpp-qt.md`: Build and release states “CMake + MSVC + Qt 6.”

No architectural reason is given for making MSVC mandatory. The architecture should either explain a real dependency on MSVC or remain toolchain-neutral within the technical specification.

**Required correction**

State the selected compiler in a toolchain decision record, including Qt distribution availability, Windows API compatibility, packaging, debugging, and CI implications. Do not turn an example from PL-04 into an unexplained architectural constraint.

### 11. The product name is inconsistent with the FS

**Affected provisions**

- Titles and several legacy references in both reviewed documents use “Erase & Rewrite.”

The FS defines the product name as `WhatToWipe`, including the About dialog and configuration paths. Even where the old name is intended only as historical context, repeated use creates a high risk of leaking it into PE metadata, installer names, executable descriptions, paths, and UI text.

**Required correction**

Use `WhatToWipe` throughout current-target normative and architecture text. Keep the old name only inside a clearly marked migration-history note where it is strictly necessary.

### 12. The architecture relies too heavily on the Go implementation as a behavioral baseline

**Affected provisions**

- Treemap model;
- Config;
- Reparse policy;
- Relationship to Go implementation;
- testing against old configuration samples.

The FS is the behavioral source of truth. The Go implementation is known to be incomplete and is the reason for the migration. Statements such as “port field semantics,” “same as the documented win-go choice,” “format 1:1,” and “behavioral baseline” risk carrying defects into the new target.

**Required correction**

Use the Go implementation only as a source of reusable code ideas and regression fixtures. Every ported behavior must be traced independently to an FS requirement or to a valid additive techspec requirement.

### 13. Error handling is not designed as a coherent subsystem

**Affected provisions**

- `arch-win-cpp-qt.md`: `ErrorDialogs` and Shell helpers.

The architecture mentions handling `#003/#004` in shell helpers but does not define a complete mapping for `#001`, `#002`, `#003`, and `#004`, nor the required error-alert and interruption-alert presentation.

**Required correction**

Define a small error domain with:

- stable FS error codes;
- parameterized FS messages;
- one alert presenter that enforces the required icons, focus, buttons, and keyboard behavior;
- a separate interruption-alert presenter;
- explicit mapping from folder selection, scanning, existence checks, Explorer launch, and file opening to FS outcomes.

### 14. “Cancel-with-dirty confirmation” adds behavior not required by FS

**Affected provision**

- `arch-win-cpp-qt.md`: Settings validation and actions.

The FS says Cancel closes the form without applying pending edits. The architecture adds a confirmation when the form is dirty. This may be acceptable as an additional safeguard, but it is not derived from the FS and changes the interaction sequence.

**Required correction**

Either remove the confirmation or explicitly classify it as a proposed product change requiring an FS amendment. The architecture must not silently introduce user-visible behavior while claiming that FS alone defines product behavior.

### 15. Settings-grid implementation is over-prescribed in the techspec and inconsistent in the architecture

**Affected provisions**

- `techspec-win-cpp-qt.md`: SG-01 and SG-02.
- `arch-win-cpp-qt.md`: acceptable Qt patterns.

The techspec strongly directs implementation toward `QTableView` plus permanent widgets. The architecture later also accepts one shared `QGridLayout`. Both can satisfy the FS when implemented correctly. The normative requirement should describe compliance properties, not select a widget mechanism unless there is a demonstrated reason.

`setIndexWidget` is also presented as the simplest primary solution without addressing scrolling, ownership, focus traversal, row resizing, model resets, or accessibility. With only the current fixed number of rows, it may still be viable, but the decision needs an actual prototype and acceptance evidence.

**Required correction**

Keep the techspec at the FS-compliance level. Put the selected Qt control strategy in the required verification declaration after a working prototype demonstrates:

- permanent visible editors;
- shared columns;
- correct focus and keyboard traversal;
- correct resizing and scrolling;
- color composite cells;
- no editor loss after model changes;
- compliance on a clean Windows VM.

### 16. Configuration validation and persistence need a transaction design

**Affected provisions**

- `arch-win-cpp-qt.md`: Config and Settings validation.

“Atomic save” is stated but not designed. The architecture does not define:

- parsing invalid existing files;
- unknown keys;
- duplicate keys;
- preservation or rejection of comments and formatting;
- temporary-file write and atomic replacement;
- recovery after a failed write;
- validation of cross-field constraints such as minimum versus maximum font size;
- application rollback if persistence succeeds but live application fails, or vice versa.

**Required correction**

Define a configuration transaction:

1. parse into a typed candidate object;
2. validate every field and cross-field constraint;
3. serialize the complete candidate;
4. write to a temporary file in the same directory;
5. flush and atomically replace the active file;
6. publish the new immutable effective configuration to the UI;
7. leave both disk configuration and live configuration unchanged on any failure before commit.

### 17. Licensing requirement BD-04 is too vague and partly conflates deployment modes

**Affected provision**

- `techspec-win-cpp-qt.md`: BD-04.

The requirement mixes LGPL deployment, object-file obligations, and commercial licensing in one example. Obligations differ materially between dynamic linking, static linking, modified Qt libraries, and a commercial Qt license.

**Required correction**

Replace BD-04 with a requirement to record the selected Qt licensing and linking model and provide the exact compliance artifacts required for that model. Do not imply that one generic package satisfies every Qt licensing scenario.

## Minor corrections

### 18. The architecture should not call the layout “squarified” merely because it descends from the Go code

The FS requires a flat space-filling treemap and states that the algorithm should minimize extreme aspect ratios. The architecture should define measurable layout acceptance criteria and select an algorithm that meets them. Reusing an algorithm family name is not evidence of compliance.

### 19. The settings row count should not be hard-coded as architectural truth

The architecture says the current form has 32 rows. This count can become stale whenever the FS parameter table changes. The implementation and tests should derive the row set from the schema or an explicitly maintained FS-to-schema mapping.

### 20. `QDesktopServices::openUrl` and `ShellExecuteW` are not interchangeable without defined behavior

The architecture should select one Windows shell-opening mechanism for folders and one for files, then define success detection and FS error mapping. “Use A or B” leaves materially different behavior unresolved.

## Recommended document changes before coding

1. Remove network-path support and define local-volume validation.
2. Remove or redesign legacy configuration migration so the FS file is always created and used.
3. Define Update as a context-subtree transaction merged into the target descriptor while navigation continues.
4. Specify the complete FS folder hierarchy descriptor.
5. Add a `TreemapProjection` subsystem for threshold clumping, maximum-tile clumping, and clump semantics.
6. Select and document ZIP/RAR catalog libraries and classification rules.
7. Correct permission-failure outcome wording.
8. Replace path-string reparse detection with a defensible Windows policy.
9. Correct Qt 6 high-DPI requirements.
10. Remove unexplained MSVC lock-in or justify it in a toolchain decision.
11. Normalize the product name to WhatToWipe.
12. Make FS traceability authoritative over Go behavioral parity.
13. Define complete error, configuration-transaction, and shell-integration designs.
14. Prototype and verify the settings grid before fixing the final Qt control strategy.

## Conclusion

The C++/Qt direction is viable, and the Settings Form strategy is much more promising than the previous Win32/walk attempts. The current documents, however, still combine migration notes, normative constraints, inherited Go behavior, and unresolved design choices. Correcting the findings above will produce a coherent implementation baseline that obeys `funcspec.md` rather than merely porting the previous application into another GUI framework.
