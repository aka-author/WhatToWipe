# Settings grid Qt control strategy (SG-09)

## Chosen approach

`QTableView` + `QAbstractTableModel` + **`setIndexWidget`** for every cell in a fixed **4-column** grid:

| Column | Content |
|--------|---------|
| 0 | Parameter name (`QLabel`, read-only) |
| 1 | Value editor (permanent `QLineEdit` or editable `QComboBox`) |
| 2 | Color preview swatch (`QLabel`) or empty placeholder cell |
| 3 | Color picker button (`QPushButton` "…") or empty placeholder cell |

Editors are created when the dialog opens and remain visible until it closes. There is no click-to-edit delegate, no detached editor panel, and no per-row independent layout.

## Why this is a real grid (FS)

- One shared tabular layout (`QTableView`) owns all rows.
- Column boundaries are identical for every row (name / value / preview / picker).
- Value editors are permanently hosted in their cells via `setIndexWidget`.
- Color rows place text editor, swatch, and picker in **separate grid columns**, not nested `QHBoxLayout` inside a single cell.
- Non-color rows use empty placeholder widgets in columns 2–3 so column alignment is preserved.

## Row schema

32 rows, order and labels ported from `win-go/internal/ui/row_schema.go`. Validation ported from `win-go/internal/ui/validation.go` and `config_mapper.go`.

## Rejected patterns

- `QFormLayout` / per-row `QHBoxLayout` composites (grid-like)
- Default `QTableView` item delegates (editors not permanent)
- Detached bottom/side editor panels
- `SysListView32` overlay approach from Go
