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
	widgetRef    func() walk.Widget
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
	if b == nil {
		return
	}
	w := b.focusWidget
	if w == nil && b.widgetRef != nil {
		w = b.widgetRef()
	}
	if w == nil {
		return
	}
	_ = w.SetFocus()
	if b.scrollParent != nil {
		win.SendMessage(b.scrollParent.Handle(), win.WM_VSCROLL, uintptr(win.SB_TOP), 0)
		step := 0
		for step < 200 {
			cur := w.BoundsPixels()
			if cur.Y >= 0 && cur.Y+cur.Height <= b.scrollParent.ClientBoundsPixels().Height {
				break
			}
			if cur.Y < 0 {
				win.SendMessage(b.scrollParent.Handle(), win.WM_VSCROLL, uintptr(win.SB_LINEUP), 0)
			} else {
				win.SendMessage(b.scrollParent.Handle(), win.WM_VSCROLL, uintptr(win.SB_LINEDOWN), 0)
			}
			step++
		}
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
		}
	}
	clearError := func() {
		showError("")
	}
	onChanged := func() { clearError() }
	rowWidgets := buildRowWidgets(s, bindings, showError, onChanged)

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
				Children: []Widget{
					Composite{
						AssignTo: &gridHost,
						Layout:   Grid{Columns: 2, Margins: Margins{0, 0, 0, 0}, Spacing: 8},
						MinSize:  Size{Width: 640, Height: len(s.states) * 34},
						Children: rowWidgets,
					},
				},
			},
			Label{
				AssignTo: &errLabel,
				Text:     "",
				Visible:  true,
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
		log.Printf("ERROR: settings dialog create failed: %v", err)
		walk.MsgBox(owner, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	log.Printf("DEBUG: gridHost nil? %v", gridHost == nil)
	log.Printf("DEBUG: sv nil? %v", sv == nil)
	log.Printf("DEBUG: state count: %d", len(s.states))
	if gridHost == nil || sv == nil {
		walk.MsgBox(owner, "Settings", "Settings grid host did not initialize.", walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	for i := range bindings {
		bindings[i].scrollParent = sv
		bindings[i].setValue(s.states[i].LastGood)
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

func buildRowWidgets(s *SettingsState, bindings []rowControlBinding, showError func(string), onChanged func()) []Widget {
	widgets := make([]Widget, 0, len(s.states)*2)
	for i := range s.states {
		state := &s.states[i]
		widgets = append(widgets, Label{
			Text:    state.Schema.Label,
			MinSize: Size{Width: 270},
		})
		widgets = append(widgets, buildEditorWidget(state, s, &bindings[i], showError, onChanged))
	}
	return widgets
}

func buildEditorWidget(state *RowState, ss *SettingsState, b *rowControlBinding, showError func(string), onChanged func()) Widget {
	switch state.Schema.Kind {
	case KindNumeric:
		var ne *walk.NumberEdit
		initVal, _ := strconv.ParseFloat(state.LastGood, 64)
		b.widgetRef = func() walk.Widget { return ne }
		commit := func() {
			if ne == nil {
				return
			}
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
		b.applyValue = func(v string) {
			if ne == nil {
				return
			}
			if f, err := strconv.ParseFloat(strings.TrimSpace(v), 64); err == nil {
				_ = ne.SetValue(f)
			}
		}
		return NumberEdit{
			AssignTo: &ne,
			Value:    initVal,
			MinValue: state.Schema.Min,
			MaxValue: state.Schema.Max,
			Decimals: state.Schema.Decimals,
			OnValueChanged: func() {
				commit()
			},
			OnKeyDown: func(key walk.Key) {
				if key == walk.KeyEscape {
					if old, err := strconv.ParseFloat(state.LastGood, 64); err == nil {
						_ = ne.SetValue(old)
					}
					state.PendingValue = state.LastGood
					onChanged()
				}
			},
		}
	case KindDropdown:
		var cb *walk.ComboBox
		b.widgetRef = func() walk.Widget { return cb }
		idx := -1
		for i, opt := range state.Schema.Options {
			if strings.EqualFold(strings.TrimSpace(opt), strings.TrimSpace(state.LastGood)) {
				idx = i
				break
			}
		}
		commit := func() {
			if cb == nil {
				return
			}
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
		b.applyValue = func(v string) {
			if cb != nil {
				_ = cb.SetText(v)
			}
		}
		return ComboBox{
			AssignTo:              &cb,
			Editable:              true,
			Model:                 state.Schema.Options,
			CurrentIndex:          idx,
			OnCurrentIndexChanged: commit,
			OnEditingFinished:     commit,
			OnKeyDown: func(key walk.Key) {
				if key == walk.KeyEscape {
					_ = cb.SetText(state.LastGood)
					state.PendingValue = state.LastGood
					onChanged()
				}
			},
		}
	case KindColor:
		var le *walk.LineEdit
		var btn *walk.PushButton
		var swatch *walk.Composite
		b.widgetRef = func() walk.Widget { return le }

		commit := func() bool {
			if le == nil {
				return false
			}
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
		b.applyValue = func(v string) {
			if le == nil {
				return
			}
			_ = le.SetText(v)
			updateSwatch(swatch, v)
		}
		b.colorLine = le
		b.colorSwatch = swatch
		return Composite{
			Layout: HBox{MarginsZero: true, Spacing: 4},
			Children: []Widget{
				LineEdit{
					AssignTo: &le,
					Text:     state.LastGood,
					OnEditingFinished: func() {
						_ = commit()
					},
				},
				PushButton{
					AssignTo: &btn,
					Text:     "...",
					MaxSize:  Size{Width: 30},
					OnClicked: func() {
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
					},
				},
				Composite{
					AssignTo: &swatch,
					MinSize:  Size{Width: 20, Height: 20},
					MaxSize:  Size{Width: 20, Height: 20},
					OnBoundsChanged: func() {
						updateSwatch(swatch, state.LastGood)
					},
				},
			},
		}
	default:
		var le *walk.LineEdit
		b.widgetRef = func() walk.Widget { return le }
		commit := func() {
			if le == nil {
				return
			}
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
		b.applyValue = func(v string) {
			if le != nil {
				_ = le.SetText(v)
			}
		}
		return LineEdit{
			AssignTo:          &le,
			Text:              state.LastGood,
			OnEditingFinished: commit,
			OnKeyDown: func(key walk.Key) {
				if key == walk.KeyEscape {
					_ = le.SetText(state.LastGood)
					state.PendingValue = state.LastGood
					onChanged()
				}
			},
		}
	}
	return Label{Text: "unsupported"}
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

func parseFloatValue(s string) (float64, error) {
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

