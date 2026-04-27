//go:build windows

package ui

import (
	"fmt"
	"math"
	"strconv"
	"strings"
)

func isPointUnitKey(key string) bool {
	switch strings.ToLower(strings.TrimSpace(key)) {
	case "treemap.mintilewidth",
		"treemap.mintileheight",
		"treemap.tilepaddingleft",
		"treemap.tilepaddingtop",
		"treemap.tilepaddingright",
		"treemap.tilepaddingbottom",
		"treemap.headingmaxfontsize",
		"treemap.headingminfontsize":
		return true
	default:
		return false
	}
}

func parsePointsInputToPt(s string, allowZero bool) (int, error) {
	raw := strings.ToLower(strings.TrimSpace(s))
	if raw == "" {
		return 0, fmt.Errorf("empty value")
	}
	unit := "pt"
	if strings.HasSuffix(raw, "mm") {
		unit = "mm"
		raw = strings.TrimSpace(strings.TrimSuffix(raw, "mm"))
	} else if strings.HasSuffix(raw, "pt") {
		raw = strings.TrimSpace(strings.TrimSuffix(raw, "pt"))
	}
	v, err := strconv.ParseFloat(raw, 64)
	if err != nil {
		return 0, err
	}
	if !allowZero && v <= 0 {
		return 0, fmt.Errorf("must be > 0")
	}
	if allowZero && v < 0 {
		return 0, fmt.Errorf("must be >= 0")
	}
	if unit == "mm" {
		v = v * 72.0 / 25.4
	}
	return int(math.Round(v)), nil
}

func formatPointsValue(pt int) string {
	return fmt.Sprintf("%dpt", pt)
}

