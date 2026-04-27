# Shitwiper Settings Grid — Implementation-Ready Architecture (Rev 3)

---

## 1. Executive Decision

**Chosen approach: `walk` property-sheet with permanent in-cell native controls.**

The settings UI is a property sheet, not a data grid. It has two columns, no sorting,
no filtering, no reordering, and a small fixed row count. Every attempt to use a
data-grid stack (web-based or otherwise) in this project has failed for the same reason:
the problem does not match the tool. A data-grid stack solves problems this UI does not
have, while adding complexity that has repeatedly broken the problems it does have.

The walk property-sheet approach matches the constraint set exactly:

- Single executable, no runtime dependencies beyond what the app already ships.
- Native Win32 controls with direct in-cell editing — no overlay, no detached panel.
- Mixed editor types are handled by placing the appropriate native control in each cell
  at construction time.
- Full build pipeline compatibility — no new toolchain, no Node, no WebView2.

**Alternatives rejected:**

Wails + AG Grid: Requires WebView2 runtime, a JS/Node build pipeline, and a separate
window lifecycle. Introduces UX inconsistency between the native app shell and the
web-rendered settings dialog. Every prior failure involving embedded web approaches is
a direct consequence of this architectural class. Rejected unconditionally under the
single-executable constraint.

Fyne: Not native Win32. Cross-platform abstraction layer introduces rendering
differences and requires significant custom work for heterogeneous in-cell editors.
Rejected.

Qt bindings: Go binding ecosystem maintenance risk. Toolchain complexity. Rejected.

wxWidgets Go wrappers: Wrapper maturity risk. Rejected.

**Future constraint to document now:** walk's `GridLayout` does not handle dynamic
widget visibility gracefully. If a future requirement adds conditional rows (show field
X only when field Y has a specific value), the correct approach is to rebuild the grid
composite entirely, not to hide/show individual widgets. Hiding a widget leaves a hole
in the layout. Do not attempt to work around this — rebuild the grid.

**Cross-field validation limitation:** all validation in this architecture is
field-level. Cross-field constraints (for example, background color must differ from
foreground color) are not supported in the current model because each field validates
independently. If this becomes a requirement, add a separate `validateObject` function
called after `validateAll` in the Apply/OK flow, which can inspect the full `[]RowState`
and return additional `ValidationError` values. This is a defined extension point, not
an oversight.

---

## 2. Architecture Diagram

```
settings_treemap_windows.go
│
├── SettingsDialog (walk.Dialog)
│   ├── ScrollView
│   │   └── Composite [GridLayout, 2 columns]
│   │       ├── [Label | Editor]  row 0
│   │       ├── [Label | Editor]  row 1
│   │       └── ...               row N
│   ├── ErrorLabel (walk.Label, red, hidden when no error)
│   └── Composite [HBox — action buttons]
│       └── [Reset | Apply | OK | Cancel]
│
├── Row Schema Layer (row_schema.go)
│   ├── []RowSchema   — static, defined once at package level, never mutated
│   └── []RowState    — created fresh per dialog open from []RowSchema + config
│
├── Editor Factory (editor_factory.go)
│   ├── buildTextEditor(state *RowState, onChanged func()) Widget
│   ├── buildNumericEditor(state *RowState, onChanged func()) Widget
│   ├── buildDropdownEditor(state *RowState, onChanged func()) Widget
│   └── buildColorEditor(state *RowState, onChanged func()) Widget
│
├── Validation Layer (validation.go)
│   ├── validateField(state *RowState) error
│   └── validateAll(states []RowState) []ValidationError
│
├── State Manager (settings_state.go)
│   ├── states     []RowState
│   ├── committed  []string   — snapshot of LastGood at last commit
│   ├── validating bool       — re-entrancy guard
│   ├── saving     bool       — save-in-progress guard
│   ├── isDirty()  bool       — compares states[i].LastGood to committed[i]
│   ├── commit()              — copies LastGood → committed
│   └── revert()              — copies committed → LastGood → PendingValue
│
└── Config Mapper (config_mapper.go)
    ├── treemapToStates(cfg config.Treemap, schemas []RowSchema) []RowState
    └── statesToTreemap(states []RowState) (config.Treemap, error)
```

---

## 3. Core Components and Responsibilities

### `settings_treemap_windows.go` — UI Composition Layer

Owns the `walk.Dialog`, `ScrollView`, grid `Composite`, `ErrorLabel`, action buttons,
and the `SettingsState` instance. Provides an `onValueChanged()` method that editor
factory callbacks invoke when a value is successfully committed. This method clears the
error label and is where any future post-commit side effects live. Has no business
logic — delegates to layers below.

The `ErrorLabel` is a `walk.Label` at the bottom of the dialog, above the action
buttons. It is empty (invisible) when there are no errors. On validation failure it
shows the relevant error message in red. It disappears on the next successful commit
via `onValueChanged()`. It is never a modal dialog and requires no extra click.

### `row_schema.go` — Schema and State Layer

Two strictly separated types:

```go
type EditorKind int

const (
    KindText EditorKind = iota
    KindNumeric
    KindDropdown
    KindColor
)

// RowSchema is static. Defined once at package level. Never mutated after init.
type RowSchema struct {
    Key         string
    Label       string
    Kind        EditorKind
    Options     []string        // KindDropdown only
    Min, Max    float64         // KindNumeric only
    Decimals    int             // KindNumeric only
    Regex       *regexp.Regexp  // compiled at init; nil if unused
    Placeholder string
}

// RowState is per-dialog-instance. Created fresh on every dialog open.
type RowState struct {
    Schema       *RowSchema
    PendingValue string
    LastGood     string
}
```

