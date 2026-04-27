# True Grid Code Bundle (Current)

## Explicit Request For Help (Priority)

We need a concrete fix for this blocker, not general advice.

Please provide:

1. A precise root-cause diagnosis for why value cells are still not editable in this `walk.TableView` setup.
2. A step-by-step implementation plan that is guaranteed to produce real in-cell editing on Windows.
3. Exact code-level guidance for the event flow and Win32 messages/hooks we must use.
4. Clear fallback path if `walk.TableView` subitem editing is fundamentally unreliable in this runtime.
5. Acceptance criteria we can test immediately to confirm the bug is solved.

Constraints:

- Single executable only.
- Native Windows UI only (no webview, no separate frontend process).
- Real grid behavior: user edits values directly in cells.
- No detached/bottom editor pattern.

This bundle captures the current implementation state for the Windows settings "true grid" and the unresolved bug.

## Issue Summary

- **Target behavior:** Value cells in the settings grid must be directly editable in-place.
- **Current behavior:** `TableView` renders rows and values, but value editing does not activate reliably on user interaction.
- **Impact:** The UI looks like a real grid, but user cannot effectively edit values.

## Reproduction

1. Build and run Windows app.
2. Open `Tools -> Settings`.
3. Click a row and click inside `Value` column.
4. Try typing.
5. Observe: values remain not editable on target machine/runtime.

## Expected vs Actual

- **Expected:** Clicking a value cell opens an in-cell editor immediately; typing updates value.
- **Actual:** Grid selection changes, but editing does not become usable.

## Help Request

Please help identify why in-cell editing is not activating in this `walk.TableView` integration.

Specific questions:

1. Is `LVM_GETSUBITEMRECT` + overlay `LineEdit` the wrong approach with this `walk` version?
2. Should editing be hosted on the underlying native list-view parent window instead of dialog?
3. Is there a required event/hook pattern (WndProc subclass, custom editor host) for reliable subitem editing?
4. Should we abandon overlay editor and implement native label edit + column swap for "Value" instead?
5. Are there known `walk`/Win32 constraints that prevent stable per-subitem editing in `TableView`?

## Current Related Sources

- `win-go/internal/ui/settings_treemap_windows.go`
- `win-go/internal/ui/run_windows.go`
- `win-go/internal/ui/config_mapper.go`
- `win-go/internal/ui/validation.go`
- `win-go/internal/ui/row_schema.go`
- `win-go/internal/ui/settings_state.go`
- `win-go/internal/config/file.go`
- `win-go/internal/config/treemap.go`

## Key Code: Settings Table Grid

```go
// win-go/internal/ui/settings_treemap_windows.go (excerpt)
type treemapTrueGridModel struct {
	walk.TableModelBase
	states []RowState
}

func (m *treemapTrueGridModel) Value(row, col int) interface{} {
	if row < 0 || row >= len(m.states) {
		return ""
	}
	switch col {
	case 0:
		if m.states[row].Schema == nil {
			return ""
		}
		return m.states[row].Schema.Label
	case 1:
		return m.states[row].PendingValue
	default:
		return ""
	}
}

func showTreemapSettingsTableDialog(owner walk.Form, current config.Treemap, onApply func(config.Treemap)) {
	var dlg *walk.Dialog
	var tv *walk.TableView
	// ... snip ...
	decl := Dialog{
		AssignTo: &dlg,
		Title:    "Settings",
		Children: []Widget{
			TableView{
				AssignTo:        &tv,
				StretchFactor:   1,
				ColumnsOrderable: false,
				Columns: []TableViewColumn{
					{Title: "Parameter", Width: 360},
					{Title: "Value", Width: 620},
				},
			},
			// ... buttons ...
		},
	}
	// ... snip ...
}
```

```go
// win-go/internal/ui/settings_treemap_windows.go (editor trigger excerpt)
tv.MouseDown().Attach(func(x, y int, button walk.MouseButton) {
	if button != walk.LeftButton {
		return
	}
	info := win.LVHITTESTINFO{Pt: win.POINT{X: int32(x), Y: int32(y)}}
	row := int(win.SendMessage(tv.Handle(), win.LVM_SUBITEMHITTEST, 0, uintptr(unsafe.Pointer(&info))))
	if row < 0 || row >= len(s.states) {
		return
	}
	if info.IItem != int32(row) || info.ISubItem != 1 {
		return
	}
	tv.SetCurrentIndex(row)
	beginEdit(row)
})
```

```go
// win-go/internal/ui/settings_treemap_windows.go (editor placement excerpt)
beginEdit := func(row int) {
	var rc win.RECT
	rc.Top = 1
	rc.Left = win.LVIR_BOUNDS
	if win.SendMessage(tv.Handle(), win.LVM_GETSUBITEMRECT, uintptr(row), uintptr(unsafe.Pointer(&rc))) == 0 {
		return
	}
	tl := win.POINT{X: rc.Left, Y: rc.Top}
	br := win.POINT{X: rc.Right, Y: rc.Bottom}
	win.ClientToScreen(tv.Handle(), &tl)
	win.ClientToScreen(tv.Handle(), &br)
	win.ScreenToClient(dlg.Handle(), &tl)
	win.ScreenToClient(dlg.Handle(), &br)
	editor.SetBoundsPixels(walk.Rectangle{
		X: int(tl.X) + 1, Y: int(tl.Y) + 1,
		Width: int(br.X-tl.X) - 2, Height: int(br.Y-tl.Y) - 2,
	})
	_ = editor.SetText(s.states[row].PendingValue)
	editor.SetVisible(true)
	_ = editor.SetFocus()
}
```

## Key Code: Settings Entry Point

```go
// win-go/internal/ui/run_windows.go (excerpt)
func (a *app) onSettings() {
	showTreemapSettingsDialog(a.mw, a.treemapCfg, func(next config.Treemap) {
		a.treemapCfg = next
		a.chartDirty = true
		a.invalidateLabelCache()
		if a.chart != nil {
			a.chart.Invalidate()
		}
	})
}
```

