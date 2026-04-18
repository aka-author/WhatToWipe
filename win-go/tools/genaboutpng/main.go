// Writes assets/art/about-bunny.png (placeholder) so //go:embed in package art succeeds.
// Prefer placing your image at codebase/assets/art/about-bunny.png — build.ps1 copies it before build.
package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
)

func moduleRoot() string {
	d, err := os.Getwd()
	if err != nil {
		return ""
	}
	for {
		if _, err := os.Stat(filepath.Join(d, "go.mod")); err == nil {
			return d
		}
		parent := filepath.Dir(d)
		if parent == d {
			return ""
		}
		d = parent
	}
}

func main() {
	root := moduleRoot()
	if root == "" {
		panic("go.mod not found; run from win-go or a subdirectory")
	}
	out := filepath.Join(root, "assets", "art", "about-bunny.png")
	if err := os.MkdirAll(filepath.Dir(out), 0o755); err != nil {
		panic(err)
	}
	img := placeholderPNG()
	var buf bytes.Buffer
	if err := png.Encode(&buf, img); err != nil {
		panic(err)
	}
	if err := os.WriteFile(out, buf.Bytes(), 0o644); err != nil {
		panic(err)
	}
}

func placeholderPNG() *image.RGBA {
	const sz = 200
	img := image.NewRGBA(image.Rect(0, 0, sz, sz))
	bg := color.RGBA{0xf4, 0xf0, 0xe8, 0xff}
	for y := 0; y < sz; y++ {
		for x := 0; x < sz; x++ {
			img.SetRGBA(x, y, bg)
		}
	}
	ear := color.RGBA{0xe8, 0xa0, 0xb8, 0xff}
	body := color.RGBA{0xc8, 0xb8, 0xa8, 0xff}
	nose := color.RGBA{0x88, 0x70, 0x68, 0xff}
	eye := color.RGBA{0x30, 0x30, 0x30, 0xff}
	fillCircle(img, 72, 58, 26, ear)
	fillCircle(img, 128, 58, 26, ear)
	fillCircle(img, 100, 118, 48, body)
	fillCircle(img, 100, 88, 14, nose)
	fillCircle(img, 82, 82, 5, eye)
	fillCircle(img, 118, 82, 5, eye)
	return img
}

func fillCircle(img *image.RGBA, cx, cy, r int, c color.RGBA) {
	b := img.Bounds()
	for dy := -r; dy <= r; dy++ {
		for dx := -r; dx <= r; dx++ {
			if dx*dx+dy*dy > r*r {
				continue
			}
			x, y := cx+dx, cy+dy
			if image.Pt(x, y).In(b) {
				img.SetRGBA(x, y, c)
			}
		}
	}
}