`RowSchema` is immutable after package initialization. `RowState` is created fresh by
`treemapToStates` on every dialog open. Reset reinitializes `RowState` values from
config defaults — it never touches the schema.

The master schema slice:

```go
var treemapSchemas = []RowSchema{
    {Key: "maxTiles",     Label: "Max tiles",        Kind: KindNumeric,  Min: 1,   Max: 9999, Decimals: 0},
    {Key: "padding",      Label: "Padding (px)",     Kind: KindNumeric,  Min: 0,   Max: 100,  Decimals: 0},
    {Key: "fontSize",     Label: "Font size (px)",   Kind: KindNumeric,  Min: 4,   Max: 200,  Decimals: 0},
    {Key: "lineHeight",   Label: "Line height",      Kind: KindNumeric,  Min: 0.5, Max: 4.0,  Decimals: 2},
    {Key: "tileFontName", Label: "Tile font",        Kind: KindDropdown, Options: loadFontOptions()},
    {Key: "bgColor",      Label: "Background color", Kind: KindColor},
    {Key: "fgColor",      Label: "Foreground color", Kind: KindColor},
    {Key: "extensions",   Label: "Extensions",       Kind: KindText,
     Regex: regexp.MustCompile(`^(\.[a-z0-9]+)(,\.[a-z0-9]+)*$`)},
}
```

`loadFontOptions()` is called once at package init. See §Font Enumeration.

### `editor_factory.go` — Editor Factory

One function per editor kind. Each function takes `*RowState` and an `onChanged func()`
callback, and returns a `declarative.Widget`. The `onChanged` callback is provided by
the dialog and is called after every successful commit. Editors call it to notify the
dialog that a value changed — the dialog then clears the error label and updates dirty
state implicitly via `isDirty()`.

Editor factory functions are pure construction functions. They own no state.

### `validation.go` — Validation Layer

```go
type ValidationError struct {
    Key     string
    Label   string
    Message string // plain English; names the field; states what is acceptable
}

func validateField(state *RowState) error
func validateAll(states []RowState) []ValidationError
```

`validateField` checks `state.PendingValue` against all constraints in `state.Schema`.
`validateAll` calls `validateField` on every state and returns all errors. It never
stops at the first error — always collects all.

Error message contract: every `ValidationError.Message` must be user-readable plain
English, name the field, and state what is acceptable.

Correct: `"Font size must be between 4 and 200."`
Wrong: `"parse error"`, `"invalid"`, `"out of range"`

### `settings_state.go` — State Manager

```go
type SettingsState struct {
    states    []RowState
    committed []string  // snapshot of LastGood values at last successful commit
    validating bool     // re-entrancy guard: true while a validation is executing
    saving     bool     // save guard: true while config write is in progress
}
```

**`isDirty()`** compares each `state.LastGood` to the corresponding `committed` entry.
Returns `true` if any differ:

```go
func (s *SettingsState) isDirty() bool {
    for i, state := range s.states {
        if state.LastGood != s.committed[i] {
            return true
        }
    }
    return false
}
```

No explicit dirty setter exists. Dirty state is always derived from the comparison.
After the color picker updates `state.LastGood`, the next `isDirty()` call returns
`true` automatically — no `markDirty()` call is needed or exists.

**`commit()`** copies each `state.LastGood` into `committed`:

```go
func (s *SettingsState) commit() {
    for i, state := range s.states {
        s.committed[i] = state.LastGood
    }
}
```

**`revert()`** copies each `committed[i]` back into `state.LastGood` and
`state.PendingValue`, then updates editor controls:

```go
func (s *SettingsState) revert() {
    for i := range s.states {
        s.states[i].LastGood = s.committed[i]
        s.states[i].PendingValue = s.committed[i]
    }
    // Caller is responsible for updating editor controls via SetText
}
```

**`saving` guard:** the `saving` boolean is set at the start of a config write and
cleared on completion. OK/Apply buttons are also disabled during save. Both protections
are required: the button disable prevents the common case; the guard protects against
the edge case where an event fires despite the button being disabled (possible in some
walk versions under rapid input). The guard is belt-and-suspenders, not redundant.

### `config_mapper.go` — Config Mapper

```go
func treemapToStates(cfg config.Treemap, schemas []RowSchema) []RowState
func statesToTreemap(states []RowState) (config.Treemap, error)
```

`treemapToStates` detects a zero-value `cfg` by checking whether key fields are at
their zero values. If so, it substitutes `config.DefaultTreemap()` and logs at INFO
level: `"settings: config is empty, using defaults"`. It must never return states with
empty `PendingValue` or `LastGood`.

`statesToTreemap` failing after `validateAll` has passed is a programming error. Handle
it as follows:

```go
cfg, err := statesToTreemap(states)
if err != nil {
    if debugBuild {
        panic(fmt.Sprintf("statesToTreemap failed after validateAll passed: %v", err))
    }
    log.Printf("ERROR: statesToTreemap: %v", err)
    showErrorLabel("Internal error — settings not saved.")
    return
}
```

---

## 4. Data Model and Mapping Contracts

### config.Treemap → []RowState

