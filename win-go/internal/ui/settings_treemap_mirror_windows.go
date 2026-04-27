//go:build windows

package ui

import (
	"fmt"
	"image/color"
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

func (m *treemapGridModel) RowCount() int {
	return len(treemapGridRowLabels)
}

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
	if t == nil || row < 0 || row >= len(treemapSchemas) {
		return ""
	}
	v, err := treemapValueByKey(t, treemapSchemas[row].Key)
	if err != nil {
		return ""
	}
	return v
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
	if row < 0 || row >= len(treemapSchemas) {
		return fmt.Errorf("unknown row")
	}
	return treemapSetValueByKey(t, treemapSchemas[row].Key, s)
}

func validateTreemap(t *config.Treemap) error {
	if t == nil {
		return fmt.Errorf("internal error")
	}
	if t.MaxTiles < 1 {
		return fmt.Errorf("treemap.maxTiles must be an integer >= 1")
	}
	if t.ClumpThreshold <= 0 {
		return fmt.Errorf("treemap.clumpThreshold must be positive")
	}
	if t.MinTileWidthPt < 1 || t.MinTileHeightPt < 1 {
		return fmt.Errorf("treemap min tile width/height must be >= 1")
	}
	if t.TilePaddingLeftPt < 0 || t.TilePaddingTopPt < 0 || t.TilePaddingRightPt < 0 || t.TilePaddingBottomPt < 0 {
		return fmt.Errorf("treemap tile padding must be >= 0")
	}
	if strings.TrimSpace(t.TileFontName) == "" {
		return fmt.Errorf("treemap.tileFontName must not be empty")
	}
	if t.HeadingMaxFontSizePt < 1 || t.HeadingMinFontSizePt < 1 {
		return fmt.Errorf("treemap heading font sizes must be >= 1")
	}
	if t.HeadingMinFontSizePt > t.HeadingMaxFontSizePt {
		return fmt.Errorf("treemap.headingMinFontSize must be <= treemap.headingMaxFontSize")
	}
	if t.HeadingLineHeight <= 0 || t.DetailsFontSizeRatio <= 0 || t.DetailsLineHeight <= 0 || t.AboveDetailsRatio <= 0 {
		return fmt.Errorf("treemap line height and ratio fields must be > 0")
	}
	if strings.TrimSpace(t.LabelPlaceholder) == "" || strings.TrimSpace(t.LabelDummy) == "" {
		return fmt.Errorf("treemap.labelPlaceholder and treemap.labelDummy must not be empty")
	}
	return nil
}

func (m *treemapGridModel) colorForRow(row int) (color.RGBA, bool) {
	if m.edited == nil || row < treemapGridColorRow0 || row >= treemapGridColorRow0+treemapGridColorRowCount {
		return color.RGBA{}, false
	}
	c, err := parseHexColor(colorHexAtRow(m.edited, row))
	if err != nil {
		return color.RGBA{}, false
	}
	return c, true
}

func (m *treemapGridModel) StyleCell(style *walk.CellStyle) {
	if style.Col() != 2 {
		return
	}
	c, ok := m.colorForRow(style.Row())
	if !ok {
		return
	}
	canvas := style.Canvas()
	if canvas == nil {
		return
	}
	b := style.Bounds()
	b.X += 3
	b.Y += 3
	b.Width -= 6
	b.Height -= 6
	if b.Width < 2 || b.Height < 2 {
		return
	}
	br, err := walk.NewSolidColorBrush(walk.RGB(c.R, c.G, c.B))
	if err != nil {
		return
	}
	defer br.Dispose()
	if err := canvas.FillRectangle(br, b); err != nil {
		return
	}
	style.Image = nil
}

func (m *treemapGridModel) refreshAll() {
	if m.RowCount() == 0 {
		return
	}
	m.PublishRowsChanged(0, m.RowCount()-1)
}
