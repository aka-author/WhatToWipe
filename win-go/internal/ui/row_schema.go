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
	{Key: "treemap.maxTiles", Label: "Maximum number of tiles", Kind: KindNumeric, Min: 1, Max: 9999, Decimals: 0},
	{Key: "treemap.clumpThreshold", Label: "Clump threshold ratio", Kind: KindNumeric, Min: 0.000001, Max: 1, Decimals: 6},
	{Key: "treemap.minTileWidth", Label: "Minimum tile width", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.minTileHeight", Label: "Minimum tile height", Kind: KindNumeric, Min: 1, Max: 4096, Decimals: 0},
	{Key: "treemap.tilePaddingLeft", Label: "Tile left padding", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingTop", Label: "Tile top padding", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingRight", Label: "Tile right padding", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tilePaddingBottom", Label: "Tile bottom padding", Kind: KindNumeric, Min: 0, Max: 1024, Decimals: 0},
	{Key: "treemap.tileFontName", Label: "Tile font family", Kind: KindDropdown, Options: loadFontOptions()},
	{Key: "treemap.headingMaxFontSize", Label: "Heading maximum font size", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingMinFontSize", Label: "Heading minimum font size", Kind: KindNumeric, Min: 1, Max: 1024, Decimals: 0},
	{Key: "treemap.headingLineHeight", Label: "Heading line height ratio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsFontSizeRatio", Label: "Details font size ratio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.detailsLineHeight", Label: "Details line height ratio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.aboveDetailsHeightRatio", Label: "Above-details height ratio", Kind: KindNumeric, Min: 0.01, Max: 10, Decimals: 3},
	{Key: "treemap.labelPlaceholder", Label: "Placeholder label text", Kind: KindText},
	{Key: "treemap.labelDummy", Label: "Dummy label text", Kind: KindText},
	{Key: "treemap.nativeFolderBgColor", Label: "Native folder background color", Kind: KindColor},
	{Key: "treemap.nativeFolderTextColor", Label: "Native folder text color", Kind: KindColor},
	{Key: "treemap.packedFolderBgColor", Label: "Packed folder background color", Kind: KindColor},
	{Key: "treemap.packedFolderTextColor", Label: "Packed folder text color", Kind: KindColor},
	{Key: "treemap.nativeFileBgColor", Label: "Native file background color", Kind: KindColor},
	{Key: "treemap.nativeFileTextColor", Label: "Native file text color", Kind: KindColor},
	{Key: "treemap.packedFileBgColor", Label: "Packed file background color", Kind: KindColor},
	{Key: "treemap.packedFileTextColor", Label: "Packed file text color", Kind: KindColor},
	{Key: "treemap.nativeClumpBgColor", Label: "Native clump background color", Kind: KindColor},
	{Key: "treemap.nativeClumpTextColor", Label: "Native clump text color", Kind: KindColor},
	{Key: "treemap.packedClumpBgColor", Label: "Packed clump background color", Kind: KindColor},
	{Key: "treemap.packedClumpTextColor", Label: "Packed clump text color", Kind: KindColor},
	{Key: "treemap.win.exeFiles", Label: "Windows executable extensions", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.linux.exeFiles", Label: "Linux executable extensions", Kind: KindText, Regex: exeFilesRE},
	{Key: "treemap.macos.exeFiles", Label: "macOS executable extensions", Kind: KindText, Regex: exeFilesRE},
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