1. If `cfg` is zero-value, substitute `config.DefaultTreemap()`. Log at INFO.
2. For each `RowSchema`, locate the corresponding field by `Key`.
3. Format the Go value to string: `strconv.Itoa`, `strconv.FormatFloat`,
   `fmt.Sprintf("#%02X%02X%02X", r, g, b)`, etc.
4. Set both `PendingValue` and `LastGood` to the formatted string.
5. Initialize `committed` in `SettingsState` as a copy of all `LastGood` values.
   This snapshot represents the on-disk state at dialog open. `isDirty()` will be
   `false` until the user makes a change.

### []RowState → config.Treemap

1. For each `RowState`, parse `PendingValue` to the target Go type.
2. Collect all parse errors before returning — never write a partial config.
3. On any error: log at ERROR, return error to caller. Caller shows error label, aborts
   save.

---

## 5. Editing and Validation State Machine

```
[Loaded] ─────────────────────────────────────────────────────────┐
    │                                                              │
    │  user activates control                                      │
    ▼                                                              │
[Editing]                                                          │
    │                                                              │
    ├── Enter / Tab / focus-loss ──► [Validating]                  │
    │                                    │                         │
    │                              valid?│                         │
    │                            ┌───────┴────────┐               │
    │                         yes│                │no              │
    │                            ▼                ▼               │
    │                      [Committed]       [Rejected]            │
    │                            │                │               │
    │                  PendingValue=input    ErrorLabel shows msg  │
    │                  LastGood=input        SetText(LastGood)     │
    │                  onValueChanged()      PendingValue=LastGood  │
    │                  ErrorLabel cleared    Focus stays on field  │
    │                  isDirty() = auto      EnsureVisible(control)│
    │                            │                │               │
    │                            └────────────────┘               │
    │                                     │                       │
    │                              back to [Loaded]               │
    │                                                             │
    └── Esc ─────────────────────────────────────────────────────►┘
          SetText(LastGood), PendingValue = LastGood
          ErrorLabel cleared
```

**Keyboard contract:**

- `Enter`: commit current field, move focus to next editor.
- `Tab`: commit current field, move focus to next editor (walk default).
- `Shift+Tab`: commit current field, move focus to previous editor.
- `Esc`: revert current field to `LastGood`. Focus stays. Error label cleared.
- Arrow keys in `NumberEdit`: increment/decrement, commit immediately.
- Arrow keys in `ComboBox`: navigate options, commit on selection.

**Focus-loss behavior:** treat identically to `Enter`.

---

## 6. Color Editor — Button Click and Focus Interaction

When the user clicks the `...` button while the `LineEdit` has focus, focus transfers to
the button. In some walk configurations, `OnEditingFinished` does not fire during this
transition. The `LineEdit` value is therefore not committed before the picker opens.

The fix is explicit: the button's `OnClicked` handler must commit the `LineEdit` value
before doing anything else.

```go
PushButton{
    AssignTo: &btn,
    Text:     "...",
    MaxSize:  Size{Width: 30},
    OnClicked: func() {
        // Explicitly commit LineEdit before opening picker.
        if !commitColorLineEdit(state, le, showErrorLabel) {
            return // invalid value — picker does not open
        }
        color := showColorDialog(btn.Handle(), state.LastGood)
        if color != "" {
            state.PendingValue = color
            state.LastGood = color
            le.SetText(color)
            updateSwatch(swatch, color)
            onChanged() // notify dialog; clears error label; isDirty() now true
        }
    },
},
```

`commitColorLineEdit` validates and commits the current `LineEdit` text. Returns `true`
on success, `false` on failure (in which case it has already reverted and shown the
error label):

```go
func commitColorLineEdit(
    state *RowState,
    le *walk.LineEdit,
    showError func(string),
) bool {
    text := le.Text()
    if !hexColorRE.MatchString(text) {
        le.SetText(state.LastGood)
        state.PendingValue = state.LastGood
        showError("Color must be in #RRGGBB format (example: #3A7FCB).")
        return false
    }
    state.PendingValue = text
    state.LastGood = text
    return true
}
```

Note that `onChanged()` is not called on failure — the error label is set directly via
`showError`. `onChanged()` is only called on success.

---

## 7. Color Editing Architecture

### In-Cell Layout

Each color row's right cell contains a horizontal `Composite` with three children:

1. `LineEdit` — displays and accepts `#RRGGBB` hex string.
2. `PushButton` ("...") — commits `LineEdit` then opens `ChooseColor`.
3. Color swatch — a borderless `Composite`, `MinSize: Size{Width: 20, Height: 20}`.
   walk translates logical pixels to physical pixels automatically on HiDPI displays.
   Background color is updated explicitly in every code path that changes the color.

The swatch is not a separate column. It lives inside the value cell.

### Hex color regex