## Key Code: Mapping + Validation

```go
// win-go/internal/ui/config_mapper.go (excerpt)
func treemapToStates(cfg config.Treemap, schemas []RowSchema) []RowState {
	if cfg.MaxTiles == 0 {
		cfg = config.DefaultTreemap()
	}
	out := make([]RowState, 0, len(schemas))
	for i := range schemas {
		v, err := treemapValueByKey(&cfg, schemas[i].Key)
		if err != nil {
			v = ""
		}
		out = append(out, RowState{
			Schema: &schemas[i], PendingValue: v, LastGood: v,
		})
	}
	return out
}
```

```go
// win-go/internal/ui/validation.go (excerpt)
func validateField(state *RowState) error {
	val := strings.TrimSpace(state.PendingValue)
	switch state.Schema.Kind {
	case KindNumeric:
		if val == "" {
			return fmt.Errorf("%s must not be empty; enter a numeric value", state.Schema.Label)
		}
		f, err := strconv.ParseFloat(val, 64)
		if err != nil {
			return fmt.Errorf("%s must be a number", state.Schema.Label)
		}
		if f < state.Schema.Min || f > state.Schema.Max {
			return fmt.Errorf("%s must be between %g and %g", state.Schema.Label, state.Schema.Min, state.Schema.Max)
		}
	}
	return nil
}
```

## Build Used for This State

- EXE marker: `2026-04-27_06-21_004A.date`
- Main artifact: `bin/win/current/EraseAndRewrite.exe`

# True Grid Code Bundle

This file contains the requested source bundle with filename headers and file contents.

---

## FILE: `win-go/internal/ui/settings_treemap_windows.go`

