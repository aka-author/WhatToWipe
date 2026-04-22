package config

import (
	"fmt"
	"image/color"
	"strings"
)

// Treemap holds treemap-related defaults (funcspec § Treemap Configuration Parameters).
type Treemap struct {
	MaxTiles int
	MinTileWidthPx  int
	MinTileHeightPx int

	NativeFolderBg, NativeFolderText   color.RGBA
	PackedFolderBg, PackedFolderText color.RGBA
	NativeFileBg, NativeFileText       color.RGBA
	PackedFileBg, PackedFileText     color.RGBA
	NativeClumpBg, NativeClumpText   color.RGBA
	PackedClumpBg, PackedClumpText   color.RGBA

	TileFontName string

	TileFontSizeLargePt  int
	TileFontSizeMediumPt int
	TileFontSizeSmallPt  int
	BeforeSizePt         int
	BeforeSharePt        int
}

func DefaultTreemap() Treemap {
	return Treemap{
		MaxTiles:        25,
		MinTileWidthPx:  16,
		MinTileHeightPx: 16,

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

		TileFontSizeLargePt:  18,
		TileFontSizeMediumPt: 14,
		TileFontSizeSmallPt:  10,
		BeforeSizePt:         10,
		BeforeSharePt:        5,
	}
}

func hexRGBA(s string) color.RGBA {
	s = strings.TrimSpace(strings.TrimPrefix(strings.TrimSpace(s), "#"))
	if len(s) != 6 {
		return color.RGBA{A: 255}
	}
	var r, g, b uint8
	_, err := fmt.Sscanf(s, "%02x%02x%02x", &r, &g, &b)
	if err != nil {
		return color.RGBA{A: 255}
	}
	return color.RGBA{R: r, G: g, B: b, A: 255}
}
