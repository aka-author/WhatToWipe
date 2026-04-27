//go:build windows

package ui

import (
	"fmt"
	"log"
	"strconv"
	"strings"

	"eraserewrite/win-go/internal/config"
)

func treemapToStates(cfg config.Treemap, schemas []RowSchema) []RowState {
	if cfg.MaxTiles == 0 {
		log.Printf("INFO: settings: config is empty, using defaults")
		cfg = config.DefaultTreemap()
	}
	out := make([]RowState, 0, len(schemas))
	for i := range schemas {
		v, err := treemapValueByKey(&cfg, schemas[i].Key)
		if err != nil {
			v = ""
		}
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
		if states[i].Schema == nil {
			return config.Treemap{}, fmt.Errorf("row %d has no schema", i)
		}
		if err := treemapSetValueByKey(&cfg, states[i].Schema.Key, states[i].PendingValue); err != nil {
			return config.Treemap{}, fmt.Errorf("%s: %w", states[i].Schema.Label, err)
		}
	}
	return cfg, nil
}

func treemapValueByKey(t *config.Treemap, key string) (string, error) {
	if t == nil {
		return "", fmt.Errorf("nil treemap config")
	}
	switch strings.ToLower(strings.TrimSpace(key)) {
	case "treemap.maxtiles":
		return strconv.Itoa(t.MaxTiles), nil
	case "treemap.clumpthreshold":
		return strconv.FormatFloat(t.ClumpThreshold, 'f', -1, 64), nil
	case "treemap.mintilewidth":
		return formatPointsValue(t.MinTileWidthPt), nil
	case "treemap.mintileheight":
		return formatPointsValue(t.MinTileHeightPt), nil
	case "treemap.tilepaddingleft":
		return formatPointsValue(t.TilePaddingLeftPt), nil
	case "treemap.tilepaddingtop":
		return formatPointsValue(t.TilePaddingTopPt), nil
	case "treemap.tilepaddingright":
		return formatPointsValue(t.TilePaddingRightPt), nil
	case "treemap.tilepaddingbottom":
		return formatPointsValue(t.TilePaddingBottomPt), nil
	case "treemap.tilefontname":
		return strings.TrimSpace(t.TileFontName), nil
	case "treemap.headingmaxfontsize":
		return formatPointsValue(t.HeadingMaxFontSizePt), nil
	case "treemap.headingminfontsize":
		return formatPointsValue(t.HeadingMinFontSizePt), nil
	case "treemap.headinglineheight":
		return strconv.FormatFloat(t.HeadingLineHeight, 'f', -1, 64), nil
	case "treemap.detailsfontsizeratio":
		return strconv.FormatFloat(t.DetailsFontSizeRatio, 'f', -1, 64), nil
	case "treemap.detailslineheight":
		return strconv.FormatFloat(t.DetailsLineHeight, 'f', -1, 64), nil
	case "treemap.abovedetailsheightratio":
		return strconv.FormatFloat(t.AboveDetailsRatio, 'f', -1, 64), nil
	case "treemap.labelplaceholder":
		return t.LabelPlaceholder, nil
	case "treemap.labeldummy":
		return t.LabelDummy, nil
	case "treemap.nativefolderbgcolor":
		return formatRGBHex(t.NativeFolderBg), nil
	case "treemap.nativefoldertextcolor":
		return formatRGBHex(t.NativeFolderText), nil
	case "treemap.packedfolderbgcolor":
		return formatRGBHex(t.PackedFolderBg), nil
	case "treemap.packedfoldertextcolor":
		return formatRGBHex(t.PackedFolderText), nil
	case "treemap.nativefilebgcolor":
		return formatRGBHex(t.NativeFileBg), nil
	case "treemap.nativefiletextcolor":
		return formatRGBHex(t.NativeFileText), nil
	case "treemap.packedfilebgcolor":
		return formatRGBHex(t.PackedFileBg), nil
	case "treemap.packedfiletextcolor":
		return formatRGBHex(t.PackedFileText), nil
	case "treemap.nativeclumpbgcolor":
		return formatRGBHex(t.NativeClumpBg), nil
	case "treemap.nativeclumptextcolor":
		return formatRGBHex(t.NativeClumpText), nil
	case "treemap.packedclumpbgcolor":
		return formatRGBHex(t.PackedClumpBg), nil
	case "treemap.packedclumptextcolor":
		return formatRGBHex(t.PackedClumpText), nil
	case "treemap.win.exefiles":
		return strings.TrimSpace(t.WinExeFiles), nil
	case "treemap.linux.exefiles":
		return strings.TrimSpace(t.LinuxExeFiles), nil
	case "treemap.macos.exefiles":
		return strings.TrimSpace(t.MacOSExeFiles), nil
	default:
		return "", fmt.Errorf("unknown treemap key: %s", key)
	}
}

