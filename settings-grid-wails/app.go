package main

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const configFileName = "Erase & Rewrite.config.txt"

type GridRow struct {
	Key      string   `json:"key"`
	Label    string   `json:"label"`
	Type     string   `json:"type"`
	Value    string   `json:"value"`
	Options  []string `json:"options,omitempty"`
	Required bool     `json:"required"`
}

type SettingsApp struct{}

func NewSettingsApp() *SettingsApp { return &SettingsApp{} }

func (a *SettingsApp) LoadRows() ([]GridRow, error) {
	cfg, err := loadConfigMap()
	if err != nil {
		return nil, err
	}
	return rowsFromMap(cfg), nil
}

func (a *SettingsApp) ResetRowsToDefaults() []GridRow {
	return rowsFromMap(defaultMap())
}

func (a *SettingsApp) ApplyRows(rows []GridRow) error {
	m := defaultMap()
	for _, r := range rows {
		m[r.Key] = strings.TrimSpace(r.Value)
	}
	if err := validateMap(m); err != nil {
		return err
	}
	return saveConfigMap(m)
}

func configPath() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(base, "Erase & Rewrite", configFileName), nil
}

func defaultMap() map[string]string {
	return map[string]string{
		"treemap.maxTiles":                 "20",
		"treemap.clumpThreshold":           "1%",
		"treemap.minTileWidth":             "50pt",
		"treemap.minTileHeight":            "50pt",
		"treemap.tilePaddingLeft":          "5pt",
		"treemap.tilePaddingTop":           "5pt",
		"treemap.tilePaddingRight":         "5pt",
		"treemap.tilePaddingBottom":        "5pt",
		"treemap.nativeFolderBgColor":      "#80ef80",
		"treemap.nativeFolderTextColor":    "#000000",
		"treemap.packedFolderBgColor":      "#06402b",
		"treemap.packedFolderTextColor":    "#ffffff",
		"treemap.nativeFileBgColor":        "#ffb09c",
		"treemap.nativeFileTextColor":      "#000000",
		"treemap.packedFileBgColor":        "#900000",
		"treemap.packedFileTextColor":      "#ffffff",
		"treemap.nativeClumpBgColor":       "#aaaaaa",
		"treemap.nativeClumpTextColor":     "#000000",
		"treemap.packedClumpBgColor":       "#323232",
		"treemap.packedClumpTextColor":     "#ffffff",
		"treemap.tileFontName":             "Segoe UI",
		"treemap.headingMaxFontSize":       "30pt",
		"treemap.headingMinFontSize":       "8pt",
		"treemap.headingLineHeight":        "1.2",
		"treemap.detailsFontSizeRatio":     "0.8",
		"treemap.detailsLineHeight":        "1.5",
		"treemap.aboveDetailsHeightRatio":  "1.0",
		"treemap.labelPlaceholder":         "...",
		"treemap.labelDummy":               "...",
		"treemap.win.exeFiles":             "bat, com, exe, dll, scr, msi",
		"treemap.linux.exeFiles":           "",
		"treemap.macos.exeFiles":           "",
		"scanning.updateInterval":          "0.5 sec",
	}
}

