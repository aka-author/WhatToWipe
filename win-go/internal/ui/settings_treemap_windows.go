//go:build windows

package ui

import (
	"fmt"
	"image/color"
	"regexp"
	"strconv"
	"strings"
	"unsafe"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"eraserewrite/win-go/internal/config"
)

var hexColorRE = regexp.MustCompile(`^#[0-9A-Fa-f]{6}$`)

func showTreemapSettingsDialog(owner walk.Form, current config.Treemap, onApply func(config.Treemap)) {
	edited := current
	def := config.DefaultTreemap()

	installedFonts, err := installedFontFamilyNames()
	if err != nil {
		walk.MsgBox(owner, "Settings", "Cannot list installed fonts: "+err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	fontModel := mergeFontModel(installedFonts, current.TileFontName, def.TileFontName)

	var dlg *walk.Dialog
	var okBtn *walk.PushButton
	var rows []Widget
	var refreshUI []func()
	var cleanup []func()

	addRow := func(label string, editor Widget) {
		rows = append(rows, LineEdit{
			Text:     label,
			ReadOnly: true,
			MinSize:  Size{Width: 260},
		})
		rows = append(rows, editor)
	}

	setFields := func(t config.Treemap) {
		edited = t
		for _, fn := range refreshUI {
			fn()
		}
	}

	saveAndApply := func() bool {
		if err := validateTreemap(&edited); err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		p, err := config.ConfigPath()
		if err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		if err := config.SaveTreemap(p, edited); err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		if onApply != nil {
			onApply(edited)
		}
		return true
	}

	addIntRow := func(label string, min float64, max float64, get func() int, set func(int)) {
		var ne *walk.NumberEdit
		addRow(label, NumberEdit{
			AssignTo: &ne,
			MinValue: min,
			MaxValue: max,
			Decimals: 0,
			Value:    float64(get()),
			OnValueChanged: func() {
				set(int(ne.Value()))
			},
		})
		refreshUI = append(refreshUI, func() { _ = ne.SetValue(float64(get())) })
	}

	addFloatRow := func(label string, min float64, max float64, decimals int, get func() float64, set func(float64)) {
		var ne *walk.NumberEdit
		addRow(label, NumberEdit{
			AssignTo: &ne,
			MinValue: min,
			MaxValue: max,
			Decimals: decimals,
			Value:    get(),
			OnValueChanged: func() {
				set(ne.Value())
			},
		})
		refreshUI = append(refreshUI, func() { _ = ne.SetValue(get()) })
	}

	addTextRow := func(label string, allowEmpty bool, get func() string, set func(string)) {
		var le *walk.LineEdit
		addRow(label, LineEdit{
			AssignTo: &le,
			Text:     get(),
			OnEditingFinished: func() {
				v := strings.TrimSpace(le.Text())
				if !allowEmpty && v == "" {
					walk.MsgBox(dlg, "Settings", "Value must not be empty", walk.MsgBoxOK|walk.MsgBoxIconWarning)
					_ = le.SetText(get())
					return
				}
				set(v)
			},
		})
		refreshUI = append(refreshUI, func() { _ = le.SetText(get()) })
	}

	addPercentRow := func(label string, get func() float64, set func(float64)) {
		var le *walk.LineEdit
		addRow(label, LineEdit{
			AssignTo: &le,
			Text:     strconv.FormatFloat(get()*100.0, 'f', -1, 64) + "%",
			OnEditingFinished: func() {
				cl, err := parsePercentOrRatio(le.Text())
				if err != nil || cl <= 0 {
					walk.MsgBox(dlg, "Settings", "Must be positive (for example 1% or 0.01)", walk.MsgBoxOK|walk.MsgBoxIconWarning)
					_ = le.SetText(strconv.FormatFloat(get()*100.0, 'f', -1, 64) + "%")
					return
				}
				set(cl)
				_ = le.SetText(strconv.FormatFloat(cl*100.0, 'f', -1, 64) + "%")
			},
		})
		refreshUI = append(refreshUI, func() { _ = le.SetText(strconv.FormatFloat(get()*100.0, 'f', -1, 64) + "%") })
	}

	addFontRow := func(label string, get func() string, set func(string)) {
		var cb *walk.ComboBox
		curIdx := 0
		for i, name := range fontModel {
			if strings.EqualFold(name, get()) {
				curIdx = i
				break
			}
		}
		addRow(label, ComboBox{
			AssignTo:     &cb,
			Model:        fontModel,
			CurrentIndex: curIdx,
			Editable:     true,
			OnCurrentIndexChanged: func() {
				v := strings.TrimSpace(cb.Text())
				if v != "" {
					set(v)
				}
			},
			OnEditingFinished: func() {
				v := strings.TrimSpace(cb.Text())
				if v == "" {
					walk.MsgBox(dlg, "Settings", "Font name must not be empty", walk.MsgBoxOK|walk.MsgBoxIconWarning)
					_ = cb.SetText(get())
					return
				}
				set(v)
			},
		})
		refreshUI = append(refreshUI, func() { _ = cb.SetText(get()) })
	}

	addColorRow := func(label string, getHex func() string, set func(color.RGBA)) {
		var le *walk.LineEdit
		var btn *walk.PushButton
		var swatch *walk.Composite
		var swatchBrush *walk.SolidColorBrush

		updateSwatch := func(hex string) {
			c, err := parseHexColor(hex)
			if err != nil {
				c = color.RGBA{A: 255}
			}
			if swatchBrush != nil {
				swatchBrush.Dispose()
				swatchBrush = nil
			}
			br, err := walk.NewSolidColorBrush(walk.RGB(c.R, c.G, c.B))
			if err != nil {
				return
			}
			swatchBrush = br
			swatch.SetBackground(br)
			swatch.Invalidate()
		}

		addRow(label, Composite{
			Layout: HBox{MarginsZero: true, Spacing: 6},
			Children: []Widget{
				LineEdit{
					AssignTo: &le,
					Text:     getHex(),
					OnEditingFinished: func() {
						v := strings.TrimSpace(le.Text())
						if !hexColorRE.MatchString(v) {
							walk.MsgBox(dlg, "Settings", "Use format #RRGGBB", walk.MsgBoxOK|walk.MsgBoxIconWarning)
							_ = le.SetText(getHex())
							updateSwatch(getHex())
							return
						}
						c, _ := parseHexColor(v)
						set(c)
						_ = le.SetText(strings.ToUpper(v))
						updateSwatch(v)
					},
				},
				PushButton{
					AssignTo: &btn,
					Text:     "...",
					MaxSize:  Size{Width: 30},
					OnClicked: func() {
						chosen := showColorDialog(btn.Handle(), le.Text())
						if chosen == "" {
							return
						}
						c, _ := parseHexColor(chosen)
						set(c)
						_ = le.SetText(chosen)
						updateSwatch(chosen)
					},
				},
				Composite{
					AssignTo: &swatch,
					MinSize:  Size{Width: 20, Height: 20},
					MaxSize:  Size{Width: 20, Height: 20},
				},
			},
		})
		refreshUI = append(refreshUI, func() {
			_ = le.SetText(getHex())
			updateSwatch(getHex())
		})
		cleanup = append(cleanup, func() {
			if swatchBrush != nil {
				swatchBrush.Dispose()
			}
		})
	}

	addIntRow("treemap.maxTiles", 1, 1000000, func() int { return edited.MaxTiles }, func(v int) { edited.MaxTiles = v })
	addPercentRow("treemap.clumpThreshold", func() float64 { return edited.ClumpThreshold }, func(v float64) { edited.ClumpThreshold = v })
	addIntRow("treemap.minTileWidth (pt)", 1, 10000, func() int { return edited.MinTileWidthPt }, func(v int) { edited.MinTileWidthPt = v })
	addIntRow("treemap.minTileHeight (pt)", 1, 10000, func() int { return edited.MinTileHeightPt }, func(v int) { edited.MinTileHeightPt = v })
	addIntRow("treemap.tilePaddingLeft (pt)", 0, 10000, func() int { return edited.TilePaddingLeftPt }, func(v int) { edited.TilePaddingLeftPt = v })
	addIntRow("treemap.tilePaddingTop (pt)", 0, 10000, func() int { return edited.TilePaddingTopPt }, func(v int) { edited.TilePaddingTopPt = v })
	addIntRow("treemap.tilePaddingRight (pt)", 0, 10000, func() int { return edited.TilePaddingRightPt }, func(v int) { edited.TilePaddingRightPt = v })
	addIntRow("treemap.tilePaddingBottom (pt)", 0, 10000, func() int { return edited.TilePaddingBottomPt }, func(v int) { edited.TilePaddingBottomPt = v })
	addFontRow("treemap.tileFontName", func() string { return edited.TileFontName }, func(v string) { edited.TileFontName = v })
	addIntRow("treemap.headingMaxFontSize (pt)", 1, 1000, func() int { return edited.HeadingMaxFontSizePt }, func(v int) { edited.HeadingMaxFontSizePt = v })
	addIntRow("treemap.headingMinFontSize (pt)", 1, 1000, func() int { return edited.HeadingMinFontSizePt }, func(v int) { edited.HeadingMinFontSizePt = v })
	addFloatRow("treemap.headingLineHeight", 0.01, 100.0, 3, func() float64 { return edited.HeadingLineHeight }, func(v float64) { edited.HeadingLineHeight = v })
	addFloatRow("treemap.detailsFontSizeRatio", 0.01, 100.0, 3, func() float64 { return edited.DetailsFontSizeRatio }, func(v float64) { edited.DetailsFontSizeRatio = v })
	addFloatRow("treemap.detailsLineHeight", 0.01, 100.0, 3, func() float64 { return edited.DetailsLineHeight }, func(v float64) { edited.DetailsLineHeight = v })
	addFloatRow("treemap.aboveDetailsHeightRatio", 0.01, 100.0, 3, func() float64 { return edited.AboveDetailsRatio }, func(v float64) { edited.AboveDetailsRatio = v })
	addTextRow("treemap.labelPlaceholder", false, func() string { return edited.LabelPlaceholder }, func(v string) { edited.LabelPlaceholder = v })
	addTextRow("treemap.labelDummy", false, func() string { return edited.LabelDummy }, func(v string) { edited.LabelDummy = v })
	addColorRow("treemap.nativeFolderBgColor", func() string { return formatRGBHex(edited.NativeFolderBg) }, func(c color.RGBA) { edited.NativeFolderBg = c })
	addColorRow("treemap.nativeFolderTextColor", func() string { return formatRGBHex(edited.NativeFolderText) }, func(c color.RGBA) { edited.NativeFolderText = c })
	addColorRow("treemap.packedFolderBgColor", func() string { return formatRGBHex(edited.PackedFolderBg) }, func(c color.RGBA) { edited.PackedFolderBg = c })
	addColorRow("treemap.packedFolderTextColor", func() string { return formatRGBHex(edited.PackedFolderText) }, func(c color.RGBA) { edited.PackedFolderText = c })
	addColorRow("treemap.nativeFileBgColor", func() string { return formatRGBHex(edited.NativeFileBg) }, func(c color.RGBA) { edited.NativeFileBg = c })
	addColorRow("treemap.nativeFileTextColor", func() string { return formatRGBHex(edited.NativeFileText) }, func(c color.RGBA) { edited.NativeFileText = c })
	addColorRow("treemap.packedFileBgColor", func() string { return formatRGBHex(edited.PackedFileBg) }, func(c color.RGBA) { edited.PackedFileBg = c })
	addColorRow("treemap.packedFileTextColor", func() string { return formatRGBHex(edited.PackedFileText) }, func(c color.RGBA) { edited.PackedFileText = c })
	addColorRow("treemap.nativeClumpBgColor", func() string { return formatRGBHex(edited.NativeClumpBg) }, func(c color.RGBA) { edited.NativeClumpBg = c })
	addColorRow("treemap.nativeClumpTextColor", func() string { return formatRGBHex(edited.NativeClumpText) }, func(c color.RGBA) { edited.NativeClumpText = c })
	addColorRow("treemap.packedClumpBgColor", func() string { return formatRGBHex(edited.PackedClumpBg) }, func(c color.RGBA) { edited.PackedClumpBg = c })
	addColorRow("treemap.packedClumpTextColor", func() string { return formatRGBHex(edited.PackedClumpText) }, func(c color.RGBA) { edited.PackedClumpText = c })
	addTextRow("treemap.win.exeFiles", true, func() string { return edited.WinExeFiles }, func(v string) { edited.WinExeFiles = v })
	addTextRow("treemap.linux.exeFiles", true, func() string { return edited.LinuxExeFiles }, func(v string) { edited.LinuxExeFiles = v })
	addTextRow("treemap.macos.exeFiles", true, func() string { return edited.MacOSExeFiles }, func(v string) { edited.MacOSExeFiles = v })

	rows = append([]Widget{
		LineEdit{Text: "Parameter", ReadOnly: true, MinSize: Size{Width: 260}},
		LineEdit{Text: "Value", ReadOnly: true},
	}, rows...)

	var icon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		icon = ic
	}

	decl := Dialog{
		AssignTo:      &dlg,
		Title:         "Settings",
		Icon:          icon,
		MinSize:       Size{Width: 720, Height: 560},
		Size:          Size{Width: 820, Height: 640},
		DefaultButton: &okBtn,
		Layout:        VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			ScrollView{
				VerticalFixed: false,
				Layout:        VBox{MarginsZero: true},
				Children: []Widget{
					Composite{
						Layout: Grid{
							Columns: 2,
							Spacing: 8,
							Margins: Margins{Left: 0, Top: 0, Right: 0, Bottom: 0},
						},
						Children: rows,
					},
				},
			},
			Composite{
				Layout: HBox{MarginsZero: true, Spacing: 8},
				Children: []Widget{
					PushButton{
						Text: "Reset Treemap Defaults",
						OnClicked: func() {
							setFields(def)
						},
					},
					HSpacer{},
					PushButton{
						Text: "Cancel",
						OnClicked: func() {
							dlg.Cancel()
						},
					},
					PushButton{
						Text:     "Apply",
						OnClicked: func() {
							_ = saveAndApply()
						},
					},
					PushButton{
						AssignTo: &okBtn,
						Text:     "OK",
						OnClicked: func() {
							if saveAndApply() {
								dlg.Accept()
							}
						},
					},
				},
			},
		},
	}

	if err := decl.Create(owner); err != nil {
		walk.MsgBox(owner, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	dlg.Disposing().Attach(func() {
		for _, fn := range cleanup {
			fn()
		}
	})
	setFields(current)
	_ = dlg.Run()
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

func pickColor(owner walk.Form, start color.RGBA) (color.RGBA, bool) {
	var custom [16]win.COLORREF
	cc := win.CHOOSECOLOR{
		LStructSize:  uint32(unsafe.Sizeof(win.CHOOSECOLOR{})),
		HwndOwner:    owner.AsFormBase().Handle(),
		RgbResult:    win.RGB(start.R, start.G, start.B),
		LpCustColors: &custom,
		Flags:        win.CC_FULLOPEN | win.CC_RGBINIT,
	}
	if !win.ChooseColor(&cc) {
		return color.RGBA{}, false
	}
	return color.RGBA{
		R: byte(cc.RgbResult & 0xFF),
		G: byte((cc.RgbResult >> 8) & 0xFF),
		B: byte((cc.RgbResult >> 16) & 0xFF),
		A: 255,
	}, true
}

func parsePercentOrRatio(s string) (float64, error) {
	s = strings.TrimSpace(strings.ToLower(s))
	if s == "" {
		return 0, fmt.Errorf("empty value")
	}
	if strings.HasSuffix(s, "%") {
		v, err := strconv.ParseFloat(strings.TrimSpace(strings.TrimSuffix(s, "%")), 64)
		if err != nil {
			return 0, err
		}
		return v / 100.0, nil
	}
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0, err
	}
	if v > 1 {
		return v / 100.0, nil
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