func treemapSetValueByKey(t *config.Treemap, key, s string) error {
	s = strings.TrimSpace(s)
	switch strings.ToLower(strings.TrimSpace(key)) {
	case "treemap.maxtiles":
		v, err := strconv.Atoi(s)
		if err != nil || v < 1 {
			return fmt.Errorf("must be an integer >= 1")
		}
		t.MaxTiles = v
	case "treemap.clumpthreshold":
		v, err := parseFloatValue(s)
		if err != nil || v <= 0 || v > 1 {
			return fmt.Errorf("must be a number > 0 and <= 1")
		}
		t.ClumpThreshold = v
	case "treemap.mintilewidth", "treemap.mintileheight", "treemap.headingmaxfontsize", "treemap.headingminfontsize":
		v, err := parsePointsInputToPt(s, false)
		if err != nil || v < 1 {
			return fmt.Errorf("must be a size like 20pt or 4mm, >= 1pt")
		}
		switch strings.ToLower(key) {
		case "treemap.mintilewidth":
			t.MinTileWidthPt = v
		case "treemap.mintileheight":
			t.MinTileHeightPt = v
		case "treemap.headingmaxfontsize":
			t.HeadingMaxFontSizePt = v
		default:
			t.HeadingMinFontSizePt = v
		}
	case "treemap.tilepaddingleft", "treemap.tilepaddingtop", "treemap.tilepaddingright", "treemap.tilepaddingbottom":
		v, err := parsePointsInputToPt(s, true)
		if err != nil || v < 0 {
			return fmt.Errorf("must be a size like 20pt or 4mm, >= 0pt")
		}
		switch strings.ToLower(key) {
		case "treemap.tilepaddingleft":
			t.TilePaddingLeftPt = v
		case "treemap.tilepaddingtop":
			t.TilePaddingTopPt = v
		case "treemap.tilepaddingright":
			t.TilePaddingRightPt = v
		default:
			t.TilePaddingBottomPt = v
		}
	case "treemap.tilefontname":
		if s == "" {
			return fmt.Errorf("font name must not be empty")
		}
		t.TileFontName = s
	case "treemap.headinglineheight", "treemap.detailsfontsizeratio", "treemap.detailslineheight", "treemap.abovedetailsheightratio":
		v, err := parseFloatValue(s)
		if err != nil || v <= 0 {
			return fmt.Errorf("must be > 0")
		}
		switch strings.ToLower(key) {
		case "treemap.headinglineheight":
			t.HeadingLineHeight = v
		case "treemap.detailsfontsizeratio":
			t.DetailsFontSizeRatio = v
		case "treemap.detailslineheight":
			t.DetailsLineHeight = v
		default:
			t.AboveDetailsRatio = v
		}
	case "treemap.labelplaceholder":
		if s == "" {
			return fmt.Errorf("must not be empty")
		}
		t.LabelPlaceholder = s
	case "treemap.labeldummy":
		if s == "" {
			return fmt.Errorf("must not be empty")
		}
		t.LabelDummy = s
	case "treemap.nativefolderbgcolor", "treemap.nativefoldertextcolor", "treemap.packedfolderbgcolor", "treemap.packedfoldertextcolor",
		"treemap.nativefilebgcolor", "treemap.nativefiletextcolor", "treemap.packedfilebgcolor", "treemap.packedfiletextcolor",
		"treemap.nativeclumpbgcolor", "treemap.nativeclumptextcolor", "treemap.packedclumpbgcolor", "treemap.packedclumptextcolor":
		c, err := parseHexColor(s)
		if err != nil {
			return err
		}
		switch strings.ToLower(key) {
		case "treemap.nativefolderbgcolor":
			t.NativeFolderBg = c
		case "treemap.nativefoldertextcolor":
			t.NativeFolderText = c
		case "treemap.packedfolderbgcolor":
			t.PackedFolderBg = c
		case "treemap.packedfoldertextcolor":
			t.PackedFolderText = c
		case "treemap.nativefilebgcolor":
			t.NativeFileBg = c
		case "treemap.nativefiletextcolor":
			t.NativeFileText = c
		case "treemap.packedfilebgcolor":
			t.PackedFileBg = c
		case "treemap.packedfiletextcolor":
			t.PackedFileText = c
		case "treemap.nativeclumpbgcolor":
			t.NativeClumpBg = c
		case "treemap.nativeclumptextcolor":
			t.NativeClumpText = c
		case "treemap.packedclumpbgcolor":
			t.PackedClumpBg = c
		default:
			t.PackedClumpText = c
		}
	case "treemap.win.exefiles":
		t.WinExeFiles = s
	case "treemap.linux.exefiles":
		t.LinuxExeFiles = s
	case "treemap.macos.exefiles":
		t.MacOSExeFiles = s
	default:
		return fmt.Errorf("unknown key")
	}
	return nil
}