func rowsFromMap(m map[string]string) []GridRow {
	get := func(k string) string { return strings.TrimSpace(m[k]) }
	return []GridRow{
		{Key: "treemap.maxTiles", Label: "treemap.maxTiles", Type: "number", Value: get("treemap.maxTiles"), Required: true},
		{Key: "treemap.clumpThreshold", Label: "treemap.clumpThreshold", Type: "text", Value: get("treemap.clumpThreshold"), Required: true},
		{Key: "treemap.minTileWidth", Label: "treemap.minTileWidth", Type: "number-pt", Value: get("treemap.minTileWidth"), Required: true},
		{Key: "treemap.minTileHeight", Label: "treemap.minTileHeight", Type: "number-pt", Value: get("treemap.minTileHeight"), Required: true},
		{Key: "treemap.tilePaddingLeft", Label: "treemap.tilePaddingLeft", Type: "number-pt", Value: get("treemap.tilePaddingLeft"), Required: true},
		{Key: "treemap.tilePaddingTop", Label: "treemap.tilePaddingTop", Type: "number-pt", Value: get("treemap.tilePaddingTop"), Required: true},
		{Key: "treemap.tilePaddingRight", Label: "treemap.tilePaddingRight", Type: "number-pt", Value: get("treemap.tilePaddingRight"), Required: true},
		{Key: "treemap.tilePaddingBottom", Label: "treemap.tilePaddingBottom", Type: "number-pt", Value: get("treemap.tilePaddingBottom"), Required: true},
		{Key: "treemap.tileFontName", Label: "treemap.tileFontName", Type: "text", Value: get("treemap.tileFontName"), Required: true},
		{Key: "treemap.headingMaxFontSize", Label: "treemap.headingMaxFontSize", Type: "number-pt", Value: get("treemap.headingMaxFontSize"), Required: true},
		{Key: "treemap.headingMinFontSize", Label: "treemap.headingMinFontSize", Type: "number-pt", Value: get("treemap.headingMinFontSize"), Required: true},
		{Key: "treemap.headingLineHeight", Label: "treemap.headingLineHeight", Type: "number", Value: get("treemap.headingLineHeight"), Required: true},
		{Key: "treemap.detailsFontSizeRatio", Label: "treemap.detailsFontSizeRatio", Type: "number", Value: get("treemap.detailsFontSizeRatio"), Required: true},
		{Key: "treemap.detailsLineHeight", Label: "treemap.detailsLineHeight", Type: "number", Value: get("treemap.detailsLineHeight"), Required: true},
		{Key: "treemap.aboveDetailsHeightRatio", Label: "treemap.aboveDetailsHeightRatio", Type: "number", Value: get("treemap.aboveDetailsHeightRatio"), Required: true},
		{Key: "treemap.labelPlaceholder", Label: "treemap.labelPlaceholder", Type: "text", Value: get("treemap.labelPlaceholder"), Required: true},
		{Key: "treemap.labelDummy", Label: "treemap.labelDummy", Type: "text", Value: get("treemap.labelDummy"), Required: true},
		{Key: "treemap.nativeFolderBgColor", Label: "treemap.nativeFolderBgColor", Type: "color", Value: get("treemap.nativeFolderBgColor"), Required: true},
		{Key: "treemap.nativeFolderTextColor", Label: "treemap.nativeFolderTextColor", Type: "color", Value: get("treemap.nativeFolderTextColor"), Required: true},
		{Key: "treemap.packedFolderBgColor", Label: "treemap.packedFolderBgColor", Type: "color", Value: get("treemap.packedFolderBgColor"), Required: true},
		{Key: "treemap.packedFolderTextColor", Label: "treemap.packedFolderTextColor", Type: "color", Value: get("treemap.packedFolderTextColor"), Required: true},
		{Key: "treemap.nativeFileBgColor", Label: "treemap.nativeFileBgColor", Type: "color", Value: get("treemap.nativeFileBgColor"), Required: true},
		{Key: "treemap.nativeFileTextColor", Label: "treemap.nativeFileTextColor", Type: "color", Value: get("treemap.nativeFileTextColor"), Required: true},
		{Key: "treemap.packedFileBgColor", Label: "treemap.packedFileBgColor", Type: "color", Value: get("treemap.packedFileBgColor"), Required: true},
		{Key: "treemap.packedFileTextColor", Label: "treemap.packedFileTextColor", Type: "color", Value: get("treemap.packedFileTextColor"), Required: true},
		{Key: "treemap.nativeClumpBgColor", Label: "treemap.nativeClumpBgColor", Type: "color", Value: get("treemap.nativeClumpBgColor"), Required: true},
		{Key: "treemap.nativeClumpTextColor", Label: "treemap.nativeClumpTextColor", Type: "color", Value: get("treemap.nativeClumpTextColor"), Required: true},
		{Key: "treemap.packedClumpBgColor", Label: "treemap.packedClumpBgColor", Type: "color", Value: get("treemap.packedClumpBgColor"), Required: true},
		{Key: "treemap.packedClumpTextColor", Label: "treemap.packedClumpTextColor", Type: "color", Value: get("treemap.packedClumpTextColor"), Required: true},
		{Key: "treemap.win.exeFiles", Label: "treemap.win.exeFiles", Type: "text", Value: get("treemap.win.exeFiles"), Required: false},
		{Key: "treemap.linux.exeFiles", Label: "treemap.linux.exeFiles", Type: "text", Value: get("treemap.linux.exeFiles"), Required: false},
		{Key: "treemap.macos.exeFiles", Label: "treemap.macos.exeFiles", Type: "text", Value: get("treemap.macos.exeFiles"), Required: false},
	}
}