```go
var hexColorRE = regexp.MustCompile(`^#[0-9A-Fa-f]{6}$`)
```

Compiled once at package level.

### Text edit path

On `OnEditingFinished`:

1. If `validating` guard is set, return immediately.
2. Set `validating = true`, defer clear.
3. Test `le.Text()` against `hexColorRE`.
4. Valid: `state.PendingValue = le.Text()`. `state.LastGood = le.Text()`. Update swatch.
   Call `onChanged()`.
5. Invalid: call `showError(...)`. `le.SetText(state.LastGood)`.
   `state.PendingValue = state.LastGood`.

### Picker path

Handled by button `OnClicked` as specified in §6.

### `showColorDialog`

```go
func showColorDialog(hwnd win.HWND, current string) string {
    var r, g, b uint8
    fmt.Sscanf(current, "#%02x%02x%02x", &r, &g, &b)

    // COLORREF is 0x00BBGGRR — byte order is intentional, not a typo
    initColor := win.COLORREF(uint32(r) | uint32(g)<<8 | uint32(b)<<16)
    var customColors [16]win.COLORREF

    cc := win.CHOOSECOLOR{
        LStructSize:  uint32(unsafe.Sizeof(win.CHOOSECOLOR{})),
        HwndOwner:    hwnd,
        RgbResult:    initColor,
        LpCustColors: &customColors[0],
        Flags:        win.CC_FULLOPEN | win.CC_RGBINIT,
    }

    if win.ChooseColor(&cc) {
        r := byte(cc.RgbResult & 0xFF)
        g := byte((cc.RgbResult >> 8) & 0xFF)
        b := byte((cc.RgbResult >> 16) & 0xFF)
        return fmt.Sprintf("#%02X%02X%02X", r, g, b)
    }
    return ""
}
```

If `win.ChooseColor` is absent from the `lxn/win` version in use, declare via syscall:

```go
var chooseColorProc = syscall.NewLazyDLL("comdlg32.dll").NewProc("ChooseColorW")
```

---

## 8. Font Enumeration

Do not use `EnumFontFamiliesEx`. Use the Windows registry — synchronous, thread-safe,
no callback required:

```go
func loadFontOptions() []string {
    const key = `SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`
    k, err := registry.OpenKey(registry.LOCAL_MACHINE, key, registry.READ)
    if err != nil {
        log.Printf("WARN: could not open font registry key: %v", err)
        return fallbackFonts()
    }
    defer k.Close()

    names, err := k.ReadValueNames(-1)
    if err != nil {
        log.Printf("WARN: could not read font names: %v", err)
        return fallbackFonts()
    }

    // Registry values are like "Arial Bold (TrueType)" — strip style and type.
    // Multiple entries may resolve to the same family name; deduplicate.
    seen := make(map[string]bool)
    var fonts []string
    for _, n := range names {
        if idx := strings.Index(n, " ("); idx != -1 {
            n = n[:idx]
        }
        // Strip trailing style words (Bold, Italic, etc.)
        // Simple approach: take only the first word group before a known style term.
        // This is heuristic — good enough for a font picker.
        if !seen[n] {
            seen[n] = true
            fonts = append(fonts, n)
        }
    }
    sort.Strings(fonts)
    return fonts
}

