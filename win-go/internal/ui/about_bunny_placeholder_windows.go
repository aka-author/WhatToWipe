//go:build windows

package ui

import (
	"image"
	"image/color"
)

// aboutBunnyPlaceholder draws a simple rabbit silhouette when art/about-bunny.png
// is not found next to the exe or repo (same colorspace as a small illustration).
func aboutBunnyPlaceholder() image.Image {
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
