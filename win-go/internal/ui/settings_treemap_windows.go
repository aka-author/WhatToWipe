//go:build windows

package ui

import (
	"fmt"
	"image/color"
	"strconv"
	"strings"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"

	"eraserewrite/win-go/internal/config"
)

func showTreemapSettingsDialog(owner walk.Form, current config.Treemap) (config.Treemap, bool) {
	edited := current
	def := config.DefaultTreemap()

	var dlg *walk.Dialog
	var okBtn, applyBtn *walk.PushButton

	var maxTilesEdit, clumpThresholdEdit *walk.LineEdit
	var minTileWidthEdit, minTileHeightEdit *walk.LineEdit
	var padLeftEdit, padTopEdit, padRightEdit, padBottomEdit *walk.LineEdit
	var tileFontEdit, headingMaxEdit, headingMinEdit *walk.LineEdit
	var headingLineHeightEdit, detailsFontRatioEdit, detailsLineHeightEdit, aboveDetailsEdit *walk.LineEdit
	var labelPlaceholderEdit, labelDummyEdit *walk.LineEdit
	var nativeFolderBgEdit, nativeFolderTextEdit *walk.LineEdit
	var packedFolderBgEdit, packedFolderTextEdit *walk.LineEdit
	var nativeFileBgEdit, nativeFileTextEdit *walk.LineEdit
	var packedFileBgEdit, packedFileTextEdit *walk.LineEdit
	var nativeClumpBgEdit, nativeClumpTextEdit *walk.LineEdit
	var packedClumpBgEdit, packedClumpTextEdit *walk.LineEdit
	var winExeEdit, linuxExeEdit, macExeEdit *walk.LineEdit

	setFields := func(t config.Treemap) {
		_ = maxTilesEdit.SetText(strconv.Itoa(t.MaxTiles))
		_ = clumpThresholdEdit.SetText(strconv.FormatFloat(t.ClumpThreshold*100.0, 'f', -1, 64) + "%")
		_ = minTileWidthEdit.SetText(strconv.Itoa(t.MinTileWidthPt))
		_ = minTileHeightEdit.SetText(strconv.Itoa(t.MinTileHeightPt))
		_ = padLeftEdit.SetText(strconv.Itoa(t.TilePaddingLeftPt))
		_ = padTopEdit.SetText(strconv.Itoa(t.TilePaddingTopPt))
		_ = padRightEdit.SetText(strconv.Itoa(t.TilePaddingRightPt))
		_ = padBottomEdit.SetText(strconv.Itoa(t.TilePaddingBottomPt))
		_ = tileFontEdit.SetText(strings.TrimSpace(t.TileFontName))
		_ = headingMaxEdit.SetText(strconv.Itoa(t.HeadingMaxFontSizePt))
		_ = headingMinEdit.SetText(strconv.Itoa(t.HeadingMinFontSizePt))
		_ = headingLineHeightEdit.SetText(strconv.FormatFloat(t.HeadingLineHeight, 'f', -1, 64))
		_ = detailsFontRatioEdit.SetText(strconv.FormatFloat(t.DetailsFontSizeRatio, 'f', -1, 64))
		_ = detailsLineHeightEdit.SetText(strconv.FormatFloat(t.DetailsLineHeight, 'f', -1, 64))
		_ = aboveDetailsEdit.SetText(strconv.FormatFloat(t.AboveDetailsRatio, 'f', -1, 64))
		_ = labelPlaceholderEdit.SetText(t.LabelPlaceholder)
		_ = labelDummyEdit.SetText(t.LabelDummy)
		_ = nativeFolderBgEdit.SetText(formatRGBHex(t.NativeFolderBg))
		_ = nativeFolderTextEdit.SetText(formatRGBHex(t.NativeFolderText))
		_ = packedFolderBgEdit.SetText(formatRGBHex(t.PackedFolderBg))
		_ = packedFolderTextEdit.SetText(formatRGBHex(t.PackedFolderText))
		_ = nativeFileBgEdit.SetText(formatRGBHex(t.NativeFileBg))
		_ = nativeFileTextEdit.SetText(formatRGBHex(t.NativeFileText))
		_ = packedFileBgEdit.SetText(formatRGBHex(t.PackedFileBg))
		_ = packedFileTextEdit.SetText(formatRGBHex(t.PackedFileText))
		_ = nativeClumpBgEdit.SetText(formatRGBHex(t.NativeClumpBg))
		_ = nativeClumpTextEdit.SetText(formatRGBHex(t.NativeClumpText))
		_ = packedClumpBgEdit.SetText(formatRGBHex(t.PackedClumpBg))
		_ = packedClumpTextEdit.SetText(formatRGBHex(t.PackedClumpText))
		_ = winExeEdit.SetText(strings.TrimSpace(t.WinExeFiles))
		_ = linuxExeEdit.SetText(strings.TrimSpace(t.LinuxExeFiles))
		_ = macExeEdit.SetText(strings.TrimSpace(t.MacOSExeFiles))
	}

	applyEdits := func() error {
		parseInt := func(name string, e *walk.LineEdit, min int) (int, error) {
			s := strings.TrimSpace(e.Text())
			v, err := strconv.Atoi(s)
			if err != nil || v < min {
				return 0, fmt.Errorf("%s must be an integer >= %d", name, min)
			}
			return v, nil
		}
		parseFloatPos := func(name string, e *walk.LineEdit) (float64, error) {
			s := strings.TrimSpace(e.Text())
			v, err := strconv.ParseFloat(s, 64)
			if err != nil || v <= 0 {
				return 0, fmt.Errorf("%s must be a number > 0", name)
			}
			return v, nil
		}

		maxTiles, err := parseInt("treemap.maxTiles", maxTilesEdit, 1)
		if err != nil {
			return err
		}
		clumpTh, err := parsePercentOrRatio(strings.TrimSpace(clumpThresholdEdit.Text()))
		if err != nil || clumpTh <= 0 {
			return fmt.Errorf("treemap.clumpThreshold must be a positive ratio or percent (for example 1%% or 0.01)")
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

		parseColor := func(name string, e *walk.LineEdit) (color.RGBA, error) {
			c, err := parseHexColor(e.Text())
			if err != nil {
				return color.RGBA{}, fmt.Errorf("%s: %w", name, err)
			}
			return c, nil
		}
		nFolderBg, err := parseColor("treemap.nativeFolderBgColor", nativeFolderBgEdit)
		if err != nil {
			return err
		}
		nFolderText, err := parseColor("treemap.nativeFolderTextColor", nativeFolderTextEdit)
		if err != nil {
			return err
		}
		pFolderBg, err := parseColor("treemap.packedFolderBgColor", packedFolderBgEdit)
		if err != nil {
			return err
		}
		pFolderText, err := parseColor("treemap.packedFolderTextColor", packedFolderTextEdit)
		if err != nil {
			return err
		}
		nFileBg, err := parseColor("treemap.nativeFileBgColor", nativeFileBgEdit)
		if err != nil {
			return err
		}
		nFileText, err := parseColor("treemap.nativeFileTextColor", nativeFileTextEdit)
		if err != nil {
			return err
		}
		pFileBg, err := parseColor("treemap.packedFileBgColor", packedFileBgEdit)
		if err != nil {
			return err
		}
		pFileText, err := parseColor("treemap.packedFileTextColor", packedFileTextEdit)
		if err != nil {
			return err
		}
		nClumpBg, err := parseColor("treemap.nativeClumpBgColor", nativeClumpBgEdit)
		if err != nil {
			return err
		}
		nClumpText, err := parseColor("treemap.nativeClumpTextColor", nativeClumpTextEdit)
		if err != nil {
			return err
		}
		pClumpBg, err := parseColor("treemap.packedClumpBgColor", packedClumpBgEdit)
		if err != nil {
			return err
		}
		pClumpText, err := parseColor("treemap.packedClumpTextColor", packedClumpTextEdit)
		if err != nil {
			return err
		}

		face := strings.TrimSpace(tileFontEdit.Text())
		if face == "" {
			return fmt.Errorf("treemap.tileFontName must not be empty")
		}
		labelPlaceholder := strings.TrimSpace(labelPlaceholderEdit.Text())
		labelDummy := strings.TrimSpace(labelDummyEdit.Text())
		if labelPlaceholder == "" || labelDummy == "" {
			return fmt.Errorf("treemap.labelPlaceholder and treemap.labelDummy must not be empty")
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
		edited.WinExeFiles = strings.TrimSpace(winExeEdit.Text())
		edited.LinuxExeFiles = strings.TrimSpace(linuxExeEdit.Text())
		edited.MacOSExeFiles = strings.TrimSpace(macExeEdit.Text())
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
		return true
	}

	label := func(s string) Widget { return Label{Text: s} }
	row := func(name string, assign **walk.LineEdit) Widget {
		return Composite{
			Layout: Grid{Columns: 2, MarginsZero: true},
			Children: []Widget{
				label(name),
				LineEdit{AssignTo: assign},
			},
		}
	}

	var icon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		icon = ic
	}

	_, _ = Dialog{
		AssignTo:      &dlg,
		Title:         "Settings",
		Icon:          icon,
		MinSize:       Size{Width: 760, Height: 700},
		Size:          Size{Width: 860, Height: 820},
		DefaultButton: &okBtn,
		Layout:        VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			Label{Text: "Treemap Configuration Parameters"},
			Composite{
				Layout: VBox{MarginsZero: true, Spacing: 6},
				Children: []Widget{
					row("treemap.maxTiles", &maxTilesEdit),
					row("treemap.clumpThreshold (e.g. 1% or 0.01)", &clumpThresholdEdit),
					row("treemap.minTileWidth (pt)", &minTileWidthEdit),
					row("treemap.minTileHeight (pt)", &minTileHeightEdit),
					row("treemap.tilePaddingLeft (pt)", &padLeftEdit),
					row("treemap.tilePaddingTop (pt)", &padTopEdit),
					row("treemap.tilePaddingRight (pt)", &padRightEdit),
					row("treemap.tilePaddingBottom (pt)", &padBottomEdit),
					row("treemap.tileFontName", &tileFontEdit),
					row("treemap.headingMaxFontSize (pt)", &headingMaxEdit),
					row("treemap.headingMinFontSize (pt)", &headingMinEdit),
					row("treemap.headingLineHeight", &headingLineHeightEdit),
					row("treemap.detailsFontSizeRatio", &detailsFontRatioEdit),
					row("treemap.detailsLineHeight", &detailsLineHeightEdit),
					row("treemap.aboveDetailsHeightRatio", &aboveDetailsEdit),
					row("treemap.labelPlaceholder", &labelPlaceholderEdit),
					row("treemap.labelDummy", &labelDummyEdit),
					row("treemap.nativeFolderBgColor", &nativeFolderBgEdit),
					row("treemap.nativeFolderTextColor", &nativeFolderTextEdit),
					row("treemap.packedFolderBgColor", &packedFolderBgEdit),
					row("treemap.packedFolderTextColor", &packedFolderTextEdit),
					row("treemap.nativeFileBgColor", &nativeFileBgEdit),
					row("treemap.nativeFileTextColor", &nativeFileTextEdit),
					row("treemap.packedFileBgColor", &packedFileBgEdit),
					row("treemap.packedFileTextColor", &packedFileTextEdit),
					row("treemap.nativeClumpBgColor", &nativeClumpBgEdit),
					row("treemap.nativeClumpTextColor", &nativeClumpTextEdit),
					row("treemap.packedClumpBgColor", &packedClumpBgEdit),
					row("treemap.packedClumpTextColor", &packedClumpTextEdit),
					row("treemap.win.exeFiles", &winExeEdit),
					row("treemap.linux.exeFiles", &linuxExeEdit),
					row("treemap.macos.exeFiles", &macExeEdit),
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
						AssignTo: &applyBtn,
						Text:     "Apply",
						OnClicked: func() {
							_ = applyBtn // keep AssignTo alive for declarative
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
	}.Run(owner)

	if dlg.Result() != walk.DlgCmdOK {
		return current, false
	}
	return edited, true
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
	return fmt.Sprintf("#%02x%02x%02x", c.R, c.G, c.B)
}