```go
//go:build windows

package ui

import (
	"fmt"
	"image/color"
	"log"
	"strconv"
	"strings"
	"unsafe"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"eraserewrite/win-go/internal/config"
)

type rowControlBinding struct {
	applyValue   func(string)
	focusWidget  walk.Widget
	colorSwatch  *walk.Composite
	colorLine    *walk.LineEdit
	scrollParent *walk.ScrollView
}

func (b *rowControlBinding) setValue(v string) {
	if b == nil || b.applyValue == nil {
		return
	}
	b.applyValue(v)
}

func (b *rowControlBinding) focus() {
	if b == nil || b.focusWidget == nil {
		return
	}
	_ = b.focusWidget.SetFocus()
	if b.scrollParent != nil {
		b.scrollParent.Invalidate()
	}
}

func updateSwatch(swatch *walk.Composite, value string) {
	if swatch == nil {
		return
	}
	c, err := parseHexColor(value)
	if err != nil {
		return
	}
	br, err := walk.NewSolidColorBrush(walk.RGB(c.R, c.G, c.B))
	if err != nil {
		return
	}
	swatch.SetBackground(br)
}

func showTreemapSettingsDialog(owner walk.Form, current config.Treemap, onApply func(config.Treemap)) {
	var dlg *walk.Dialog
	var sv *walk.ScrollView
	var gridHost *walk.Composite
	var errLabel *walk.Label
	var applyBtn *walk.PushButton
	var okBtn *walk.PushButton
	var cancelBtn *walk.PushButton

	s := newSettingsState(treemapToStates(current, treemapSchemas))
	bindings := make([]rowControlBinding, len(s.states))

	showError := func(msg string) {
		if errLabel != nil {
			errLabel.SetText(msg)
			errLabel.SetVisible(strings.TrimSpace(msg) != "")
		}
	}
	clearError := func() {
		showError("")
	}

	decl := Dialog{
		AssignTo: &dlg,
		Title:    "Settings",
		MinSize:  Size{Width: 760, Height: 620},
		Size:     Size{Width: 860, Height: 700},
		Layout:   VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			ScrollView{
				AssignTo:      &sv,
				StretchFactor: 1,
				Layout:        VScroll{},
				Children: []Widget{
					Composite{
						AssignTo: &gridHost,
						Layout:   Grid{Columns: 2, Margins: Margins{0, 0, 0, 0}, Spacing: 8},
					},
				},
			},
			Label{
				AssignTo: &errLabel,
				Text:     "",
				Visible:  false,
			},
			Composite{
				Layout: HBox{MarginsZero: true, Spacing: 8},
				Children: []Widget{
					PushButton{
						Text: "Reset Treemap Defaults",
						OnClicked: func() {
							next := treemapToStates(config.DefaultTreemap(), treemapSchemas)
							for i := range s.states {
								s.states[i].PendingValue = next[i].PendingValue
								s.states[i].LastGood = next[i].LastGood
								bindings[i].setValue(next[i].PendingValue)
							}
							clearError()
						},
					},
					HSpacer{},
					PushButton{AssignTo: &cancelBtn, Text: "Cancel"},
					PushButton{AssignTo: &applyBtn, Text: "Apply"},
					PushButton{AssignTo: &okBtn, Text: "OK"},
				},
			},
		},
	}
	if err := decl.Create(owner); err != nil {
		walk.MsgBox(owner, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	onChanged := func() { clearError() }

	for i := range s.states {
		lbl, _ := walk.NewTextLabel(gridHost)
		_ = lbl.SetText(s.states[i].Schema.Label)
		lbl.SetMinMaxSize(walk.Size{Width: 270}, walk.Size{})

		bindings[i] = buildEditorControl(gridHost, &s.states[i], s, showError, onChanged)
		bindings[i].scrollParent = sv
	}

	applyFlow := func(closeOnSuccess bool) bool {
		if s.saving {
			return false
		}
		s.saving = true
		if applyBtn != nil {
			applyBtn.SetEnabled(false)
		}
		if okBtn != nil {
			okBtn.SetEnabled(false)
		}
		defer func() {
			s.saving = false
			if applyBtn != nil {
				applyBtn.SetEnabled(true)
			}
			if okBtn != nil {
				okBtn.SetEnabled(true)
			}
		}()

		if errs := validateAll(s.states); len(errs) > 0 {
			showError(errs[0].Message)
			if idx := indexByKey(s.states, errs[0].Key); idx >= 0 {
				bindings[idx].focus()
			}
			return false
		}
		if errs := validateObject(s.states); len(errs) > 0 {
			showError(errs[0].Message)
			if idx := indexByKey(s.states, errs[0].Key); idx >= 0 {
				bindings[idx].focus()
			}
			return false
		}
		nextCfg, err := statesToTreemap(s.states)
		if err != nil {
			log.Printf("ERROR: statesToTreemap: %v", err)
			showError("Internal error — settings not saved.")
			return false
		}
		p, err := config.ConfigPath()
		if err != nil {
			showError("Settings could not be saved. Check that the config file is writable.")
			return false
		}
		if err := config.SaveTreemap(p, nextCfg); err != nil {
			log.Printf("ERROR: SaveTreemap: %v", err)
			showError("Settings could not be saved. Check that the config file is writable.")
			return false
		}
		s.commit()
		clearError()
		if onApply != nil {
			onApply(nextCfg)
		}
		if closeOnSuccess {
			dlg.Accept()
		}
		return true
	}

	if applyBtn != nil {
		applyBtn.Clicked().Attach(func() { _ = applyFlow(false) })
	}
	if okBtn != nil {
		okBtn.Clicked().Attach(func() { _ = applyFlow(true) })
	}
	if cancelBtn != nil {
		cancelBtn.Clicked().Attach(func() {
			if s.isDirty() {
				if walk.MsgBox(dlg, "Settings", "You have unsaved changes. Discard them?", walk.MsgBoxYesNo|walk.MsgBoxIconQuestion) != walk.DlgCmdYes {
					return
				}
			}
			dlg.Cancel()
		})
	}
	_ = dlg.Run()
}

func indexByKey(states []RowState, key string) int {
	for i := range states {
		if states[i].Schema != nil && states[i].Schema.Key == key {
			return i
		}
	}
	return -1
}

func buildEditorControl(parent walk.Container, state *RowState, ss *SettingsState, showError func(string), onChanged func()) rowControlBinding {
	b := rowControlBinding{}
	switch state.Schema.Kind {
	case KindNumeric:
		ne, _ := walk.NewNumberEdit(parent)
		ne.SetMinValue(state.Schema.Min)
		ne.SetMaxValue(state.Schema.Max)
		ne.SetDecimals(state.Schema.Decimals)
		if f, err := strconv.ParseFloat(state.LastGood, 64); err == nil {
			_ = ne.SetValue(f)
		}
		commit := func() {
			if ss.validating {
				return
			}
			ss.validating = true
			defer func() { ss.validating = false }()
			f := ne.Value()
			text := strconv.FormatFloat(f, 'f', state.Schema.Decimals, 64)
			if state.Schema.Decimals == 0 {
				text = strconv.Itoa(int(f))
			}
			state.PendingValue = text
			if err := validateField(state); err != nil {
				showError(err.Error())
				if old, err2 := strconv.ParseFloat(state.LastGood, 64); err2 == nil {
					_ = ne.SetValue(old)
				}
				state.PendingValue = state.LastGood
				return
			}
			state.LastGood = state.PendingValue
			onChanged()
		}
		ne.ValueChanged().Attach(commit)
		b.applyValue = func(v string) {
			if f, err := strconv.ParseFloat(strings.TrimSpace(v), 64); err == nil {
				_ = ne.SetValue(f)
			}
		}
		b.focusWidget = ne
	case KindDropdown:
		cb, _ := walk.NewComboBox(parent)
		cb.SetEditable(true)
		_ = cb.SetModel(state.Schema.Options)
		_ = cb.SetText(state.LastGood)
		commit := func() {
			if ss.validating {
				return
			}
			ss.validating = true
			defer func() { ss.validating = false }()
			state.PendingValue = strings.TrimSpace(cb.Text())
			if err := validateField(state); err != nil {
				showError(err.Error())
				_ = cb.SetText(state.LastGood)
				state.PendingValue = state.LastGood
				return
			}
			state.LastGood = state.PendingValue
			onChanged()
		}
		cb.CurrentIndexChanged().Attach(commit)
		cb.EditingFinished().Attach(commit)
		b.applyValue = func(v string) { _ = cb.SetText(v) }
		b.focusWidget = cb
	case KindColor:
		row, _ := walk.NewComposite(parent)
		_ = row.SetLayout(walk.NewHBoxLayout())
		le, _ := walk.NewLineEdit(row)
		_ = le.SetText(state.LastGood)
		btn, _ := walk.NewPushButton(row)
		_ = btn.SetText("...")
		btn.SetMinMaxSize(walk.Size{Width: 30}, walk.Size{Width: 30})
		swatch, _ := walk.NewComposite(row)
		swatch.SetMinMaxSize(walk.Size{Width: 20, Height: 20}, walk.Size{Width: 20, Height: 20})
		updateSwatch(swatch, state.LastGood)

		commit := func() bool {
			if ss.validating {
				return false
			}
			ss.validating = true
			defer func() { ss.validating = false }()
			state.PendingValue = strings.TrimSpace(le.Text())
			if err := validateField(state); err != nil {
				showError(err.Error())
				_ = le.SetText(state.LastGood)
				state.PendingValue = state.LastGood
				return false
			}
			state.LastGood = state.PendingValue
			updateSwatch(swatch, state.LastGood)
			onChanged()
			return true
		}
		le.EditingFinished().Attach(func() { _ = commit() })
		btn.Clicked().Attach(func() {
			if !commit() {
				return
			}
			chosen := showColorDialog(btn.Handle(), state.LastGood)
			if chosen == "" {
				return
			}
			state.PendingValue = chosen
			state.LastGood = chosen
			_ = le.SetText(chosen)
			updateSwatch(swatch, chosen)
			onChanged()
		})
		b.applyValue = func(v string) {
			_ = le.SetText(v)
			updateSwatch(swatch, v)
		}
		b.focusWidget = le
		b.colorLine = le
		b.colorSwatch = swatch
	default:
		le, _ := walk.NewLineEdit(parent)
		_ = le.SetText(state.LastGood)
		commit := func() {
			if ss.validating {
				return
			}
			ss.validating = true
			defer func() { ss.validating = false }()
			state.PendingValue = strings.TrimSpace(le.Text())
			if err := validateField(state); err != nil {
				showError(err.Error())
				_ = le.SetText(state.LastGood)
				state.PendingValue = state.LastGood
				return
			}
			state.LastGood = state.PendingValue
			onChanged()
		}
		le.EditingFinished().Attach(commit)
		le.KeyDown().Attach(func(key walk.Key) {
			if key == walk.KeyEscape {
				_ = le.SetText(state.LastGood)
				state.PendingValue = state.LastGood
				onChanged()
			}
		})
		b.applyValue = func(v string) { _ = le.SetText(v) }
		b.focusWidget = le
	}
	return b
}

func showColorDialog(hwnd win.HWND, current string) string {
	var r, g, b uint8
	_, _ = fmt.Sscanf(strings.TrimSpace(current), "#%02x%02x%02x", &r, &g, &b)
	initColor := win.COLORREF(uint32(r) | uint32(g)<<8 | uint32(b)<<16)
	var customColors [16]win.COLORREF
	cc := win.CHOOSECOLOR{
		LStructSize:  uint32(unsafe.Sizeof(win.CHOOSECOLOR{})),
		HwndOwner:    hwnd,
		RgbResult:    initColor,
		LpCustColors: &customColors,
		Flags:        win.CC_FULLOPEN | win.CC_RGBINIT,
	}
	if !win.ChooseColor(&cc) {
		return ""
	}
	return fmt.Sprintf("#%02X%02X%02X", byte(cc.RgbResult&0xFF), byte((cc.RgbResult>>8)&0xFF), byte((cc.RgbResult>>16)&0xFF))
}

func parsePercentOrRatio(s string) (float64, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return 0, fmt.Errorf("empty value")
	}
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0, err
	}
	return v, nil
}

func parseHexColor(s string) (color.RGBA, error) {
	s = strings.TrimSpace(strings.TrimPrefix(strings.TrimSpace(s), "#"))
	if len(s) != 6 {
		return color.RGBA{}, fmt.Errorf("must be 6 hex digits")
	}
	var r, g, b uint8
	if _, err := fmt.Sscanf(s, "%02x%02x%02x", &r, &g, &b); err != nil {
		return color.RGBA{}, fmt.Errorf("invalid hex color")
	}
	return color.RGBA{R: r, G: g, B: b, A: 255}, nil
}

func formatRGBHex(c color.RGBA) string {
	return fmt.Sprintf("#%02X%02X%02X", c.R, c.G, c.B)
}
```

