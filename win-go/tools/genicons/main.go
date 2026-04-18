// genicons writes icons/app.ico (PE / Explorer) and the same bytes to internal/ui/icons/app.ico
// so the main window can embed the identical icon.
package main

import (
	"bytes"
	"encoding/binary"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"os"
	"path/filepath"
)

func main() {
	// Run from module root: go run ./tools/genicons
	appDir := filepath.Join("icons")
	winDir := filepath.Join("internal", "ui", "icons")
	if err := os.MkdirAll(appDir, 0o755); err != nil {
		panic(err)
	}
	if err := os.MkdirAll(winDir, 0o755); err != nil {
		panic(err)
	}
	data := mustEncodeICO(appExeIcon)
	if err := os.WriteFile(filepath.Join(appDir, "app.ico"), data, 0o644); err != nil {
		panic(err)
	}
	if err := os.WriteFile(filepath.Join(winDir, "app.ico"), data, 0o644); err != nil {
		panic(err)
	}
}

func mustEncodeICO(drawFn func(int) image.Image) []byte {
	// Multiple embedded PNGs so Windows can pick crisp sizes for title bar, taskbar, and Alt+Tab.
	sizes := []int{256, 64, 48, 32, 16}
	var pngs [][]byte
	for _, sz := range sizes {
		var buf bytes.Buffer
		if err := png.Encode(&buf, drawFn(sz)); err != nil {
			panic(err)
		}
		pngs = append(pngs, buf.Bytes())
	}
	data, err := encodePNGBasedICO(pngs, sizes)
	if err != nil {
		panic(err)
	}
	return data
}

// appExeIcon: treemap-style tiles (Explorer / .exe).
func appExeIcon(size int) image.Image {
	img := image.NewRGBA(image.Rect(0, 0, size, size))
	bg := color.RGBA{0xf4, 0xf6, 0xf9, 0xff}
	draw.Draw(img, img.Bounds(), &image.Uniform{bg}, image.Point{}, draw.Src)
	margin := size / 14
	if margin < 1 {
		margin = 1
	}
	inner := image.Rect(margin, margin, size-margin, size-margin)
	a := color.RGBA{0x3d, 0x8b, 0xd4, 0xff}
	b := color.RGBA{0xe6, 0x7e, 0x22, 0xff}
	c := color.RGBA{0x27, 0xae, 0x60, 0xff}
	border := color.RGBA{0x2c, 0x3e, 0x50, 0xff}
	w := inner.Dx()
	split := w * 5 / 8
	left := image.Rect(inner.Min.X, inner.Min.Y, inner.Min.X+split, inner.Max.Y)
	right := image.Rect(inner.Min.X+split, inner.Min.Y, inner.Max.X, inner.Max.Y)
	midY := left.Min.Y + left.Dy()/2
	top := image.Rect(left.Min.X, left.Min.Y, left.Max.X, midY)
	bot := image.Rect(left.Min.X, midY, left.Max.X, left.Max.Y)
	fillTile(img, top, a, border)
	fillTile(img, bot, b, border)
	fillTile(img, right, c, border)
	return img
}

func fillTile(img *image.RGBA, r image.Rectangle, fill, stroke color.RGBA) {
	r = r.Inset(1)
	if r.Dx() < 1 || r.Dy() < 1 {
		return
	}
	draw.Draw(img, r, &image.Uniform{fill}, image.Point{}, draw.Src)
	for x := r.Min.X; x < r.Max.X; x++ {
		img.SetRGBA(x, r.Min.Y, stroke)
		img.SetRGBA(x, r.Max.Y-1, stroke)
	}
	for y := r.Min.Y; y < r.Max.Y; y++ {
		img.SetRGBA(r.Min.X, y, stroke)
		img.SetRGBA(r.Max.X-1, y, stroke)
	}
}

func encodePNGBasedICO(pngs [][]byte, pixelSizes []int) ([]byte, error) {
	n := len(pngs)
	header := 6 + n*16
	var buf bytes.Buffer
	_ = binary.Write(&buf, binary.LittleEndian, uint16(0))
	_ = binary.Write(&buf, binary.LittleEndian, uint16(1))
	_ = binary.Write(&buf, binary.LittleEndian, uint16(n))
	offset := uint32(header)
	for i := 0; i < n; i++ {
		w := pixelSizes[i]
		bw := byte(w)
		if w >= 256 {
			bw = 0
		}
		bh := bw
		_ = buf.WriteByte(bw)
		_ = buf.WriteByte(bh)
		_ = buf.WriteByte(0)
		_ = buf.WriteByte(0)
		_ = binary.Write(&buf, binary.LittleEndian, uint16(1))
		_ = binary.Write(&buf, binary.LittleEndian, uint16(32))
		sz := uint32(len(pngs[i]))
		_ = binary.Write(&buf, binary.LittleEndian, sz)
		_ = binary.Write(&buf, binary.LittleEndian, offset)
		offset += sz
	}
	for i := 0; i < n; i++ {
		_, _ = buf.Write(pngs[i])
	}
	return buf.Bytes(), nil
}
