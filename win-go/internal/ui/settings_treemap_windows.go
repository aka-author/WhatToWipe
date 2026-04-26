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

func chooseColorForRow(owner walk.Form, edited *config.Treemap, row int, grid *treemapGridModel) {
	start, err := parseHexColor(colorHexAtRow(edited, row))
	if err != nil {
		start = color.RGBA{A: 255}
	}
	picked, ok := pickColor(owner, start)
	if !ok {
		return
	}
	setTreemapColorByRow(edited, row, picked)
	grid.refreshAll()
}

func showTreemapSettingsDialog(owner walk.Form, current config.Treemap, onApply func(config.Treemap)) {
	edited := current
	def := config.DefaultTreemap()

	var dlg *walk.Dialog
	var okBtn *walk.PushButton
	var gridTV *walk.TableView

	gridModel := &treemapGridModel{edited: &edited}

	setFields := func(t config.Treemap) {
		edited = t
		gridModel.edited = &edited
		gridModel.refreshAll()
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
			TableView{
				AssignTo:                 &gridTV,
				StretchFactor:            1,
				AlternatingRowBG:         true,
				LastColumnStretched:      true,
				NotSortableByHeaderClick: true,
				MinSize:                  Size{Height: 320},
				Columns: []TableViewColumn{
					{Title: "Parameter", Width: 220},
					{Title: "Value", Width: 320},
					{Title: "Sample", Width: 56},
					{Title: "", Width: 44},
				},
				Model: gridModel,
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

	gridTV.MouseDown().Attach(func(_, _ int, button walk.MouseButton) {
		if button != walk.LeftButton {
			return
		}
		row := gridTV.CurrentIndex()
		if row < treemapGridColorRow0 || row >= treemapGridColorRow0+treemapGridColorRowCount {
			return
		}
		chooseColorForRow(dlg, &edited, row, gridModel)
	})

	if gridTV != nil {
		gridTV.SetGridlines(true)
		gridTV.SetAlternatingRowBG(true)
	}
	setFields(current)
	_ = dlg.Run()
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