---

## FILE: `win-go/internal/ui/settings_treemap_mirror_windows.go`

```go
//go:build windows

package ui

import (
	"fmt"
	"image/color"
	"strconv"
	"strings"

	"github.com/lxn/walk"

	"eraserewrite/win-go/internal/config"
)

// treemapGridRowLabels is the fixed row order for the treemap settings grid (32 rows).
var treemapGridRowLabels = []string{
	"treemap.maxTiles",
	"treemap.clumpThreshold",
	"treemap.minTileWidth (pt)",
	"treemap.minTileHeight (pt)",
	"treemap.tilePaddingLeft (pt)",
	"treemap.tilePaddingTop (pt)",
	"treemap.tilePaddingRight (pt)",
	"treemap.tilePaddingBottom (pt)",
	"treemap.tileFontName",
	"treemap.headingMaxFontSize (pt)",
	"treemap.headingMinFontSize (pt)",
	"treemap.headingLineHeight",
	"treemap.detailsFontSizeRatio",
	"treemap.detailsLineHeight",
	"treemap.aboveDetailsHeightRatio",
	"treemap.labelPlaceholder",
	"treemap.labelDummy",
	"treemap.nativeFolderBgColor",
	"treemap.nativeFolderTextColor",
	"treemap.packedFolderBgColor",
	"treemap.packedFolderTextColor",
	"treemap.nativeFileBgColor",
	"treemap.nativeFileTextColor",
	"treemap.packedFileBgColor",
	"treemap.packedFileTextColor",
	"treemap.nativeClumpBgColor",
	"treemap.nativeClumpTextColor",
	"treemap.packedClumpBgColor",
	"treemap.packedClumpTextColor",
	"treemap.win.exeFiles",
	"treemap.linux.exeFiles",
	"treemap.macos.exeFiles",
}

const treemapGridColorRow0 = 17
const treemapGridColorRowCount = 12

type treemapGridModel struct {
	walk.TableModelBase
	edited    *config.Treemap
	fontModel []string
}

func (m *treemapGridModel) RowCount() int { return len(treemapGridRowLabels) }

func (m *treemapGridModel) Value(row, col int) interface{} {
	if m.edited == nil || row < 0 || row >= m.RowCount() || col < 0 || col > 3 {
		return ""
	}
	switch col {
	case 0:
		return treemapGridRowLabels[row]
	case 1:
		return treemapValueColString(m.edited, row)
	case 2:
		return ""
	case 3:
		return ""
	default:
		return ""
	}
}

func treemapValueColString(t *config.Treemap, row int) string {
	if t == nil {
		return ""
	}
	switch row {
	case 0:
		return strconv.Itoa(t.MaxTiles)
	case 1:
		return strconv.FormatFloat(t.ClumpThreshold, 'f', -1, 64)
	case 2:
		return strconv.Itoa(t.MinTileWidthPt)
	case 3:
		return strconv.Itoa(t.MinTileHeightPt)
	case 4:
		return strconv.Itoa(t.TilePaddingLeftPt)
	case 5:
		return strconv.Itoa(t.TilePaddingTopPt)
	case 6:
		return strconv.Itoa(t.TilePaddingRightPt)
	case 7:
		return strconv.Itoa(t.TilePaddingBottomPt)
	case 8:
		return strings.TrimSpace(t.TileFontName)
	case 9:
		return strconv.Itoa(t.HeadingMaxFontSizePt)
	case 10:
		return strconv.Itoa(t.HeadingMinFontSizePt)
	case 11:
		return strconv.FormatFloat(t.HeadingLineHeight, 'f', -1, 64)
	case 12:
		return strconv.FormatFloat(t.DetailsFontSizeRatio, 'f', -1, 64)
	case 13:
		return strconv.FormatFloat(t.DetailsLineHeight, 'f', -1, 64)
	case 14:
		return strconv.FormatFloat(t.AboveDetailsRatio, 'f', -1, 64)
	case 15:
		return t.LabelPlaceholder
	case 16:
		return t.LabelDummy
	case 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28:
		return strings.TrimSpace(colorHexAtRow(t, row))
	case 29:
		return strings.TrimSpace(t.WinExeFiles)
	case 30:
		return strings.TrimSpace(t.LinuxExeFiles)
	case 31:
		return strings.TrimSpace(t.MacOSExeFiles)
	default:
		return ""
	}
}

func colorHexAtRow(t *config.Treemap, row int) string {
	switch row {
	case 17:
		return formatRGBHex(t.NativeFolderBg)
	case 18:
		return formatRGBHex(t.NativeFolderText)
	case 19:
		return formatRGBHex(t.PackedFolderBg)
	case 20:
		return formatRGBHex(t.PackedFolderText)
	case 21:
		return formatRGBHex(t.NativeFileBg)
	case 22:
		return formatRGBHex(t.NativeFileText)
	case 23:
		return formatRGBHex(t.PackedFileBg)
	case 24:
		return formatRGBHex(t.PackedFileText)
	case 25:
		return formatRGBHex(t.NativeClumpBg)
	case 26:
		return formatRGBHex(t.NativeClumpText)
	case 27:
		return formatRGBHex(t.PackedClumpBg)
	case 28:
		return formatRGBHex(t.PackedClumpText)
	default:
		return ""
	}
}

func setTreemapColorByRow(t *config.Treemap, row int, c color.RGBA) {
	switch row {
	case 17:
		t.NativeFolderBg = c
	case 18:
		t.NativeFolderText = c
	case 19:
		t.PackedFolderBg = c
	case 20:
		t.PackedFolderText = c
	case 21:
		t.NativeFileBg = c
	case 22:
		t.NativeFileText = c
	case 23:
		t.PackedFileBg = c
	case 24:
		t.PackedFileText = c
	case 25:
		t.NativeClumpBg = c
	case 26:
		t.NativeClumpText = c
	case 27:
		t.PackedClumpBg = c
	case 28:
		t.PackedClumpText = c
	}
}

func treemapSetValueColFromString(t *config.Treemap, row int, s string) error {
	s = strings.TrimSpace(s)
	switch row {
	case 0:
		v, err := strconv.Atoi(s)
		if err != nil || v < 1 {
			return fmt.Errorf("must be an integer >= 1")
		}
		t.MaxTiles = v
		return nil
	case 1:
		cl, err := parsePercentOrRatio(s)
		if err != nil || cl <= 0 || cl > 1 {
			return fmt.Errorf("must be a number > 0 and <= 1")
		}
		t.ClumpThreshold = cl
		return nil
	case 2, 3:
		v, err := strconv.Atoi(s)
		if err != nil || v < 1 {
			return fmt.Errorf("must be an integer >= 1")
		}
		if row == 2 {
			t.MinTileWidthPt = v
		} else {
			t.MinTileHeightPt = v
		}
		return nil
	case 4, 5, 6, 7:
		v, err := strconv.Atoi(s)
		if err != nil || v < 0 {
			return fmt.Errorf("must be an integer >= 0")
		}
		switch row {
		case 4:
			t.TilePaddingLeftPt = v
		case 5:
			t.TilePaddingTopPt = v
		case 6:
			t.TilePaddingRightPt = v
		case 7:
			t.TilePaddingBottomPt = v
		}
		return nil
	case 8:
		if s == "" {
			return fmt.Errorf("font name must not be empty")
		}
		t.TileFontName = s
		return nil
	case 9, 10:
		v, err := strconv.Atoi(s)
		if err != nil || v < 1 {
			return fmt.Errorf("must be an integer >= 1")
		}
		if row == 9 {
			t.HeadingMaxFontSizePt = v
		} else {
			t.HeadingMinFontSizePt = v
		}
		return nil
	case 11, 12, 13, 14:
		v, err := strconv.ParseFloat(s, 64)
		if err != nil || v <= 0 {
			return fmt.Errorf("must be > 0")
		}
		switch row {
		case 11:
			t.HeadingLineHeight = v
		case 12:
			t.DetailsFontSizeRatio = v
		case 13:
			t.DetailsLineHeight = v
		case 14:
			t.AboveDetailsRatio = v
		}
		return nil
	case 15, 16:
		if s == "" {
			return fmt.Errorf("must not be empty")
		}
		if row == 15 {
			t.LabelPlaceholder = s
		} else {
			t.LabelDummy = s
		}
		return nil
	case 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28:
		c, err := parseHexColor(s)
		if err != nil {
			return err
		}
		setTreemapColorByRow(t, row, c)
		return nil
	case 29:
		t.WinExeFiles = s
		return nil
	case 30:
		t.LinuxExeFiles = s
		return nil
	case 31:
		t.MacOSExeFiles = s
		return nil
	default:
		return fmt.Errorf("unknown row")
	}
}
```

