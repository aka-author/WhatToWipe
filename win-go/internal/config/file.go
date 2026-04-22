package config

import (
	"bufio"
	"bytes"
	"fmt"
	"image/color"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// ConfigFileName is stored next to the executable (human-readable text, funcspec § Program Setup Configuration).
const ConfigFileName = "WhatToWipe.config.txt"

// ConfigPath returns the absolute path to the configuration file.
func ConfigPath() (string, error) {
	exe, err := os.Executable()
	if err != nil {
		return "", err
	}
	return filepath.Join(filepath.Dir(exe), ConfigFileName), nil
}

// LoadTreemapFromPath loads treemap keys from a given file path (e.g. seedconfig for dist/; not ConfigPath()).
func LoadTreemapFromPath(path string) (Treemap, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return DefaultTreemap(), err
	}
	d := DefaultTreemap()
	applyTreemapLines(&d, data)
	return d, nil
}

// LoadOrInitTreemap reads the config file; if it is missing, writes defaults and returns them.
// Parse errors leave fields at defaults for unrecognized or invalid lines.
func LoadOrInitTreemap() (Treemap, error) {
	path, err := ConfigPath()
	if err != nil {
		return DefaultTreemap(), err
	}
	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			d := DefaultTreemap()
			if werr := SaveTreemap(path, d); werr != nil {
				return d, werr
			}
			return d, nil
		}
		return DefaultTreemap(), err
	}
	d := DefaultTreemap()
	applyTreemapLines(&d, data)
	if !ConfigFileListsScanningUpdateInterval(data) {
		if werr := SaveTreemap(path, d); werr != nil {
			return d, werr
		}
	}
	return d, nil
}

// ConfigFileListsScanningUpdateInterval reports whether a non-comment line assigns scanning.updateInterval.
func ConfigFileListsScanningUpdateInterval(data []byte) bool {
	sc := bufio.NewScanner(bytes.NewReader(data))
	for sc.Scan() {
		s := strings.TrimSpace(strings.ToLower(sc.Text()))
		if s == "" || strings.HasPrefix(s, "#") {
			continue
		}
		if strings.HasPrefix(s, "scanning.updateinterval") && strings.Contains(s, "=") {
			return true
		}
	}
	return false
}

// SaveTreemap writes treemap parameters plus scanning.updateInterval (key = value lines only; no generated comments).
func SaveTreemap(path string, t Treemap) error {
	var b strings.Builder
	w := func(key, val string) { fmt.Fprintf(&b, "%s = %s\n", key, val) }
	w("treemap.maxTiles", strconv.Itoa(nonzeroOr(t.MaxTiles, 25)))
	w("treemap.minTileWidth", fmtPx(nonzeroOr(t.MinTileWidthPx, 16)))
	w("treemap.minTileHeight", fmtPx(nonzeroOr(t.MinTileHeightPx, 16)))
	w("treemap.nativeFolderBgColor", formatHex(t.NativeFolderBg))
	w("treemap.nativeFolderTextColor", formatHex(t.NativeFolderText))
	w("treemap.packedFolderBgColor", formatHex(t.PackedFolderBg))
	w("treemap.packedFolderTextColor", formatHex(t.PackedFolderText))
	w("treemap.nativeFileBgColor", formatHex(t.NativeFileBg))
	w("treemap.nativeFileTextColor", formatHex(t.NativeFileText))
	w("treemap.packedFileBgColor", formatHex(t.PackedFileBg))
	w("treemap.packedFileTextColor", formatHex(t.PackedFileText))
	w("treemap.nativeClumpBgColor", formatHex(t.NativeClumpBg))
	w("treemap.nativeClumpTextColor", formatHex(t.NativeClumpText))
	w("treemap.packedClumpBgColor", formatHex(t.PackedClumpBg))
	w("treemap.packedClumpTextColor", formatHex(t.PackedClumpText))
	face := t.TileFontName
	if face == "" {
		face = "Segoe UI"
	}
	w("treemap.tileFontName", face)
	w("treemap.tileFontSizeLarge", fmtPt(nonzeroOr(t.TileFontSizeLargePt, 18)))
	w("treemap.tileFontSizeMedium", fmtPt(nonzeroOr(t.TileFontSizeMediumPt, 14)))
	w("treemap.tileFontSizeSmall", fmtPt(nonzeroOr(t.TileFontSizeSmallPt, 10)))
	w("treemap.beforeSize", fmtPt(nonzeroOr(t.BeforeSizePt, 10)))
	w("treemap.beforeShare", fmtPt(nonzeroOr(t.BeforeSharePt, 5)))
	fmt.Fprintf(&b, "\nscanning.updateInterval = %s\n", ScanPathUpdateIntervalFileValue)
	return os.WriteFile(path, []byte(b.String()), 0o644)
}

