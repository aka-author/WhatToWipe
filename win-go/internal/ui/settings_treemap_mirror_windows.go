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
	if t == nil {
		return ""
	}
	switch row {
	case 0:
		return strconv.Itoa(t.MaxTiles)
	case 1:
		return strconv.FormatFloat(t.ClumpThreshold*100.0, 'f', -1, 64) + "%"
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
		if err != nil || cl <= 0 {
			return fmt.Errorf("must be positive (for example 1%% or 0.01)")
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