---

## FILE: `win-go/internal/ui/row_schema.go`

```go
//go:build windows

package ui

import (
	"log"
	"regexp"
	"sort"
	"strings"

	"golang.org/x/sys/windows/registry"
)

type EditorKind int

const (
	KindText EditorKind = iota
	KindNumeric
	KindDropdown
	KindColor
)

type RowSchema struct {
	Key         string
	Label       string
	Kind        EditorKind
	Options     []string
	Min         float64
	Max         float64
	Decimals    int
	Regex       *regexp.Regexp
	Placeholder string
}

type RowState struct {
	Schema       *RowSchema
	PendingValue string
	LastGood     string
}

var exeFilesRE = regexp.MustCompile(`^[A-Za-z0-9., _-]*$`)

var treemapSchemas = []RowSchema{
	{Key: "treemap.maxTiles", Label: "treemap.maxTiles", Kind: KindNumeric, Min: 1, Max: 9999, Decimals: 0},
	{Key: "treemap.clumpThreshold", Label: "treemap.clumpThreshold", Kind: KindNumeric, Min: 0.000001, Max: 1, Decimals: 6},
	{Key: "treemap.minTileWidth", Label: "treemap.minTileWidth (pt)", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.minTileHeight", Label: "treemap.minTileHeight (pt)", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.tilePaddingLeft", Label: "treemap.tilePaddingLeft (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingTop", Label: "treemap.tilePaddingTop (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingRight", Label: "treemap.tilePaddingRight (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingBottom", Label: "treemap.tilePaddingBottom (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tileFontName", Label: "treemap.tileFontName", Kind: KindDropdown, Options: loadFontOptions()},
	{Key: "treemap.headingMaxFontSize", Label: "treemap.headingMaxFontSize (pt)", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingMinFontSize", Label: "treemap.headingMinFontSize (pt)", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingLineHeight", Label: "treemap.headingLineHeight", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsFontSizeRatio", Label: "treemap.detailsFontSizeRatio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsLineHeight", Label: "treemap.detailsLineHeight", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.aboveDetailsHeightRatio", Label: "treemap.aboveDetailsHeightRatio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.labelPlaceholder", Label: "treemap.labelPlaceholder", Kind: KindText},
	{Key: "treemap.labelDummy", Label: "treemap.labelDummy", Kind: KindText},
	{Key: "treemap.nativeFolderBgColor", Label: "treemap.nativeFolderBgColor", Kind: KindColor},
	{Key: "treemap.nativeFolderTextColor", Label: "treemap.nativeFolderTextColor", Kind: KindColor},
	{Key: "treemap.packedFolderBgColor", Label: "treemap.packedFolderBgColor", Kind: KindColor},
	{Key: "treemap.packedFolderTextColor", Label: "treemap.packedFolderTextColor", Kind: KindColor},
	{Key: "treemap.nativeFileBgColor", Label: "treemap.nativeFileBgColor", Kind: KindColor},
	{Key: "treemap.nativeFileTextColor", Label: "treemap.nativeFileTextColor", Kind: KindColor},
	{Key: "treemap.packedFileBgColor", Label: "treemap.packedFileBgColor", Kind: KindColor},
	{Key: "treemap.packedFileTextColor", Label: "treemap.packedFileTextColor", Kind: KindColor},
	{Key: "treemap.nativeClumpBgColor", Label: "treemap.nativeClumpBgColor", Kind: KindColor},
	{Key: "treemap.nativeClumpTextColor", Label: "treemap.nativeClumpTextColor", Kind: KindColor},
	{Key: "treemap.packedClumpBgColor", Label: "treemap.packedClumpBgColor", Kind: KindColor},
	{Key: "treemap.packedClumpTextColor", Label: "treemap.packedClumpTextColor", Kind: KindColor},
	{Key: "treemap.win.exeFiles", Label: "treemap.win.exeFiles", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.linux.exeFiles", Label: "treemap.linux.exeFiles", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.macos.exeFiles", Label: "treemap.macos.exeFiles", Kind: KindText, Regex: exeFilesRE},
}

func loadFontOptions() []string {
	seen := map[string]struct{}{}
	load := func(root registry.Key, path string) error {
		k, err := registry.OpenKey(root, path, registry.READ)
		if err != nil {
			return err
		}
		defer k.Close()
		names, err := k.ReadValueNames(-1)
		if err != nil {
			return err
		}
		for _, n := range names {
			n = strings.TrimSpace(n)
			if idx := strings.Index(n, " ("); idx >= 0 {
				n = strings.TrimSpace(n[:idx])
			}
			if n != "" {
				seen[n] = struct{}{}
			}
		}
		return nil
	}
	const fontsKey = `SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`
	if err := load(registry.LOCAL_MACHINE, fontsKey); err != nil {
		log.Printf("WARN: could not read HKLM fonts: %v", err)
	}
	if err := load(registry.CURRENT_USER, fontsKey); err != nil {
		log.Printf("WARN: could not read HKCU fonts: %v", err)
	}
	if len(seen) == 0 {
		return fallbackFonts()
	}
	out := make([]string, 0, len(seen))
	for n := range seen {
		out = append(out, n)
	}
	sort.Strings(out)
	return out
}

func fallbackFonts() []string {
	return []string{
		"Arial", "Calibri", "Consolas", "Courier New",
		"Georgia", "Segoe UI", "Tahoma", "Times New Roman", "Verdana",
	}
}
```

