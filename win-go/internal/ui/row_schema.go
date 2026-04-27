//go:build windows

package ui

import (
	"log"
	"regexp"
	"sort"
	"strings"

	"golang.org/x/sys/windows/registry"
)

type EditorKind int

const (
	KindText EditorKind = iota
	KindNumeric
	KindDropdown
	KindColor
)

type RowSchema struct {
	Key         string
	Label       string
	Kind        EditorKind
	Options     []string
	Min         float64
	Max         float64
	Decimals    int
	Regex       *regexp.Regexp
	Placeholder string
}

type RowState struct {
	Schema       *RowSchema
	PendingValue string
	LastGood     string
}

var exeFilesRE = regexp.MustCompile(`^[^#\r\n]*$`)

var treemapSchemas = []RowSchema{
	{Key: "treemap.maxTiles", Label: "treemap.maxTiles", Kind: KindNumeric, Min: 1, Max: 9999, Decimals: 0},
	{Key: "treemap.clumpThreshold", Label: "treemap.clumpThreshold", Kind: KindNumeric, Min: 0.000001, Max: 1, Decimals: 6},
	{Key: "treemap.minTileWidth", Label: "treemap.minTileWidth (pt)", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.minTileHeight", Label: "treemap.minTileHeight (pt)", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.tilePaddingLeft", Label: "treemap.tilePaddingLeft (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingTop", Label: "treemap.tilePaddingTop (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingRight", Label: "treemap.tilePaddingRight (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingBottom", Label: "treemap.tilePaddingBottom (pt)", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tileFontName", Label: "treemap.tileFontName", Kind: KindDropdown, Options: loadFontOptions()},
	{Key: "treemap.headingMaxFontSize", Label: "treemap.headingMaxFontSize (pt)", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingMinFontSize", Label: "treemap.headingMinFontSize (pt)", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingLineHeight", Label: "treemap.headingLineHeight", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsFontSizeRatio", Label: "treemap.detailsFontSizeRatio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsLineHeight", Label: "treemap.detailsLineHeight", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.aboveDetailsHeightRatio", Label: "treemap.aboveDetailsHeightRatio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.labelPlaceholder", Label: "treemap.labelPlaceholder", Kind: KindText},
	{Key: "treemap.labelDummy", Label: "treemap.labelDummy", Kind: KindText},
	{Key: "treemap.nativeFolderBgColor", Label: "treemap.nativeFolderBgColor", Kind: KindColor},
	{Key: "treemap.nativeFolderTextColor", Label: "treemap.nativeFolderTextColor", Kind: KindColor},
	{Key: "treemap.packedFolderBgColor", Label: "treemap.packedFolderBgColor", Kind: KindColor},
	{Key: "treemap.packedFolderTextColor", Label: "treemap.packedFolderTextColor", Kind: KindColor},
	{Key: "treemap.nativeFileBgColor", Label: "treemap.nativeFileBgColor", Kind: KindColor},
	{Key: "treemap.nativeFileTextColor", Label: "treemap.nativeFileTextColor", Kind: KindColor},
	{Key: "treemap.packedFileBgColor", Label: "treemap.packedFileBgColor", Kind: KindColor},
	{Key: "treemap.packedFileTextColor", Label: "treemap.packedFileTextColor", Kind: KindColor},
	{Key: "treemap.nativeClumpBgColor", Label: "treemap.nativeClumpBgColor", Kind: KindColor},
	{Key: "treemap.nativeClumpTextColor", Label: "treemap.nativeClumpTextColor", Kind: KindColor},
	{Key: "treemap.packedClumpBgColor", Label: "treemap.packedClumpBgColor", Kind: KindColor},
	{Key: "treemap.packedClumpTextColor", Label: "treemap.packedClumpTextColor", Kind: KindColor},
	{Key: "treemap.win.exeFiles", Label: "treemap.win.exeFiles", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.linux.exeFiles", Label: "treemap.linux.exeFiles", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.macos.exeFiles", Label: "treemap.macos.exeFiles", Kind: KindText, Regex: exeFilesRE},
}

func loadFontOptions() []string {
	seen := map[string]struct{}{}
	load := func(root registry.Key, path string) error {
		k, err := registry.OpenKey(root, path, registry.READ)
		if err != nil {
			return err
		}
		defer k.Close()
		names, err := k.ReadValueNames(-1)
		if err != nil {
			return err
		}
		for _, n := range names {
			n = normalizeFontDisplayName(n)
			if n != "" {
				seen[n] = struct{}{}
			}
		}
		return nil
	}
	const fontsKey = `SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`
	if err := load(registry.LOCAL_MACHINE, fontsKey); err != nil {
		log.Printf("WARN: could not read HKLM fonts: %v", err)
	}
	if err := load(registry.CURRENT_USER, fontsKey); err != nil {
		log.Printf("WARN: could not read HKCU fonts: %v", err)
	}
	if len(seen) == 0 {
		return fallbackFonts()
	}
	out := make([]string, 0, len(seen))
	for n := range seen {
		out = append(out, n)
	}
	sort.Strings(out)
	return out
}

func fallbackFonts() []string {
	return []string{
		"Arial", "Calibri", "Consolas", "Courier New",
		"Georgia", "Segoe UI", "Tahoma", "Times New Roman", "Verdana",
	}
}

func normalizeFontDisplayName(name string) string {
	name = strings.TrimSpace(name)
	if name == "" {
		return ""
	}
	if idx := strings.Index(name, " ("); idx >= 0 {
		name = strings.TrimSpace(name[:idx])
	}
	for _, suffix := range []string{
		" bold italic",
		" bold",
		" italic",
		" regular",
		" narrow",
		" light",
		" semibold",
		" medium",
		" black",
	} {
		if strings.HasSuffix(strings.ToLower(name), suffix) {
			name = strings.TrimSpace(name[:len(name)-len(suffix)])
		}
	}
	return strings.TrimSpace(name)
}
