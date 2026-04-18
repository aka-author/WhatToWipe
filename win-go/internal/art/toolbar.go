//go:build windows

// Toolbar glyphs are from Open Iconic (https://github.com/iconic/open-iconic),
// used under the MIT License. Source files: open-iconic/*-3x.png (24×24).

package art

import (
	"bytes"
	_ "embed"
	"image"
	"image/color"
	"image/draw"
	_ "image/png"

	"github.com/lxn/walk"
	xdraw "golang.org/x/image/draw"
)

// ToolbarIconSize matches funcspec (24×24 at 96 DPI).
const ToolbarIconSize = 24

//go:embed open-iconic/folder-3x.png
var folderPNG []byte

//go:embed open-iconic/arrow-circle-top-3x.png
var arrowCircleTopPNG []byte

//go:embed open-iconic/eye-3x.png
var eyePNG []byte

//go:embed open-iconic/media-play-3x.png
var mediaPlayPNG []byte

//go:embed open-iconic/media-stop-3x.png
var mediaStopPNG []byte

func OpenFolderBitmap() (*walk.Bitmap, error) {
	// Warm folder yellow (Explorer-like cue).
	ink := color.RGBA{196, 140, 20, 255}
	return bitmapFromPNG(folderPNG, color.RGBA{255, 248, 225, 255}, &ink)
}

func UpInCircleBitmap() (*walk.Bitmap, error) {
	ink := color.RGBA{0, 110, 200, 255}
	return bitmapFromPNG(arrowCircleTopPNG, color.RGBA{235, 245, 255, 255}, &ink)
}

func EyeBitmap() (*walk.Bitmap, error) {
	ink := color.RGBA{108, 72, 190, 255}
	return bitmapFromPNG(eyePNG, color.RGBA{248, 244, 255, 255}, &ink)
}

func UpdatePlayBitmap() (*walk.Bitmap, error) {
	ink := color.RGBA{16, 150, 72, 255}
	return bitmapFromPNG(mediaPlayPNG, color.RGBA{230, 252, 236, 255}, &ink)
}

func StopSolidBitmap() (*walk.Bitmap, error) {
	ink := color.RGBA{215, 48, 48, 255}
	return bitmapFromPNG(mediaStopPNG, color.RGBA{255, 238, 238, 255}, &ink)
}

func bitmapFromPNG(raw []byte, bg color.RGBA, replaceInk *color.RGBA) (*walk.Bitmap, error) {
	src, _, err := image.Decode(bytes.NewReader(raw))
	if err != nil {
		return nil, err
	}
	b := src.Bounds()
	dst := image.NewRGBA(image.Rect(0, 0, ToolbarIconSize, ToolbarIconSize))
	draw.Draw(dst, dst.Bounds(), &image.Uniform{bg}, image.Point{}, draw.Src)

	if b.Dx() == ToolbarIconSize && b.Dy() == ToolbarIconSize {
		draw.Draw(dst, dst.Bounds(), src, b.Min, draw.Over)
	} else {
		xdraw.CatmullRom.Scale(dst, dst.Bounds(), src, b, draw.Over, nil)
	}

	if replaceInk != nil {
		tintDarkInkSoft(dst, *replaceInk)
	}
	return walk.NewBitmapFromImage(dst)
}

// tintDarkInkSoft maps Open Iconic strokes to a saturated ink with softer AA halos.
func tintDarkInkSoft(img *image.RGBA, ink color.RGBA) {
	const alphaTh = 28
	edge := lightenRGBA(ink, 0.42)
	for y := 0; y < img.Rect.Dy(); y++ {
		for x := 0; x < img.Rect.Dx(); x++ {
			c := img.RGBAAt(x, y)
			if c.A < alphaTh {
				continue
			}
			lum := (299*int(c.R) + 587*int(c.G) + 114*int(c.B)) / 1000
			switch {
			case lum < 72:
				img.SetRGBA(x, y, ink)
			case lum < 135:
				t := float32(135-lum) / float32(135-72)
				img.SetRGBA(x, y, mixRGBA(edge, ink, t))
			default:
				// leave background / highlights
			}
		}
	}
}

func lightenRGBA(c color.RGBA, amt float32) color.RGBA {
	amt = clamp01(amt)
	return color.RGBA{
		R: uint8(float32(c.R) + (255-float32(c.R))*amt),
		G: uint8(float32(c.G) + (255-float32(c.G))*amt),
		B: uint8(float32(c.B) + (255-float32(c.B))*amt),
		A: c.A,
	}
}

func mixRGBA(a, b color.RGBA, t float32) color.RGBA {
	t = clamp01(t)
	return color.RGBA{
		R: uint8(float32(a.R)*(1-t) + float32(b.R)*t),
		G: uint8(float32(a.G)*(1-t) + float32(b.G)*t),
		B: uint8(float32(a.B)*(1-t) + float32(b.B)*t),
		A: uint8(float32(a.A)*(1-t) + float32(b.A)*t),
	}
}

func clamp01(t float32) float32 {
	if t < 0 {
		return 0
	}
	if t > 1 {
		return 1
	}
	return t
}