---

## FILE: `win-go/internal/ui/settings_state.go`

```go
//go:build windows

package ui

type SettingsState struct {
	states     []RowState
	committed  []string
	validating bool
	saving     bool
}

func newSettingsState(states []RowState) *SettingsState {
	s := &SettingsState{
		states:    states,
		committed: make([]string, len(states)),
	}
	s.commit()
	return s
}

func (s *SettingsState) isDirty() bool {
	for i, state := range s.states {
		if state.LastGood != s.committed[i] {
			return true
		}
	}
	return false
}

func (s *SettingsState) commit() {
	for i, state := range s.states {
		s.committed[i] = state.LastGood
	}
}

func (s *SettingsState) revert() {
	for i := range s.states {
		s.states[i].LastGood = s.committed[i]
		s.states[i].PendingValue = s.committed[i]
	}
}
```

---

## FILE: `win-go/internal/ui/config_mapper.go`

```go
//go:build windows

package ui

import (
	"fmt"
	"log"

	"eraserewrite/win-go/internal/config"
)

func treemapToStates(cfg config.Treemap, schemas []RowSchema) []RowState {
	if cfg == (config.Treemap{}) {
		log.Printf("INFO: settings: config is empty, using defaults")
		cfg = config.DefaultTreemap()
	}
	out := make([]RowState, 0, len(schemas))
	for i := range schemas {
		v := treemapValueColString(&cfg, i)
		out = append(out, RowState{
			Schema:       &schemas[i],
			PendingValue: v,
			LastGood:     v,
		})
	}
	return out
}

func statesToTreemap(states []RowState) (config.Treemap, error) {
	cfg := config.DefaultTreemap()
	for i := range states {
		if err := treemapSetValueColFromString(&cfg, i, states[i].PendingValue); err != nil {
			return config.Treemap{}, fmt.Errorf("%s: %w", states[i].Schema.Label, err)
		}
	}
	return cfg, nil
}
```

