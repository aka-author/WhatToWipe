# True Grid Implementation — Related Sources

This file lists all source files related to the current Windows true-grid/property-sheet implementation for treemap settings.

## Primary UI Implementation

- `win-go/internal/ui/settings_treemap_windows.go`
- `win-go/internal/ui/settings_treemap_mirror_windows.go`
- `win-go/internal/ui/run_windows.go`

## Architecture Modules Added for This Implementation

- `win-go/internal/ui/row_schema.go`
- `win-go/internal/ui/settings_state.go`
- `win-go/internal/ui/config_mapper.go`
- `win-go/internal/ui/validation.go`

## Supporting UI/Platform Files Referenced

- `win-go/internal/ui/fontlist_windows.go`

## Config Contract and Persistence (must remain 1:1 compatible)

- `win-go/internal/config/treemap.go`
- `win-go/internal/config/file.go`

## Architecture and Requirements Docs Used

- `docs/specs/arch-for-true-grid-win-go.md`
- `docs/guides/win/win-go-editable-property-grids.md`
- `docs/specs/funcspec.md`

