package scan

import (
	"sort"

	"eraserewrite/win-go/internal/config"
	"eraserewrite/win-go/internal/model"
)

type treemapCand struct {
	name           string
	path           string
	size           int64
	isFolder       bool
	isNode         bool
	isEmpty        bool
	isExecFile     bool
	share          float64
	clump          bool
	packing        model.PackingType
	clumpNonNative bool
}

func isExecutablePath(path string, cfg config.Treemap) bool {
	return config.FileMatchesExeList(path, config.ExeTypeList(cfg))
}

// BuildTreemapItems builds tiles for the current context folder (nested file system objects
// one level down), honoring treemap.maxTiles and clump aggregation (funcspec Р’В§ Treemap РІвЂ вЂ™ Tiles).
func BuildTreemapItems(cur *model.FolderNode, driveTotal uint64, cfg config.Treemap) []model.TreeItem {
	if cur == nil {
		return nil
	}
	max := cfg.MaxTiles
	if max < 1 {
		max = 25
	}
	clumpThreshold := cfg.ClumpThreshold
	if clumpThreshold <= 0 {
		clumpThreshold = 0.05
	}

	var cands []treemapCand
	for _, k := range cur.Kids {
		cands = append(cands, treemapCand{
			name: k.Name, path: k.Path, size: k.Size, isFolder: true, isNode: k.IsNode, share: k.Share,
			isEmpty: len(k.Kids) == 0 && len(k.Files) == 0,
			packing: model.PackingNative,
		})
	}
	for _, f := range cur.Files {
		sh := 0.0
		if driveTotal > 0 {
			sh = float64(f.Size) / float64(driveTotal)
		}
		cands = append(cands, treemapCand{
			name: f.Name, path: f.Path, size: f.Size, isFolder: false, isNode: false, share: sh,
			isExecFile: isExecutablePath(f.Path, cfg),
			packing:    f.Packing,
		})
	}
	if len(cands) == 0 {
		return nil
	}
	forcedClump := make([]treemapCand, 0)
	kept := make([]treemapCand, 0, len(cands))
	if cur.Size > 0 && clumpThreshold > 0 {
		limit := float64(cur.Size) * clumpThreshold
		for _, c := range cands {
			// Spec rule: small file objects always go to clump.
			if !c.isFolder && float64(c.size) < limit {
				forcedClump = append(forcedClump, c)
				continue
			}
			kept = append(kept, c)
		}
	} else {
		kept = cands
	}
	sort.SliceStable(kept, func(i, j int) bool {
		if kept[i].size != kept[j].size {
			return kept[i].size > kept[j].size
		}
		return kept[i].path < kept[j].path
	})

	var picked []treemapCand
	needClump := len(forcedClump) > 0 || len(kept) > max
	if !needClump {
		picked = kept
	} else {
		head := max - 1
		if head < 0 {
			head = 0
		}
		if head > len(kept) {
			head = len(kept)
		}
		picked = append(picked, kept[:head]...)
		var sum int64
		anyNonNative := false
		clumpMembers := append([]treemapCand(nil), forcedClump...)
		clumpMembers = append(clumpMembers, kept[head:]...)
		for _, c := range clumpMembers {
			sum += c.size
			if !c.isFolder && c.packing != model.PackingNative {
				anyNonNative = true
			}
		}
		clShare := 0.0
		if driveTotal > 0 {
			clShare = float64(sum) / float64(driveTotal)
		}
		picked = append(picked, treemapCand{
			name: "Other", path: "", size: sum, isFolder: false, isNode: false, share: clShare, clump: true,
			clumpNonNative: anyNonNative,
		})
	}

	out := make([]model.TreeItem, 0, len(picked))
	for _, c := range picked {
		ti := model.TreeItem{
			Name: c.name, Path: c.path, Size: c.size,
			DriveShare: c.share, IsNode: c.isNode, IsEmpty: c.isEmpty, IsExecFile: c.isExecFile,
		}
		switch {
		case c.clump:
			ti.Kind = model.TreemapItemClump
			if c.clumpNonNative {
				ti.Color = cfg.PackedClumpBg
				ti.TextColor = cfg.PackedClumpText
			} else {
				ti.Color = cfg.NativeClumpBg
				ti.TextColor = cfg.NativeClumpText
			}
		case c.isFolder:
			ti.Kind = model.TreemapItemFolder
			ti.Color = cfg.NativeFolderBg
			ti.TextColor = cfg.NativeFolderText
		default:
			ti.Kind = model.TreemapItemFile
			switch c.packing {
			case model.PackingPackedFile:
				ti.Color = cfg.PackedFileBg
				ti.TextColor = cfg.PackedFileText
			case model.PackingPackedFolder:
				ti.Color = cfg.PackedFolderBg
				ti.TextColor = cfg.PackedFolderText
			case model.PackingPackedClump:
				ti.Color = cfg.PackedClumpBg
				ti.TextColor = cfg.PackedClumpText
			default:
				ti.Color = cfg.NativeFileBg
				ti.TextColor = cfg.NativeFileText
			}
		}
		out = append(out, ti)
	}
	return out
}
