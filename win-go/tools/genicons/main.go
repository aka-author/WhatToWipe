// genicons writes icons/app.ico (PE / Explorer) and internal/ui/icons/app.ico
// from size-specific art in codebase/assets/art/broombunny*.png.
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

type iconLayer struct {
	size   int
	file   string
	smooth bool
}

// Each ICO size uses the art file drawn for that display role (not a single downscale).
var iconLayers = []iconLayer{
	{256, "broombunny.png", true},
	{64, "broombunny-small.png", true},
	{48, "broombunny-small.png", true},
	{32, "broombunny-32x32.png", false},
	{16, "broombunny-16x16.png", false},
}

func main() {
	appDir := filepath.Join("icons")
	winDir := filepath.Join("internal", "ui", "icons")
	if err := os.MkdirAll(appDir, 0o755); err != nil {
		panic(err)
	}
	if err := os.MkdirAll(winDir, 0o755); err != nil {
		panic(err)
	}
	data := mustEncodeICO()
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

func artDir() string {
	root := moduleRoot()
	if root == "" {
		panic("go.mod not found; run from win-go or a subdirectory")
	}
	return filepath.Join(root, "..", "assets", "art")
}

func loadImage(name string) image.Image {
	path := filepath.Join(artDir(), name)
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

func renderLayer(layer iconLayer) image.Image {
	src := loadImage(layer.file)
	dst := image.NewRGBA(image.Rect(0, 0, layer.size, layer.size))
	scaler := xdraw.NearestNeighbor
	if layer.smooth {
		scaler = xdraw.CatmullRom
	}
	scaler.Scale(dst, dst.Bounds(), src, src.Bounds(), xdraw.Over, nil)
	return dst
}

func mustEncodeICO() []byte {
	sizes := make([]int, len(iconLayers))
	pngs := make([][]byte, len(iconLayers))
	for i, layer := range iconLayers {
		sizes[i] = layer.size
		var buf bytes.Buffer
		if err := png.Encode(&buf, renderLayer(layer)); err != nil {
			panic(err)
		}
		pngs[i] = buf.Bytes()
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