func fallbackFonts() []string {
    return []string{
        "Arial", "Calibri", "Consolas", "Courier New",
        "Georgia", "Segoe UI", "Tahoma", "Times New Roman", "Verdana",
    }
}
```

Log at WARN on any registry failure. The fallback list is used silently from the user's
perspective. The font combo is editable (`Editable: true`) so the user can type any
font name not present in the list.

---

## 9. Action Flows

### Load

1. Call `config.LoadTreemap()`. On error or zero-value, substitute
   `config.DefaultTreemap()`. Log at INFO.
2. Call `treemapToStates(cfg, treemapSchemas)`.
3. Initialize `SettingsState` with states. Set `committed` to a copy of all
   `LastGood` values. `isDirty() == false`.
4. Call `buildRows()` to construct editor widgets, passing `onValueChanged` callback.
5. Open dialog.

### Edit (valid)

1. User edits a control.
2. On commit trigger: `validateField(state)`. Set `validating` guard.
3. Passes: `state.LastGood = state.PendingValue`. Call `onChanged()`.
   `onChanged()` clears error label. `isDirty()` now returns `true` automatically.

### Edit (invalid)

1. User edits a control.
2. On commit trigger: `validateField(state)`. Set `validating` guard.
3. Fails: call `showError(message)`. `control.SetText(state.LastGood)`.
   `state.PendingValue = state.LastGood`. Return focus to offending control.
   Call `scrollView.EnsureVisible(control)`.

### Apply

1. Set `saving = true`. Disable Apply and OK buttons. Set wait cursor.
2. Call `validateAll(states)`.
3. Errors: re-enable buttons. Clear wait cursor. Set `saving = false`.
   Show first error in error label. Focus first invalid field. `EnsureVisible` it.
   Do not write config.
4. All valid: call `statesToTreemap(states)`. On error: re-enable, clear cursor,
   set `saving = false`, show error label. Do not write.
5. Call `config.SaveTreemap(newCfg)`. On error: re-enable, clear cursor,
   set `saving = false`. Error label: "Settings could not be saved. Check that the
   config file is writable." Log OS error at ERROR. Do not close.
6. `state.commit()`. Re-enable buttons. Clear wait cursor. Set `saving = false`.
   Error label cleared. Dialog stays open.

### OK

1. Execute Apply flow.
2. Apply succeeded: `dlg.Accept()`.
3. Apply failed: remain open.

### Cancel

1. If `isDirty()`: show confirmation — "You have unsaved changes. Discard them?"
   This confirmation is mandatory.
2. Confirmed (or not dirty): `dlg.Cancel()`. No config write.
3. Declined: return to dialog.

### Reset to Defaults

1. Call `config.DefaultTreemap()`.
2. Call `treemapToStates(defaults, treemapSchemas)` to produce fresh `[]RowState`.
3. Replace `settingsState.states` with new states.
4. Set `validating = true` to suppress re-entrant validation during programmatic
   updates.
5. For each editor, call `control.SetText(state.PendingValue)`.
6. For each color row, call `updateSwatch(colorSwatches[i], state.PendingValue)`
   explicitly. `SetText` on the `LineEdit` does NOT update the swatch automatically.
7. Clear `validating`.
8. `isDirty()` will now return `true` (defaults differ from committed values).
   Config is not written — Apply/OK still required.

---

## 10. Concurrency and State Safety

walk is single-threaded. All UI events fire on the main goroutine. Never start
goroutines from event handlers. Never call walk APIs from any goroutine other than
main.

**Re-entrancy guard** (`validating bool`): prevents `SetText` inside a handler from
triggering `OnEditingFinished` / `OnTextChanged` again. Apply at the top of every
`OnEditingFinished`, `OnValueChanged`, and in the Reset flow before programmatic
`SetText` calls:

```go
if s.validating {
    return
}
s.validating = true
defer func() { s.validating = false }()
```

**Save guard** (`saving bool`): set at start of config write, cleared on completion.
OK/Apply buttons are also disabled during save. Both protections are required: the
button disable prevents the common case; the guard protects against the edge case
where an event fires despite button disabling in some walk versions under rapid input.

Config save is synchronous. The dialog is modal — blocking the main goroutine during
save is acceptable. Save operations must disable OK/Apply, set the wait cursor, and
restore both on completion regardless of outcome.

---

## 11. Error Handling Strategy

**User-facing errors** — shown in `ErrorLabel` (non-modal, inline, no extra click):

- Field validation failure: immediate, on the offending field. Clears on next success.
- Object validation failure on Apply/OK: first error message; focus and scroll to first
  invalid field.
- Save failure: "Settings could not be saved. Check that the config file is writable."

**Internal errors** — logged, not shown unless they affect correctness:

- `statesToTreemap` failure after `validateAll` passed: programming error. Panic in
  debug build. Log at ERROR + generic error label in release.
- Font registry read failure: log at WARN, use fallback list silently.
- Config load failure at open: log at WARN, use defaults silently.

**Log levels:**

- INFO: config substituted with defaults (normal first-run case).
- WARN: non-fatal recoverable failures (font registry, etc.).
- ERROR: unexpected failures that affect correctness.

**Recovery:** on any error the dialog stays open. Last committed config on disk is
unchanged. The user can retry or Cancel.

---

## 12. Manifest and DPI

Required for native visual styles on Windows 10/11. Without it, controls render in
Windows 2000 style.

`app.manifest`:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
        type="win32"
        name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0"
        processorArchitecture="*"
        publicKeyToken="6595b64144ccf1df"
        language="*"/>
    </dependentAssembly>
  </dependency>
  <asmv3:application xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
    <asmv3:windowsSettings>
      <!-- true/PM = per-monitor DPI awareness. Requires Windows 8.1+.
           On Windows 7 this entry is ignored and system DPI scaling applies
           (blurry but functional). No special handling required. -->
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/PM</dpiAware>
    </asmv3:windowsSettings>
  </asmv3:application>
</assembly>
```

`main.rc`:

```rc
1 24 "app.manifest"
```

`main.go`:

```go
//go:generate rsrc -manifest app.manifest -o rsrc.syso
```

Run `go generate` once after any manifest change. Verify `rsrc.syso` is present before
running `build.ps1`.

---

## 13. Risk Register and Mitigations

**Re-entrancy loop from `SetText` inside `OnEditingFinished`.**
Mitigation: `validating` guard at top of every handler.

**`NumberEdit` fires `OnValueChanged` during programmatic Reset.**
Mitigation: same `validating` guard suppresses write-back during Reset.

**`LineEdit` value not committed when user clicks `...` button directly.**
Mitigation: button handler explicitly calls `commitColorLineEdit` before opening picker.
If `commitColorLineEdit` returns `false`, picker does not open.

**Color swatch not updated after programmatic Reset.**
Mitigation: Reset flow explicitly calls `updateSwatch` for all color rows after
`SetText`. Documented in §9 Reset flow.

**Font registry read fails or returns empty.**
Mitigation: `fallbackFonts()` always provides a usable list. Failure logged at WARN,
silent to user.

**Font family duplicates in registry** (e.g., "Arial Bold" and "Arial Italic" both
strip to "Arial").
Mitigation: `seen` map deduplicates before appending to font list.

**`win.ChooseColor` absent from `lxn/win` version in use.**
Mitigation: declare syscall manually via `comdlg32.dll`. Add build-time call to confirm
linkage.

**Color swatch not repainting after explicit update.**
Mitigation: `updateSwatch` is called explicitly in every code path that changes a color
value. Do not rely on implicit invalidation.

**Dialog opens with stale or zero values if config load fails.**
Mitigation: `treemapToStates` detects zero-value config and substitutes defaults.
Asserted non-empty before return.

**Double-save on rapid OK/Apply clicks.**
Mitigation: `saving` guard + button disable. Both required.

**Scroll position hides invalid field after validation failure.**
Mitigation: `scrollView.EnsureVisible(control)` called in every rejection path.

**`statesToTreemap` called without prior `validateAll` passing.**
Mitigation: only call `statesToTreemap` in Apply/OK after `validateAll` returns no
errors. The debug panic catches violations during development.

---

## 14. Migration Plan with Milestones

### Milestone 0 — Compile checkpoint (no behavior change)

