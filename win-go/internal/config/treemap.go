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

	// Host-OS executable file type lists (comma-separated tokens, with or without leading dot).
	// Used for treemap interaction; see funcspec Exploring a File and Treemap Configuration Parameters.
	WinExeFiles   string
	LinuxExeFiles string
	MacOSExeFiles string
}

func DefaultTreemap() Treemap {
	return Treemap{
		MaxTiles:            20,
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

		WinExeFiles: "com, exe, dll, scr, msi",
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

// ExeTypeList returns normalized file extensions (lowercase, with a leading dot) used to treat
// a file as executable for treemap rules, for the host OS (funcspec Exploring a File).
func ExeTypeList(cfg Treemap) []string {
	var raw string
	switch runtime.GOOS {
	case "windows":
		raw = cfg.WinExeFiles
	case "darwin":
		raw = cfg.MacOSExeFiles
	default:
		raw = cfg.LinuxExeFiles
	}
	return normalizeExeTokens(raw)
}

func normalizeExeTokens(csv string) []string {
	csv = strings.TrimSpace(csv)
	if csv == "" {
		return []string{".com", ".exe", ".dll", ".scr", ".msi"}
	}
	var out []string
	for _, tok := range strings.Split(csv, ",") {
		t := strings.TrimSpace(strings.ToLower(tok))
		if t == "" {
			continue
		}
		if !strings.HasPrefix(t, ".") {
			t = "." + t
		}
		out = append(out, t)
	}
	if len(out) == 0 {
		return []string{".com", ".exe", ".dll", ".scr", ".msi"}
	}
	return out
}

// FileMatchesExeList reports whether path's extension is one of exts (from ExeTypeList).
func FileMatchesExeList(path string, exts []string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	if ext == "" {
		return false
	}
	for _, e := range exts {
		if ext == e {
			return true
		}
	}
	return false
}
