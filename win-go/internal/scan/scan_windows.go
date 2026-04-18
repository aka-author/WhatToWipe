//go:build windows

package scan

import (
	"context"
	"fmt"
	"image/color"
	"os"
	"path/filepath"
	"sort"
	"time"

	"whatrwipe/win-go/internal/model"
)

// ScanTree walks the folder tree from root. onProgress receives the folder path currently scanned (FS: status bar during scan).
// Returns cancelled true if ctx cancelled. errCount is entries that could not be read (partial IO-02 tracking).
func ScanTree(ctx context.Context, root string, onProgress func(string)) (model.FolderNode, int, bool) {
	visited := map[string]struct{}{}
	var errCount int
	n, cancelled := scanDir(ctx, root, visited, &errCount, onProgress)
	return n, errCount, cancelled
}

func scanDir(ctx context.Context, path string, visited map[string]struct{}, errCount *int, onProgress func(string)) (model.FolderNode, bool) {
	select {
	case <-ctx.Done():
		return model.FolderNode{}, true
	default:
	}

	key := filepath.Clean(path)
	if _, ok := visited[key]; ok {
		*errCount++
		base := filepath.Base(path)
		if base == "." || base == "" {
			base = path
		}
		// IO-03: skip revisiting the same canonical path (junction/symlink cycles).
		return model.FolderNode{Path: path, Name: base, Error: "skipped (reparse cycle)"}, false
	}
	visited[key] = struct{}{}

	onProgress(path)

	node := model.FolderNode{Path: path, Name: filepath.Base(path)}
	if node.Name == "" || node.Name == "." {
		node.Name = path
	}

	entries, err := readDirBounded(ctx, path, 30*time.Second)
	if err != nil {
		node.Error = err.Error()
		*errCount++
		return node, false
	}

	var total int64
	for _, e := range entries {
		select {
		case <-ctx.Done():
			return model.FolderNode{}, true
		default:
		}
		full := filepath.Join(path, e.Name())
		if e.IsDir() {
			kid, cancelled := scanDir(ctx, full, visited, errCount, onProgress)
			if cancelled {
				return model.FolderNode{}, true
			}
			total += kid.Size
			if kid.Error != "" && node.Error == "" {
				node.Error = "partial read in subtree"
			}
			node.Kids = append(node.Kids, kid)
			continue
		}
		info, err := e.Info()
		if err != nil {
			*errCount++
			continue
		}
		total += info.Size()
	}
	node.Size = total

	sort.Slice(node.Kids, func(i, j int) bool {
		return node.Kids[i].Size > node.Kids[j].Size
	})
	node.IsNode = len(node.Kids) > 0
	onProgress(path)
	return node, false
}

// AnnotateShares sets Share and IsNode recursively from drive total.
func AnnotateShares(n *model.FolderNode, drive uint64) {
	if drive == 0 {
		n.Share = 0
	} else {
		n.Share = float64(n.Size) / float64(drive)
	}
	n.IsNode = len(n.Kids) > 0
	for i := range n.Kids {
		AnnotateShares(&n.Kids[i], drive)
	}
}

// BuildTreeItems prepares treemap tiles for direct children (FS: tile = subfolder).
// Drive shares must come from AnnotateShares (k.Share) so they match FS hierarchy rules.
func BuildTreeItems(kids []model.FolderNode) []model.TreeItem {
	var sum int64
	for _, k := range kids {
		sum += max64(k.Size, 0)
	}
	if sum <= 0 {
		return nil
	}
	palette := []color.RGBA{
		{220, 90, 90, 255}, {70, 130, 220, 255}, {240, 190, 60, 255}, {60, 175, 165, 255},
		{150, 110, 220, 255}, {235, 140, 55, 255}, {50, 175, 95, 255}, {215, 75, 70, 255},
	}
	out := make([]model.TreeItem, 0, len(kids))
	for i, k := range kids {
		var c model.TreeItem
		c.Name = k.Name
		c.Path = k.Path
		c.Size = k.Size
		c.IsNode = k.IsNode
		c.DriveShare = k.Share
		if k.IsNode {
			c.Color = palette[i%len(palette)]
		} else {
			g := uint8(155 + (i%5)*8)
			c.Color = color.RGBA{R: g, G: g, B: g + 6, A: 255}
		}
		out = append(out, c)
	}
	return out
}

func max64(a, b int64) int64 {
	if a > b {
		return a
	}
	return b
}

// readDirBounded runs os.ReadDir off the cancellation path with a per-directory ceiling (techspec IO-04 default).
func readDirBounded(ctx context.Context, path string, perOp time.Duration) ([]os.DirEntry, error) {
	type result struct {
		entries []os.DirEntry
		err     error
	}
	ch := make(chan result, 1)
	go func() {
		e, err := os.ReadDir(path)
		ch <- result{e, err}
	}()
	timer := time.NewTimer(perOp)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return nil, ctx.Err()
	case r := <-ch:
		return r.entries, r.err
	case <-timer.C:
		return nil, fmt.Errorf("timed out reading directory after %v", perOp)
	}
}