---

## FILE: `win-go/internal/ui/validation.go`

```go
//go:build windows

package ui

import (
	"fmt"
	"strconv"
	"strings"
)

type ValidationError struct {
	Key     string
	Label   string
	Message string
}

func validateField(state *RowState) error {
	if state == nil || state.Schema == nil {
		return fmt.Errorf("internal error")
	}
	val := strings.TrimSpace(state.PendingValue)
	switch state.Schema.Kind {
	case KindText:
		if state.Schema.Regex != nil && val != "" && !state.Schema.Regex.MatchString(val) {
			return fmt.Errorf("%s contains invalid characters", state.Schema.Label)
		}
		if (state.Schema.Key == "treemap.labelPlaceholder" || state.Schema.Key == "treemap.labelDummy") && val == "" {
			return fmt.Errorf("%s must not be empty", state.Schema.Label)
		}
		return nil
	case KindNumeric:
		f, err := strconv.ParseFloat(val, 64)
		if err != nil {
			return fmt.Errorf("%s must be a number", state.Schema.Label)
		}
		if f < state.Schema.Min || f > state.Schema.Max {
			return fmt.Errorf("%s must be between %g and %g", state.Schema.Label, state.Schema.Min, state.Schema.Max)
		}
		return nil
	case KindDropdown:
		if strings.TrimSpace(val) == "" {
			return fmt.Errorf("%s must not be empty", state.Schema.Label)
		}
		return nil
	case KindColor:
		if _, err := parseHexColor(val); err != nil {
			return fmt.Errorf("%s must be in #RRGGBB format", state.Schema.Label)
		}
		return nil
	default:
		return fmt.Errorf("unsupported field type")
	}
}

func validateAll(states []RowState) []ValidationError {
	var out []ValidationError
	for i := range states {
		if err := validateField(&states[i]); err != nil {
			out = append(out, ValidationError{
				Key:     states[i].Schema.Key,
				Label:   states[i].Schema.Label,
				Message: err.Error(),
			})
		}
	}
	return out
}

// validateObject checks cross-field constraints across the full row state.
// Currently no cross-field rules exist. Add rules here when required.
// Called after validateAll passes, before statesToTreemap.
func validateObject(states []RowState) []ValidationError {
	return nil
}
```

---

## FILE: `win-go/internal/config/treemap.go`

```go
package config

import (
	"fmt"
	"image/color"
	"path/filepath"
	"runtime"
	"strings"
)

// Treemap holds treemap-related defaults (funcspec § Treemap Configuration Parameters).
type Treemap struct {
	MaxTiles            int
	ClumpThreshold      float64
	MinTileWidthPt      int
	MinTileHeightPt     int
	TilePaddingLeftPt   int
	TilePaddingTopPt    int
	TilePaddingRightPt  int
	TilePaddingBottomPt int

	NativeFolderBg, NativeFolderText color.RGBA
	PackedFolderBg, PackedFolderText color.RGBA
	NativeFileBg, NativeFileText     color.RGBA
	PackedFileBg, PackedFileText     color.RGBA
	NativeClumpBg, NativeClumpText   color.RGBA
	PackedClumpBg, PackedClumpText   color.RGBA

	TileFontName string

	HeadingMaxFontSizePt int
	HeadingMinFontSizePt int
	HeadingLineHeight    float64
	DetailsFontSizeRatio float64
	DetailsLineHeight    float64
	AboveDetailsRatio    float64
	LabelPlaceholder     string
	LabelDummy           string

	WinExeFiles   string
	LinuxExeFiles string
	MacOSExeFiles string
}

func DefaultTreemap() Treemap {
	return Treemap{
		MaxTiles:            20,
		ClumpThreshold:      0.01,
		MinTileWidthPt:      50,
		MinTileHeightPt:     50,
		TilePaddingLeftPt:   5,
		TilePaddingTopPt:    5,
		TilePaddingRightPt:  5,
		TilePaddingBottomPt: 5,

		NativeFolderBg:   hexRGBA("#80ef80"),
		NativeFolderText: hexRGBA("#000000"),
		PackedFolderBg:   hexRGBA("#06402b"),
		PackedFolderText: hexRGBA("#ffffff"),
		NativeFileBg:     hexRGBA("#ffb09c"),
		NativeFileText:   hexRGBA("#000000"),
		PackedFileBg:     hexRGBA("#900000"),
		PackedFileText:   hexRGBA("#ffffff"),
		NativeClumpBg:    hexRGBA("#aaaaaa"),
		NativeClumpText:  hexRGBA("#000000"),
		PackedClumpBg:    hexRGBA("#323232"),
		PackedClumpText:  hexRGBA("#ffffff"),

		TileFontName: "Segoe UI",

		HeadingMaxFontSizePt: 30,
		HeadingMinFontSizePt: 8,
		HeadingLineHeight:    1.2,
		DetailsFontSizeRatio: 0.8,
		DetailsLineHeight:    1.5,
		AboveDetailsRatio:    1.0,
		LabelPlaceholder:     "...",
		LabelDummy:           "...",

		WinExeFiles: "bat, com, exe, dll, scr, msi",
	}
}
```

---