func nonzeroOr(v, def int) int {
	if v == 0 {
		return def
	}
	return v
}

func fmtPt(pt int) string { return fmt.Sprintf("%d pt", pt) }
func fmtPx(px int) string { return fmt.Sprintf("%d px", px) }

func formatHex(c color.RGBA) string {
	return fmt.Sprintf("#%02x%02x%02x", c.R, c.G, c.B)
}

func applyTreemapLines(d *Treemap, data []byte) {
	sc := bufio.NewScanner(bytes.NewReader(data))
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		key, val, ok := strings.Cut(line, "=")
		if !ok {
			continue
		}
		key = strings.TrimSpace(strings.ToLower(key))
		val = strings.TrimSpace(val)
		if key == "" {
			continue
		}
		switch key {
		case "treemap.maxtiles":
			if n, err := strconv.Atoi(val); err == nil && n > 0 {
				d.MaxTiles = n
			}
		case "treemap.mintilewidth":
			if n, ok := parsePx(val); ok {
				d.MinTileWidthPx = n
			}
		case "treemap.mintileheight":
			if n, ok := parsePx(val); ok {
				d.MinTileHeightPx = n
			}
		case "treemap.nativefolderbgcolor":
			d.NativeFolderBg = hexRGBA(val)
		case "treemap.nativefoldertextcolor":
			d.NativeFolderText = hexRGBA(val)
		case "treemap.packedfolderbgcolor":
			d.PackedFolderBg = hexRGBA(val)
		case "treemap.packedfoldertextcolor":
			d.PackedFolderText = hexRGBA(val)
		case "treemap.nativefilebgcolor":
			d.NativeFileBg = hexRGBA(val)
		case "treemap.nativefiletextcolor":
			d.NativeFileText = hexRGBA(val)
		case "treemap.packedfilebgcolor":
			d.PackedFileBg = hexRGBA(val)
		case "treemap.packedfiletextcolor":
			d.PackedFileText = hexRGBA(val)
		case "treemap.nativeclumpbgcolor":
			d.NativeClumpBg = hexRGBA(val)
		case "treemap.nativeclumptextcolor":
			d.NativeClumpText = hexRGBA(val)
		case "treemap.packedclumpbgcolor":
			d.PackedClumpBg = hexRGBA(val)
		case "treemap.packedclumptextcolor":
			d.PackedClumpText = hexRGBA(val)
		case "treemap.tilefontname":
			d.TileFontName = val
		case "treemap.tilefontsizelarge":
			if n, ok := parsePt(val); ok {
				d.TileFontSizeLargePt = n
			}
		case "treemap.tilefontsizemedium":
			if n, ok := parsePt(val); ok {
				d.TileFontSizeMediumPt = n
			}
		case "treemap.tilefontsizesmall":
			if n, ok := parsePt(val); ok {
				d.TileFontSizeSmallPt = n
			}
		case "treemap.beforesize":
			if n, ok := parsePt(val); ok {
				d.BeforeSizePt = n
			}
		case "treemap.beforeshare":
			if n, ok := parsePt(val); ok {
				d.BeforeSharePt = n
			}
		}
	}
}

func parsePt(s string) (int, bool) {
	s = strings.TrimSpace(strings.ToLower(s))
	s = strings.TrimSuffix(s, "pt")
	s = strings.TrimSpace(s)
	n, err := strconv.Atoi(s)
	if err != nil || n <= 0 {
		return 0, false
	}
	return n, true
}

func parsePx(s string) (int, bool) {
	s = strings.TrimSpace(strings.ToLower(s))
	s = strings.TrimSuffix(s, "px")
	s = strings.TrimSpace(s)
	n, err := strconv.Atoi(s)
	if err != nil || n <= 0 {
		return 0, false
	}
	return n, true
}
