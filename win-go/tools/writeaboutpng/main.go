// Writes internal/ui/about-bunny.png (placeholder) so //go:embed succeeds.
// Replace that file with your real rabbit artwork; keep the same filename.
package main

import (
	"bytes"
	"image/png"
	"os"
	"path/filepath"

	"whatrwipe/win-go/internal/ui"
)

func main() {
	// aboutBunnyPlaceholder is in package ui — call via exported helper.
	_ = ui.WriteDefaultAboutPNGForEmbed(filepath.Join("..", "..", "internal", "ui", "about-bunny.png"))
}