## FILE: `win-go/internal/config/file.go`

```go
package config

import (
	"bufio"
	"bytes"
	"fmt"
	"image/color"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const ConfigFileName = "Erase & Rewrite.config.txt"

func ConfigPath() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(base, "Erase & Rewrite", ConfigFileName), nil
}

func LoadTreemapFromPath(path string) (Treemap, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return DefaultTreemap(), err
	}
	d := DefaultTreemap()
	applyTreemapLines(&d, data)
	return d, nil
}

func LoadOrInitTreemap() (Treemap, error) {
	path, err := ConfigPath()
	if err != nil {
		return DefaultTreemap(), err
	}
	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			d := DefaultTreemap()
			if werr := SaveTreemap(path, d); werr != nil {
				return d, werr
			}
			return d, nil
		}
		return DefaultTreemap(), err
	}
	d := DefaultTreemap()
	applyTreemapLines(&d, data)
	if !ConfigFileListsScanningUpdateInterval(data) {
		if werr := SaveTreemap(path, d); werr != nil {
			return d, werr
		}
	}
	return d, nil
}

func SaveTreemap(path string, t Treemap) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	var b strings.Builder
	w := func(key, val string) { fmt.Fprintf(&b, "%s = %s\n", key, val) }
	w("treemap.maxTiles", strconv.Itoa(nonzeroOr(t.MaxTiles, 20)))
	w("treemap.clumpThreshold", fmtPercentRatio(t.ClumpThreshold, 0.01))
	// ... remainder unchanged in source file ...
	return os.WriteFile(path, []byte(b.String()), 0o644)
}
```

---

Note:
- This bundle is intended for architecture review and includes the core related files requested.
- If you want, I can extend this bundle with additional full files (`run_windows.go`, `fontlist_windows.go`) in the same format.

---

## Architect Request: Current Defects and Precise Questions

### Context

We implemented a custom Win32 true grid host using `SysListView32` for rendering plus owner-managed in-cell editors in `win-go/internal/ui/custom_grid_windows.go`.

Current implementation includes:
- In-place text editor (`EDIT`) for text/numeric fields
- In-place combo editor (`COMBOBOX`) for dropdown fields
- In-place color editor (`EDIT` + right-side `...` button)
- Keyboard hooks for `Enter`, `Esc`, `Up`, `Down`
- Validation flow and config mapping in existing `validation.go` and `config_mapper.go`

The user confirms this is still not acceptable due to 4 concrete defects.

### Defect 1: Parameter column is too wide

Observed:
- Parameter column still dominates visual width and feels oversized.
- Value column does not have enough practical focus.

Expected:
- Parameter column should be compact, just enough for readable keys.
- Value column should get priority for editing ergonomics.

Question A1:
- What exact width policy should be used?
  - Fixed parameter width + stretch value?
  - Proportional split?
  - Min/max clamped split on resize?

Question A2:
- Should we support user-resizable columns, or enforce locked widths for deterministic UX?

### Defect 2: Parameter labels must not include units

Observed:
- Some labels include units, e.g. `(... pt)`.
- User explicitly requests unit-free parameter names.

Expected:
- Parameter names remain canonical keys/labels without embedded units.
- Units should be represented in value formatting or placeholder/help only.

Question B1:
- Confirm canonical approach:
  - Remove unit suffixes from all `RowSchema.Label`
  - Keep unit-aware parsing/formatting in value editor only (`20pt`, `4mm`)

Question B2:
- For point-based fields, should the displayed stored value always normalize to `Npt` after commit, even if user entered `mm`?

### Defect 3: Color pickers not working correctly

Observed:
- Intended behavior: click cell to edit, then click `...` button to open picker.
- Reported behavior: picker flow still not reliably usable.
- Current host handles color through `EDIT` + sibling `BUTTON`, with `WM_COMMAND/BN_CLICKED`.

Expected:
- Single click on color Value cell opens in-place color editor.
- Clicking `...` always opens `ChooseColor`.
- Chosen color updates current editor text immediately.
- `Enter` commits, `Esc` reverts, focus behavior consistent.

Question C1:
- Is `WM_COMMAND` from child button guaranteed to route through the subclassed ListView proc in this setup, or should color editor/button be hosted in a dedicated child container window owned by the dialog instead?

Question C2:
- Should color button trigger immediate commit after picker selection, or only update text and wait for explicit `Enter`/focus-out commit?

Question C3:
- What is the preferred fallback when picker is canceled?
  - Keep current typed value untouched (current behavior intent)
  - Restore pre-open value

### Defect 4: Font combo is not working

Observed:
- User requires a true combo box behavior (editable combo), not plain list semantics.
- Current implementation uses `CBS_DROPDOWN`, but behavior is reported as non-working.

Expected:
- Font row uses editable combo UI.
- Typing custom font names is possible.
- Selecting from list works.
- Commit/revert keys follow grid rules.

Question D1:
- For editable `COMBOBOX` (`CBS_DROPDOWN`), should commit use:
  - `CBN_SELCHANGE` + `CBN_EDITCHANGE` + `CBN_KILLFOCUS`, or
  - only explicit key-driven commit (`Enter`) and focus-out?

Question D2:
- Do we need to subclass the combo’s inner edit control separately to guarantee `Enter/Esc/Up/Down` semantics, instead of relying on parent combo/ListView message routing?

Question D3:
- Should `Up/Down` while combo drop-down list is open navigate list items (native combo behavior), and only perform row navigation when list is closed?

### Requested Architect Output

Please provide:
1. A deterministic event/state model for three editor types (`EDIT`, `COMBOBOX`, `EDIT+BUTTON`), including exact `WM_*`/`CBN_*`/`BN_*` handling.
2. A recommended ownership model for transient editors:
   - child of ListView vs child of dialog overlay host
   - pros/cons for focus and command routing reliability.
3. Exact column sizing policy with numeric defaults.
4. Label/value unit policy (labels unit-free; values unit-aware) confirmation.
5. A minimal acceptance checklist that can be verified manually in one pass.
