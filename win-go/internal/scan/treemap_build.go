package scan

import (
	"sort"

	"whatrwipe/win-go/internal/config"
	"whatrwipe/win-go/internal/model"
)

type treemapCand struct {
	name     string
	path     string
	size     int64
	isFolder bool
	isNode   bool
	share    float64
	clump    bool
}

// BuildTreemapItems builds tiles for the current context folder (nested file system objects
// one level down), honoring treemap.maxTiles and clump aggregation (funcspec § Treemap → Tiles).
func BuildTreemapItems(cur *model.FolderNode, driveTotal uint64, cfg config.Treemap) []model.TreeItem {
	if cur == nil {
		return nil
	}
	max := cfg.MaxTiles
	if max < 1 {
		max = 25
	}

	var cands []treemapCand
	for _, k := range cur.Kids {
		cands = append(cands, treemapCand{
			name: k.Name, path: k.Path, size: k.Size, isFolder: true, isNode: k.IsNode, share: k.Share,
		})
	}
	for _, f := range cur.Files {
		sh := 0.0
		if driveTotal > 0 {
			sh = float64(f.Size) / float64(driveTotal)
		}
		cands = append(cands, treemapCand{
			name: f.Name, path: f.Path, size: f.Size, isFolder: false, isNode: false, share: sh,
		})
	}
	if len(cands) == 0 {
		return nil
	}
	sort.SliceStable(cands, func(i, j int) bool {
		if cands[i].size != cands[j].size {
			return cands[i].size > cands[j].size
		}
		return cands[i].path < cands[j].path
	})

	var picked []treemapCand
	if len(cands) <= max {
		picked = cands
	} else {
		head := max - 1
		if head < 0 {
			head = 0
		}
		picked = append(picked, cands[:head]...)
		var sum int64
		for _, c := range cands[head:] {
			sum += c.size
		}
		clShare := 0.0
		if driveTotal > 0 {
			clShare = float64(sum) / float64(driveTotal)
		}
		picked = append(picked, treemapCand{
			name: "Other", path: "", size: sum, isFolder: false, isNode: false, share: clShare, clump: true,
		})
	}

	out := make([]model.TreeItem, 0, len(picked))
	for _, c := range picked {
		ti := model.TreeItem{
			Name: c.name, Path: c.path, Size: c.size,
			DriveShare: c.share, IsNode: c.isNode,
		}
		switch {
		case c.clump:
			ti.Kind = model.TreemapItemClump
			ti.Color = cfg.NativeClumpBg
			ti.TextColor = cfg.NativeClumpText
		case c.isFolder:
			ti.Kind = model.TreemapItemFolder
			ti.Color = cfg.NativeFolderBg
			ti.TextColor = cfg.NativeFolderText
		default:
			ti.Kind = model.TreemapItemFile
			ti.Color = cfg.NativeFileBg
			ti.TextColor = cfg.NativeFileText
		}
		out = append(out, ti)
	}
	return out
}