func loadConfigMap() (map[string]string, error) {
	path, err := configPath()
	if err != nil {
		return nil, err
	}
	m := defaultMap()
	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return m, nil
		}
		return nil, err
	}
	sc := bufio.NewScanner(bytes.NewReader(data))
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		k, v, ok := strings.Cut(line, "=")
		if !ok {
			continue
		}
		k = strings.TrimSpace(k)
		v = strings.TrimSpace(v)
		if k != "" {
			m[k] = v
		}
	}
	return m, nil
}

func saveConfigMap(m map[string]string) error {
	path, err := configPath()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	order := rowsFromMap(m)
	var b strings.Builder
	for _, r := range order {
		fmt.Fprintf(&b, "%s = %s\n", r.Key, strings.TrimSpace(m[r.Key]))
	}
	fmt.Fprintf(&b, "\nscanning.updateInterval = %s\n", strings.TrimSpace(m["scanning.updateInterval"]))
	return os.WriteFile(path, []byte(b.String()), 0o644)
}

func validateMap(m map[string]string) error {
	needIntPos := []string{"treemap.maxTiles"}
	for _, k := range needIntPos {
		n, err := strconv.Atoi(strings.TrimSpace(m[k]))
		if err != nil || n < 1 {
			return fmt.Errorf("%s must be integer >= 1", k)
		}
	}
	needPtPos := []string{
		"treemap.minTileWidth", "treemap.minTileHeight",
		"treemap.headingMaxFontSize", "treemap.headingMinFontSize",
	}
	for _, k := range needPtPos {
		if !validPt(strings.TrimSpace(m[k]), true) {
			return fmt.Errorf("%s must be positive pt value", k)
		}
	}
	needPtNonNeg := []string{
		"treemap.tilePaddingLeft", "treemap.tilePaddingTop", "treemap.tilePaddingRight", "treemap.tilePaddingBottom",
	}
	for _, k := range needPtNonNeg {
		if !validPt(strings.TrimSpace(m[k]), false) {
			return fmt.Errorf("%s must be non-negative pt value", k)
		}
	}
	for _, k := range []string{
		"treemap.headingLineHeight", "treemap.detailsFontSizeRatio", "treemap.detailsLineHeight", "treemap.aboveDetailsHeightRatio",
	} {
		f, err := strconv.ParseFloat(strings.TrimSpace(m[k]), 64)
		if err != nil || f <= 0 {
			return fmt.Errorf("%s must be > 0", k)
		}
	}
	for _, k := range []string{"treemap.labelPlaceholder", "treemap.labelDummy", "treemap.tileFontName"} {
		if strings.TrimSpace(m[k]) == "" {
			return fmt.Errorf("%s must not be empty", k)
		}
	}
	for _, k := range []string{
		"treemap.nativeFolderBgColor", "treemap.nativeFolderTextColor", "treemap.packedFolderBgColor", "treemap.packedFolderTextColor",
		"treemap.nativeFileBgColor", "treemap.nativeFileTextColor", "treemap.packedFileBgColor", "treemap.packedFileTextColor",
		"treemap.nativeClumpBgColor", "treemap.nativeClumpTextColor", "treemap.packedClumpBgColor", "treemap.packedClumpTextColor",
	} {
		if !validHex(strings.TrimSpace(m[k])) {
			return fmt.Errorf("%s must be #RRGGBB", k)
		}
	}
	return nil
}

func validPt(s string, strictPositive bool) bool {
	s = strings.TrimSpace(strings.ToLower(s))
	s = strings.TrimSuffix(s, "pt")
	n, err := strconv.Atoi(strings.TrimSpace(s))
	if err != nil {
		return false
	}
	if strictPositive {
		return n > 0
	}
	return n >= 0
}

func validHex(s string) bool {
	if len(s) != 7 || s[0] != '#' {
		return false
	}
	for i := 1; i < len(s); i++ {
		c := s[i]
		ok := (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
		if !ok {
			return false
		}
	}
	return true
}
