# True Grid Implementation — Related Sources (Current)

This list reflects the **current** table-grid attempt and removed legacy mirror code.

## Primary UI Implementation

- `win-go/internal/ui/settings_treemap_windows.go`
- `win-go/internal/ui/run_windows.go`

## Settings Data/Rules Modules

- `win-go/internal/ui/row_schema.go`
- `win-go/internal/ui/settings_state.go`
- `win-go/internal/ui/config_mapper.go`
- `win-go/internal/ui/validation.go`

## Config Contract and Persistence

- `win-go/internal/config/treemap.go`
- `win-go/internal/config/file.go`

## Notes

- Removed from codebase: `win-go/internal/ui/settings_treemap_mirror_windows.go`
- Active unresolved issue: table values still not editable on target runtime.

## Architecture and Requirement Docs

- `docs/specs/arch-for-true-grid-win-go.md`
- `docs/guides/win/win-go-editable-property-grids.md`
- `docs/specs/funcspec.md`