Verify `app.manifest` and `rsrc.syso` present. Add `//go:build windows` to all
walk-using files. Create stub files:

```
win-go/internal/ui/row_schema.go
win-go/internal/ui/editor_factory.go
win-go/internal/ui/validation.go
win-go/internal/ui/settings_state.go
win-go/internal/ui/config_mapper.go
```

Run `go build ./...`. Must pass. No UI changes.

### Milestone 1 — Schema and config mapper

Implement `RowSchema`, `RowState`, `EditorKind`, `treemapSchemas`.
Implement `treemapToStates` and `statesToTreemap`.

Unit tests required:
- Round-trip: `treemapToStates(cfg)` → `statesToTreemap(states)` reproduces `cfg`.
- Zero-value config: `treemapToStates` substitutes defaults; no empty strings returned.
- Parse error: invalid string in numeric field returns error from `statesToTreemap`.
- `committed` initialized equal to all `LastGood` values; `isDirty()` returns `false`.

Run tests. Must pass. No UI changes.

### Milestone 2 — Validation layer

Implement `validateField` and `validateAll`. Unit tests for every constraint type:
min, max, regex, option membership, empty value, color hex format, `isDirty()` before
and after a state change.

Run tests. Must pass. No UI changes.

### Milestone 3 — Editor factory (harness dialog)

Implement all four `build*Editor` functions. Build a minimal harness dialog (separate
file, `//go:build harness` tag):

```
go run -tags harness ./cmd/harness
```

Verify manually:
- Each editor type is directly editable.
- Invalid input rejected with correct revert.
- Color: text path and picker path work. Swatch updates on both paths.
- Color: click `...` with invalid `LineEdit` value — rejects, picker does not open.
- `onChanged()` callback fires on every successful commit.

Do not merge until all four types confirmed working.

### Milestone 4 — State manager and action buttons

Implement `SettingsState` with `isDirty`, `commit`, `revert`, `validating`, `saving`.
Wire Apply, OK, Cancel, Reset in harness. Verify all action flows in §9 manually.
Confirm `validating` guard prevents re-entrancy. Confirm `EnsureVisible` scrolls to
invalid field. Confirm Cancel confirmation appears when `isDirty()` is true. Confirm
Reset updates color swatches explicitly.

### Milestone 5 — Full settings dialog

Replace `settings_treemap_windows.go`. Wire to `run_windows.go` `onSettings()`. Remove
all old `TableView`-based code.

Run `go build ./...` and `go vet ./...`. Fix all issues. Run `win-go/build.ps1`.
Verify built executable opens settings dialog. Run `scripts/build-installer.bat`.
Verify installer produces working install.

### Milestone 6 — Full verification

Execute complete verification plan (§15). All items must pass before merge.

**Rollback:** each milestone is a separate commit. Milestones 0–2 have no user-visible
effect and carry zero rollback risk.

---

## 15. Verification Plan

### Manual runtime checks

- Dialog opens with correct current values from config.
- Every cell is directly editable by clicking. No activation click required.
- No detached, floating, or bottom-panel editor appears at any point.
- `NumberEdit` clamps to min/max on blur.
- `NumberEdit` increments/decrements with arrow keys.
- Dropdown shows all options; selection updates value.
- Font combo accepts custom font names not in list.
- Color `LineEdit` accepts valid `#RRGGBB`; swatch updates; error label cleared.
- Color `LineEdit` rejects invalid input; error label appears; field reverts.
- Click `...` with invalid `LineEdit` value: rejected, picker does not open.
- Click `...` with valid `LineEdit` value: picker opens pre-seeded with current color.
- Picker confirmation: `LineEdit` and swatch updated.
- Picker Cancel: value unchanged.
- Reset: all fields populated with defaults. Swatches updated. Config not written.
- Apply: config written, dialog stays open, dirty flag clears.
- OK: config written, dialog closes.
- Cancel with dirty state: confirmation shown.
- Cancel confirmed: dialog closes, config unchanged.
- Re-open after Apply: shows applied values.
- Validation failure: error label shown, first invalid field focused and scrolled into
  view.

### Negative validation checks

- Letters in numeric field: rejected, error label, reverts.
- Number below `Min`: rejected, error label, reverts.
- Number above `Max`: rejected, error label, reverts.
- `#GGHHII` in color field: rejected, error label, reverts.
- Extensions field violating regex: rejected, error label, reverts.
- OK with invalid field: error label, focus on invalid field, config not written, dialog
  stays open.

### Keyboard behavior checks

- Tab: commits, moves to next editor.
- Shift+Tab: commits, moves to previous editor.
- Enter: commits, moves to next editor.
- Esc: reverts current field, error label cleared, focus stays.

### Build and install checks

- `go build ./...` passes with zero errors.
- `go vet ./...` passes with zero warnings.
- `win-go/build.ps1` produces single executable.
- Executable runs without any runtime not present in a clean Windows install.
- `scripts/build-installer.bat` produces working installer.
- Installed app passes all manual runtime checks on a clean machine with no Go
  toolchain.

---

## 16. Strict Definition of Done

All items are pass/fail. Feature is not done until every item passes.

