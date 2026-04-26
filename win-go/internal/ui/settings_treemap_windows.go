//go:build windows

package ui

import (
	"fmt"
	"image/color"
	"strconv"
	"strings"
	"unsafe"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"eraserewrite/win-go/internal/config"
)

type colorField struct {
	edit   *walk.LineEdit
	swatch *walk.Label
	brush  *walk.SolidColorBrush
}

func (cf *colorField) dispose() {
	if cf.brush != nil {
		cf.brush.Dispose()
		cf.brush = nil
	}
}

func (cf *colorField) updateSwatchFromText() {
	if cf == nil || cf.edit == nil || cf.swatch == nil {
		return
	}
	c, err := parseHexColor(cf.edit.Text())
	if err != nil {
		return
	}
	cf.dispose()
	b, berr := walk.NewSolidColorBrush(walk.RGB(c.R, c.G, c.B))
	if berr != nil {
		return
	}
	cf.brush = b
	cf.swatch.SetBackground(b)
}

func (cf *colorField) setHex(hex string) {
	if cf.edit != nil {
		_ = cf.edit.SetText(strings.ToUpper(hex))
	}
	cf.updateSwatchFromText()
}

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
	var okBtn, applyBtn *walk.PushButton

	var maxTilesEdit, clumpThresholdEdit *walk.LineEdit
	var minTileWidthEdit, minTileHeightEdit *walk.LineEdit
	var padLeftEdit, padTopEdit, padRightEdit, padBottomEdit *walk.LineEdit
	var tileFontCombo *walk.ComboBox
	var headingMaxEdit, headingMinEdit *walk.LineEdit
	var headingLineHeightEdit, detailsFontRatioEdit, detailsLineHeightEdit, aboveDetailsEdit *walk.LineEdit
	var labelPlaceholderEdit, labelDummyEdit *walk.LineEdit
	var winExeEdit, linuxExeEdit, macExeEdit *walk.LineEdit

	cfNativeFolderBg := &colorField{}
	cfNativeFolderText := &colorField{}
	cfPackedFolderBg := &colorField{}
	cfPackedFolderText := &colorField{}
	cfNativeFileBg := &colorField{}
	cfNativeFileText := &colorField{}
	cfPackedFileBg := &colorField{}
	cfPackedFileText := &colorField{}
	cfNativeClumpBg := &colorField{}
	cfNativeClumpText := &colorField{}
	cfPackedClumpBg := &colorField{}
	cfPackedClumpText := &colorField{}
	colorFields := []*colorField{
		cfNativeFolderBg, cfNativeFolderText,
		cfPackedFolderBg, cfPackedFolderText,
		cfNativeFileBg, cfNativeFileText,
		cfPackedFileBg, cfPackedFileText,
		cfNativeClumpBg, cfNativeClumpText,
		cfPackedClumpBg, cfPackedClumpText,
	}

	setFields := func(t config.Treemap) {
		_ = maxTilesEdit.SetText(strconv.Itoa(t.MaxTiles))
		_ = clumpThresholdEdit.SetText(strconv.FormatFloat(t.ClumpThreshold*100.0, 'f', -1, 64) + "%")
		_ = minTileWidthEdit.SetText(strconv.Itoa(t.MinTileWidthPt))
		_ = minTileHeightEdit.SetText(strconv.Itoa(t.MinTileHeightPt))
		_ = padLeftEdit.SetText(strconv.Itoa(t.TilePaddingLeftPt))
		_ = padTopEdit.SetText(strconv.Itoa(t.TilePaddingTopPt))
		_ = padRightEdit.SetText(strconv.Itoa(t.TilePaddingRightPt))
		_ = padBottomEdit.SetText(strconv.Itoa(t.TilePaddingBottomPt))
		if tileFontCombo != nil {
			face := strings.TrimSpace(t.TileFontName)
			found := false
			for i, n := range fontModel {
				if strings.EqualFold(n, face) {
					_ = tileFontCombo.SetCurrentIndex(i)
					found = true
					break
				}
			}
			if !found && len(fontModel) > 0 {
				_ = tileFontCombo.SetCurrentIndex(0)
			}
		}
		_ = headingMaxEdit.SetText(strconv.Itoa(t.HeadingMaxFontSizePt))
		_ = headingMinEdit.SetText(strconv.Itoa(t.HeadingMinFontSizePt))
		_ = headingLineHeightEdit.SetText(strconv.FormatFloat(t.HeadingLineHeight, 'f', -1, 64))
		_ = detailsFontRatioEdit.SetText(strconv.FormatFloat(t.DetailsFontSizeRatio, 'f', -1, 64))
		_ = detailsLineHeightEdit.SetText(strconv.FormatFloat(t.DetailsLineHeight, 'f', -1, 64))
		_ = aboveDetailsEdit.SetText(strconv.FormatFloat(t.AboveDetailsRatio, 'f', -1, 64))
		_ = labelPlaceholderEdit.SetText(t.LabelPlaceholder)
		_ = labelDummyEdit.SetText(t.LabelDummy)
		_ = winExeEdit.SetText(strings.TrimSpace(t.WinExeFiles))
		_ = linuxExeEdit.SetText(strings.TrimSpace(t.LinuxExeFiles))
		_ = macExeEdit.SetText(strings.TrimSpace(t.MacOSExeFiles))

		cfNativeFolderBg.setHex(formatRGBHex(t.NativeFolderBg))
		cfNativeFolderText.setHex(formatRGBHex(t.NativeFolderText))
		cfPackedFolderBg.setHex(formatRGBHex(t.PackedFolderBg))
		cfPackedFolderText.setHex(formatRGBHex(t.PackedFolderText))
		cfNativeFileBg.setHex(formatRGBHex(t.NativeFileBg))
		cfNativeFileText.setHex(formatRGBHex(t.NativeFileText))
		cfPackedFileBg.setHex(formatRGBHex(t.PackedFileBg))
		cfPackedFileText.setHex(formatRGBHex(t.PackedFileText))
		cfNativeClumpBg.setHex(formatRGBHex(t.NativeClumpBg))
		cfNativeClumpText.setHex(formatRGBHex(t.NativeClumpText))
		cfPackedClumpBg.setHex(formatRGBHex(t.PackedClumpBg))
		cfPackedClumpText.setHex(formatRGBHex(t.PackedClumpText))
	}

	chooseColor := func(cf *colorField) {
		if cf == nil || cf.edit == nil {
			return
		}
		start, err := parseHexColor(cf.edit.Text())
		if err != nil {
			start = color.RGBA{A: 255}
		}
		picked, ok := pickColor(owner, start)
		if !ok {
			return
		}
		cf.setHex(formatRGBHex(picked))
	}

	applyEdits := func() error {
		parseInt := func(name string, e *walk.LineEdit, min int) (int, error) {
			v, err := strconv.Atoi(strings.TrimSpace(e.Text()))
			if err != nil || v < min {
				return 0, fmt.Errorf("%s must be an integer >= %d", name, min)
			}
			return v, nil
		}
		parseFloatPos := func(name string, e *walk.LineEdit) (float64, error) {
			v, err := strconv.ParseFloat(strings.TrimSpace(e.Text()), 64)
			if err != nil || v <= 0 {
				return 0, fmt.Errorf("%s must be > 0", name)
			}
			return v, nil
		}
		parseColor := func(name string, cf *colorField) (color.RGBA, error) {
			c, err := parseHexColor(cf.edit.Text())
			if err != nil {
				return color.RGBA{}, fmt.Errorf("%s: %w", name, err)
			}
			return c, nil
		}

		maxTiles, err := parseInt("treemap.maxTiles", maxTilesEdit, 1)
		if err != nil {
			return err
		}
		clumpTh, err := parsePercentOrRatio(strings.TrimSpace(clumpThresholdEdit.Text()))
		if err != nil || clumpTh <= 0 {
			return fmt.Errorf("treemap.clumpThreshold must be positive (for example 1%% or 0.01)")
		}
		minW, err := parseInt("treemap.minTileWidth", minTileWidthEdit, 1)
		if err != nil {
			return err
		}
		minH, err := parseInt("treemap.minTileHeight", minTileHeightEdit, 1)
		if err != nil {
			return err
		}
		padL, err := parseInt("treemap.tilePaddingLeft", padLeftEdit, 0)
		if err != nil {
			return err
		}
		padT, err := parseInt("treemap.tilePaddingTop", padTopEdit, 0)
		if err != nil {
			return err
		}
		padR, err := parseInt("treemap.tilePaddingRight", padRightEdit, 0)
		if err != nil {
			return err
		}
		padB, err := parseInt("treemap.tilePaddingBottom", padBottomEdit, 0)
		if err != nil {
			return err
		}
		headingMax, err := parseInt("treemap.headingMaxFontSize", headingMaxEdit, 1)
		if err != nil {
			return err
		}
		headingMin, err := parseInt("treemap.headingMinFontSize", headingMinEdit, 1)
		if err != nil {
			return err
		}
		if headingMin > headingMax {
			return fmt.Errorf("treemap.headingMinFontSize must be <= treemap.headingMaxFontSize")
		}
		headingLH, err := parseFloatPos("treemap.headingLineHeight", headingLineHeightEdit)
		if err != nil {
			return err
		}
		detailsRatio, err := parseFloatPos("treemap.detailsFontSizeRatio", detailsFontRatioEdit)
		if err != nil {
			return err
		}
		detailsLH, err := parseFloatPos("treemap.detailsLineHeight", detailsLineHeightEdit)
		if err != nil {
			return err
		}
		aboveDetails, err := parseFloatPos("treemap.aboveDetailsHeightRatio", aboveDetailsEdit)
		if err != nil {
			return err
		}
		if tileFontCombo == nil {
			return fmt.Errorf("treemap.tileFontName: internal error")
		}
		face := strings.TrimSpace(tileFontCombo.Text())
		if face == "" {
			return fmt.Errorf("treemap.tileFontName must not be empty")
		}
		labelPlaceholder := strings.TrimSpace(labelPlaceholderEdit.Text())
		labelDummy := strings.TrimSpace(labelDummyEdit.Text())
		if labelPlaceholder == "" || labelDummy == "" {
			return fmt.Errorf("treemap.labelPlaceholder and treemap.labelDummy must not be empty")
		}

		nFolderBg, err := parseColor("treemap.nativeFolderBgColor", cfNativeFolderBg)
		if err != nil {
			return err
		}
		nFolderText, err := parseColor("treemap.nativeFolderTextColor", cfNativeFolderText)
		if err != nil {
			return err
		}
		pFolderBg, err := parseColor("treemap.packedFolderBgColor", cfPackedFolderBg)
		if err != nil {
			return err
		}
		pFolderText, err := parseColor("treemap.packedFolderTextColor", cfPackedFolderText)
		if err != nil {
			return err
		}
		nFileBg, err := parseColor("treemap.nativeFileBgColor", cfNativeFileBg)
		if err != nil {
			return err
		}
		nFileText, err := parseColor("treemap.nativeFileTextColor", cfNativeFileText)
		if err != nil {
			return err
		}
		pFileBg, err := parseColor("treemap.packedFileBgColor", cfPackedFileBg)
		if err != nil {
			return err
		}
		pFileText, err := parseColor("treemap.packedFileTextColor", cfPackedFileText)
		if err != nil {
			return err
		}
		nClumpBg, err := parseColor("treemap.nativeClumpBgColor", cfNativeClumpBg)
		if err != nil {
			return err
		}
		nClumpText, err := parseColor("treemap.nativeClumpTextColor", cfNativeClumpText)
		if err != nil {
			return err
		}
		pClumpBg, err := parseColor("treemap.packedClumpBgColor", cfPackedClumpBg)
		if err != nil {
			return err
		}
		pClumpText, err := parseColor("treemap.packedClumpTextColor", cfPackedClumpText)
		if err != nil {
			return err
		}

		edited.MaxTiles = maxTiles
		edited.ClumpThreshold = clumpTh
		edited.MinTileWidthPt = minW
		edited.MinTileHeightPt = minH
		edited.TilePaddingLeftPt = padL
		edited.TilePaddingTopPt = padT
		edited.TilePaddingRightPt = padR
		edited.TilePaddingBottomPt = padB
		edited.TileFontName = face
		edited.HeadingMaxFontSizePt = headingMax
		edited.HeadingMinFontSizePt = headingMin
		edited.HeadingLineHeight = headingLH
		edited.DetailsFontSizeRatio = detailsRatio
		edited.DetailsLineHeight = detailsLH
		edited.AboveDetailsRatio = aboveDetails
		edited.LabelPlaceholder = labelPlaceholder
		edited.LabelDummy = labelDummy
		edited.WinExeFiles = strings.TrimSpace(winExeEdit.Text())
		edited.LinuxExeFiles = strings.TrimSpace(linuxExeEdit.Text())
		edited.MacOSExeFiles = strings.TrimSpace(macExeEdit.Text())
		edited.NativeFolderBg = nFolderBg
		edited.NativeFolderText = nFolderText
		edited.PackedFolderBg = pFolderBg
		edited.PackedFolderText = pFolderText
		edited.NativeFileBg = nFileBg
		edited.NativeFileText = nFileText
		edited.PackedFileBg = pFileBg
		edited.PackedFileText = pFileText
		edited.NativeClumpBg = nClumpBg
		edited.NativeClumpText = nClumpText
		edited.PackedClumpBg = pClumpBg
		edited.PackedClumpText = pClumpText
		return nil
	}

	saveAndApply := func() bool {
		if err := applyEdits(); err != nil {
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

	gridRows := []Widget{
		Label{Text: "treemap.maxTiles"},
		LineEdit{AssignTo: &maxTilesEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.clumpThreshold"},
		LineEdit{AssignTo: &clumpThresholdEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.minTileWidth (pt)"},
		LineEdit{AssignTo: &minTileWidthEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.minTileHeight (pt)"},
		LineEdit{AssignTo: &minTileHeightEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.tilePaddingLeft (pt)"},
		LineEdit{AssignTo: &padLeftEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.tilePaddingTop (pt)"},
		LineEdit{AssignTo: &padTopEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.tilePaddingRight (pt)"},
		LineEdit{AssignTo: &padRightEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.tilePaddingBottom (pt)"},
		LineEdit{AssignTo: &padBottomEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.tileFontName"},
		ComboBox{AssignTo: &tileFontCombo, Model: fontModel, Editable: false},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.headingMaxFontSize (pt)"},
		LineEdit{AssignTo: &headingMaxEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.headingMinFontSize (pt)"},
		LineEdit{AssignTo: &headingMinEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.headingLineHeight"},
		LineEdit{AssignTo: &headingLineHeightEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.detailsFontSizeRatio"},
		LineEdit{AssignTo: &detailsFontRatioEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.detailsLineHeight"},
		LineEdit{AssignTo: &detailsLineHeightEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.aboveDetailsHeightRatio"},
		LineEdit{AssignTo: &aboveDetailsEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.labelPlaceholder"},
		LineEdit{AssignTo: &labelPlaceholderEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.labelDummy"},
		LineEdit{AssignTo: &labelDummyEdit},
		HSpacer{},
		HSpacer{},
	}
	addColor := func(name string, cf *colorField) {
		gridRows = append(gridRows,
			Label{Text: name},
			LineEdit{
				AssignTo: &cf.edit,
				OnTextChanged: func() {
					cf.updateSwatchFromText()
				},
			},
			Label{
				AssignTo: &cf.swatch,
				Text:     "      ",
				MinSize:  Size{Width: 46, Height: 20},
				MaxSize:  Size{Width: 46, Height: 20},
			},
			PushButton{
				Text: "Pick\u2026",
				OnClicked: func() {
					chooseColor(cf)
				},
			},
		)
	}
	addColor("treemap.nativeFolderBgColor", cfNativeFolderBg)
	addColor("treemap.nativeFolderTextColor", cfNativeFolderText)
	addColor("treemap.packedFolderBgColor", cfPackedFolderBg)
	addColor("treemap.packedFolderTextColor", cfPackedFolderText)
	addColor("treemap.nativeFileBgColor", cfNativeFileBg)
	addColor("treemap.nativeFileTextColor", cfNativeFileText)
	addColor("treemap.packedFileBgColor", cfPackedFileBg)
	addColor("treemap.packedFileTextColor", cfPackedFileText)
	addColor("treemap.nativeClumpBgColor", cfNativeClumpBg)
	addColor("treemap.nativeClumpTextColor", cfNativeClumpText)
	addColor("treemap.packedClumpBgColor", cfPackedClumpBg)
	addColor("treemap.packedClumpTextColor", cfPackedClumpText)
	gridRows = append(gridRows,
		Label{Text: "treemap.win.exeFiles"},
		LineEdit{AssignTo: &winExeEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.linux.exeFiles"},
		LineEdit{AssignTo: &linuxExeEdit},
		HSpacer{},
		HSpacer{},
		Label{Text: "treemap.macos.exeFiles"},
		LineEdit{AssignTo: &macExeEdit},
		HSpacer{},
		HSpacer{},
	)

	var icon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		icon = ic
	}

	decl := Dialog{
		AssignTo:      &dlg,
		Title:         "Settings",
		Icon:          icon,
		MinSize:       Size{Width: 900, Height: 760},
		Size:          Size{Width: 980, Height: 860},
		DefaultButton: &okBtn,
		Layout:        VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			Label{Text: "Treemap Configuration Parameters"},
			Composite{
				Layout: Grid{Columns: 4, MarginsZero: true, Spacing: 6},
				Children: gridRows,
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
						AssignTo: &applyBtn,
						Text:     "Apply",
						OnClicked: func() {
							_ = applyBtn
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
	setFields(current)
	_ = dlg.Run()
	for _, cf := range colorFields {
		cf.dispose()
	}
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
