# IO-03 reparse-point policy (Phase 1)

## Chosen strategy

**Do not traverse** directory reparse points (junctions, directory symlinks, mount points). Record the entry in the descriptor tree without following the link target.

This matches the documented Go skip strategy and techspec IO-03.

## Descriptor semantics

| Field | Value for untraversed reparse entry |
|-------|-------------------------------------|
| `traversalState` | `ReparseTargetNotTraversed` |
| `treeRole` | `NodeFolder` |
| `measuredSize` | `0` |
| `sizeCompleteness` | `Complete` |

`TreeRole::EmptyFolder` is **not** used for reparse entries. `recomputeAggregates()` preserves `ReparseTargetNotTraversed` and does not treat the node as a known-empty folder.

## Size rule (v1)

The represented size of an untraversed reparse directory entry is **zero**. Linked-target contents are excluded from parent `measuredSize` because the scanner does not enter the target subtree through the reparse entry.

If the same target path is reachable as a normal directory child elsewhere in the walk, that reachable copy is enumerated independently.

## Detection

`platform::DirEntry` sets `isReparsePoint` from `FILE_ATTRIBUTE_REPARSE_POINT`. `ScanWorker::isDirectoryReparsePoint()` requires both directory and reparse attributes before applying skip semantics.

## Evidence

- `win-cpp-qt/tests/test_phase1.cpp`: `reparse_entry_traversal_state`, `reparse_target_nonempty_excluded`
- `docs/specs/impl-win-cpp-qt.md` §6.2