- [ ] Dialog opens with correct current config values.
- [ ] Every cell is directly editable. No activation click required.
- [ ] No detached, floating, or bottom-panel editor exists.
- [ ] All four editor types function correctly.
- [ ] Invalid input rejected with error label in every editor type.
- [ ] Last valid value restored after rejection in every editor type.
- [ ] Color `...` click with invalid `LineEdit`: rejected, picker does not open.
- [ ] Color swatch updates on every successful color change (text and picker paths).
- [ ] Color swatch updated explicitly during Reset.
- [ ] Apply writes config without closing dialog.
- [ ] OK writes config and closes dialog.
- [ ] Cancel with unsaved changes: mandatory confirmation shown.
- [ ] Cancel confirmed: closes without writing config.
- [ ] Reset populates defaults without writing config.
- [ ] Full-object validation on Apply/OK; all errors before any write.
- [ ] First invalid field focused and scrolled into view on validation failure.
- [ ] OK/Apply disabled during save; wait cursor shown; both restored on completion.
- [ ] `isDirty()` returns `false` at open; `true` after any committed change.
- [ ] `isDirty()` returns `false` after Apply.
- [ ] Config file format and path unchanged from pre-migration.
- [ ] `go build ./...` passes.
- [ ] `go vet ./...` passes.
- [ ] `win-go/build.ps1` produces single executable, no external runtime deps.
- [ ] `scripts/build-installer.bat` produces working installer.
- [ ] Installed app passes all manual runtime checks on clean Windows machine.

---

## 17. Implementation Checklist (Developer-Ready)

- [ ] Verify `app.manifest` present. Run `go generate`. Verify `rsrc.syso` present.
- [ ] Add `//go:build windows` to all walk-using files.
- [ ] Create stub files. Run `go build ./...`. Must pass.
- [ ] Implement `RowSchema`, `RowState`, `EditorKind`, `treemapSchemas`.
- [ ] Implement `loadFontOptions()` with registry read, deduplication, `fallbackFonts()`.
- [ ] Implement `treemapToStates` and `statesToTreemap`. Write and pass unit tests.
- [ ] Implement `validateField` and `validateAll`. Write and pass unit tests.
- [ ] Implement `buildTextEditor` with `onChanged` callback. Test in harness.
- [ ] Implement `buildNumericEditor` with `onChanged` callback. Test in harness.
- [ ] Implement `buildDropdownEditor` with `onChanged` callback. Test in harness.
- [ ] Implement `buildColorEditor` with swatch, `commitColorLineEdit`, picker,
      `onChanged` callback. Test in harness. Specifically test: `...` click with
      invalid `LineEdit` value must reject and not open picker.
- [ ] Implement `SettingsState`: `isDirty`, `commit`, `revert`, `validating`, `saving`.
- [ ] Verify `isDirty()` uses `committed` comparison, not an explicit dirty flag.
- [ ] Verify no `markDirty()` method exists anywhere.
- [ ] Wire Apply, OK, Cancel, Reset in harness. Verify all action flows per §9.
- [ ] Verify `EnsureVisible` scrolls to invalid field on rejection.
- [ ] Verify Cancel confirmation appears when `isDirty()` is true.
- [ ] Verify Reset explicitly calls `updateSwatch` for all color rows.
- [ ] Replace `settings_treemap_windows.go`. Remove all old `TableView` code.
- [ ] Wire to `run_windows.go` `onSettings()`.
- [ ] Run `go build ./...` and `go vet ./...`. Fix all issues.
- [ ] Run `win-go/build.ps1`. Verify output.
- [ ] Execute full verification plan. Fix all failures.
- [ ] Run `scripts/build-installer.bat`. Verify on clean machine.
- [ ] All Definition of Done items checked.


## Pre-Implementation Q&A — Shitwiper Settings Grid

---

**Q: You call this a "true grid," but the architecture is a 2-column permanent-control
property sheet. Do you explicitly accept this as compliant?**

Yes, explicitly. The term "true grid" in the requirements meant "direct in-cell editing
with no detached editor panel" — not "spreadsheet-style data grid with sorting and
filtering." The 2-column permanent-control property sheet satisfies every acceptance
criterion in the requirements. The architecture document was written specifically to
reject data-grid stacks for this UI. This is not a compromise — it is the correct
implementation of the requirement.

---

**Q: In `treemapSchemas`, sample keys (`maxTiles`, `padding`, `fontSize`) don't match
current real keys (`treemap.*` set). Should I treat the sample as illustrative only
and map to full existing treemap keyset?**

Yes, treat every key in the architecture document as illustrative. The schema slice in
`row_schema.go` must be built from the full existing `treemap.*` keyset as it actually
exists in `internal/config`. Do not invent keys, do not drop existing keys, do not
rename anything. The architecture specifies the structure and typing rules — the actual
keys come from the real config model.

---

**Q: Should I preserve the current `config.Treemap` field names and serialization 1:1
with zero format changes in saved config?**

Yes, unconditionally. The config file format and field names must not change. Existing
config files on user machines must load correctly after the UI rewrite. The settings UI
is being replaced — the config contract is not. Any field rename or format change is
out of scope and must not happen.

---

**Q: For ratio fields (e.g. clump threshold), do you want `%` input accepted (`75%`)
or numeric only (`0.75` / `75`)?**

Numeric only. Strip `%` input support entirely. The underlying value is a number — the
editor should accept a number. If the field stores a value between 0 and 1, the editor
accepts `0.75`. If it stores 0–100, it accepts `75`. Pick the range that matches the
existing config field and use `NumberEdit` with appropriate `Min`, `Max`, and
`Decimals`. The `%` suffix is cosmetic noise that adds a parsing edge case for no user
benefit.

---

