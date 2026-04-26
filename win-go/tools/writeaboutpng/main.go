// Writes internal/ui/about-bunny.png (placeholder) so //go:embed succeeds.
// Replace that file with your real rabbit artwork; keep the same filename.
package main

import (
	"bytes"
	"image/png"
	"os"
	"path/filepath"

	"eraserewrite/win-go/internal/ui"
)

func main() {
	// aboutBunnyPlaceholder is in package ui РІР‚вЂќ call via exported helper.
	_ = ui.WriteDefaultAboutPNGForEmbed(filepath.Join("..", "..", "internal", "ui", "about-bunny.png"))
}
