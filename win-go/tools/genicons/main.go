// genicons writes icons/app.ico (PE / Explorer) and the same bytes to internal/ui/icons/app.ico
// from codebase/assets/art/broombunny.png.
package main

import (
	"bytes"
	"encoding/binary"
	"image"
	"image/png"
	_ "image/png"
	"os"
	"path/filepath"

	xdraw "golang.org/x/image/draw"
)

func main() {
	appDir := filepath.Join("icons")
	winDir := filepath.Join("internal", "ui", "icons")
	if err := os.MkdirAll(appDir, 0o755); err != nil {
		panic(err)
	}
	if err := os.MkdirAll(winDir, 0o755); err != nil {
		panic(err)
	}
	data := mustEncodeICO(iconFromSource)
	if err := os.WriteFile(filepath.Join(appDir, "app.ico"), data, 0o644); err != nil {
		panic(err)
	}
	if err := os.WriteFile(filepath.Join(winDir, "app.ico"), data, 0o644); err != nil {
		panic(err)
	}
}

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

func sourceIconPath() string {
	root := moduleRoot()
	if root == "" {
		panic("go.mod not found; run from win-go or a subdirectory")
	}
	return filepath.Join(root, "..", "assets", "art", "broombunny.png")
}

func loadSourceIcon() image.Image {
	path := sourceIconPath()
	f, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	defer f.Close()
	img, _, err := image.Decode(f)
	if err != nil {
		panic(err)
	}
	return img
}

func iconFromSource(size int) image.Image {
	src := loadSourceIcon()
	dst := image.NewRGBA(image.Rect(0, 0, size, size))
	xdraw.CatmullRom.Scale(dst, dst.Bounds(), src, src.Bounds(), xdraw.Over, nil)
	return dst
}

func mustEncodeICO(drawFn func(int) image.Image) []byte {
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