**Q: For `tileFontName`, confirm editable combo is mandatory even when installed-font
list exists.**

Confirmed, mandatory. The editable combo is required precisely because the installed-font
list is built from the developer's machine at build time via registry read. A font
present on the user's machine may not appear in the list. The editable combo ensures
the user can always type a font name directly. Do not make it a strict dropdown under
any circumstances.

---

**Q: For color rows, do you want swatch inline in value cell only (no extra column),
as stated?**

Confirmed. The swatch lives inside the value cell as the third child of the color
`Composite` — after the `LineEdit` and before the `...` button, or after the button,
whichever reads more naturally. There is no separate swatch column. The grid has exactly
two columns throughout.

---

**Q: For error UX, should we use only inline `ErrorLabel` and avoid modal `MsgBox` for
validation errors entirely?**

Yes, avoid `MsgBox` for all field-level and object-level validation errors. Use the
inline `ErrorLabel` exclusively. `MsgBox` is permitted only for the Cancel discard
confirmation and for catastrophic I/O errors where the dialog cannot recover and must
inform the user before closing. For all validation feedback, the `ErrorLabel` is the
only mechanism.

---

**Q: Cancel flow: should discard confirmation always show when dirty, or only when the
user changed at least one committed field?**

Same thing, but confirming the rule: show the confirmation whenever `isDirty()` returns
`true`. `isDirty()` is defined as any `state.LastGood` differing from its corresponding
`committed` value. If the user opened the dialog, changed a field, then reverted it
manually back to its original value, `isDirty()` will return `false` and no confirmation
is shown. If they changed it and left it changed, confirmation is shown. This is the
correct behavior — the comparison is against committed state, not against "did the user
touch anything."

---

**Q: On Apply failure due to file write error, should focus stay on the current field
rather than moving to the first invalid field as it does for validation errors?**

Yes. Distinguish the two failure modes explicitly:

- Validation failure: focus moves to the first invalid field and scrolls to it.
- I/O failure (file write error): focus stays where it is. The error label shows the
  I/O error message. The user has not made an input error — moving focus would be
  disorienting and incorrect.

Implement this distinction in the Apply handler by checking whether the failure came
from `validateAll` or from `config.SaveTreemap`.

---

**Q: Should the Milestone 3 harness (`-tags harness`) be committed to the repo or
kept local-only during development?**

Commit it to the repo under `cmd/harness/` with the `//go:build harness` tag. Reasons:
it serves as a permanent regression test surface for editor behavior; it documents the
expected editor contract for future developers; and keeping it local-only means it
disappears when the branch is merged. The build tag ensures it is never compiled into
the production binary. It does not ship.

---

**Q: Should `go vet ./...` be enforced in CI or only as a manual pre-merge check?**

Enforce it in CI. A manual pre-merge check that is not automated will be skipped under
pressure. `go vet` catches real bugs (misused format strings, unreachable code, lock
copying) and has zero false-positive rate in practice for this kind of codebase. Add it
to the build pipeline now. If CI does not currently exist for this project, the
pre-merge gate is: `go build ./...` and `go vet ./...` must both pass locally before
any merge. Document this in the repo README.

---

**Q: "No goroutines in handlers" — do you also want an explicit ban on any async save
path in this dialog?**

Yes, explicit ban. The config save is synchronous and blocking. There is no async save
path in this dialog. The correct user-visible behavior during save is: buttons disabled,
wait cursor, brief block, buttons re-enabled. The settings dialog is modal — the user
cannot interact with anything else while it is open. A synchronous save is correct and
safe. An async save would require cancellation logic, error surfacing across goroutine
boundaries, and UI state management that adds complexity with no benefit. Do not
implement it.

---

**Q: For font registry read from `HKLM`, do we need an `HKCU` fallback for restricted
environments?**

Yes, add it. On machines where the user lacks read access to `HKLM` (unusual but
possible in locked-down enterprise environments), the registry call will fail and the
fallback list will be used. Before falling back, attempt `HKCU` first:

```
HKCU\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts
```

User-installed fonts live here and the user always has read access to their own hive.
The read order is: HKLM first, merge with HKCU, deduplicate, sort. If both fail, use
`fallbackFonts()`. Log at WARN if either read fails.

---

**Q: Do you want the `validateObject` extension point added now as a stub, or only when
the first cross-field rule is requested?**

Add it now as a stub. The cost is ten lines. The benefit is that the extension point
exists in the right place in the call chain — between `validateAll` and
`statesToTreemap` in the Apply/OK flow — so a future developer does not have to
restructure the handler to add cross-field validation. The stub:

```go
// validateObject checks cross-field constraints across the full row state.
// Currently no cross-field rules exist. Add rules here when required.
// Called after validateAll passes, before statesToTreemap.
func validateObject(states []RowState) []ValidationError {
    return nil
}
```

Wire it into the Apply/OK handler immediately after `validateAll`. When it returns
`nil`, the flow proceeds exactly as before.

---

**Q: Final acceptance authority: is runtime behavior confirmed in your manual check the
only source of truth, over docs, build, and test?**

Yes. The Definition of Done is a checklist, but the final gate is a human running the
dialog on a real Windows machine and confirming it behaves correctly. Docs can be
correct while the binary is wrong. Tests can pass while the installed executable fails.
Build can succeed while the UI is broken. Runtime behavior on a clean machine is the
only thing that matters to the user, and it is therefore the only thing that closes the
feature. Do not consider the work done until that confirmation is given.