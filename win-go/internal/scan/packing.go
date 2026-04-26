package scan

import (
	"archive/zip"
	"io"
	"path/filepath"
	"strings"

	"github.com/nwaples/rardecode"

	"eraserewrite/win-go/internal/model"
)

type arcEntry struct {
	name  string
	isDir bool
}

func normArchivePath(name string) string {
	s := filepath.ToSlash(strings.TrimSpace(name))
	return strings.TrimSuffix(s, "/")
}

// classifyArchiveLayout maps catalog entries to packing type (funcspec glossary).
func classifyArchiveLayout(entries []arcEntry) model.PackingType {
	var names []arcEntry
	for _, e := range entries {
		if normArchivePath(e.name) == "" {
			continue
		}
		names = append(names, e)
	}
	if len(names) == 0 {
		return model.PackingPackedClump
	}
	roots := map[string]struct{}{}
	for _, e := range names {
		parts := strings.Split(normArchivePath(e.name), "/")
		if len(parts) == 0 || parts[0] == "" {
			continue
		}
		roots[parts[0]] = struct{}{}
	}
	if len(roots) > 1 {
		return model.PackingPackedClump
	}
	var root string
	for r := range roots {
		root = r
		break
	}
	if root == "" {
		return model.PackingPackedClump
	}
	for _, e := range names {
		n := normArchivePath(e.name)
		if n != root && !strings.HasPrefix(n, root+"/") {
			return model.PackingPackedClump
		}
	}
	rootFiles := 0
	underFiles := 0
	for _, e := range names {
		if e.isDir {
			continue
		}
		n := normArchivePath(e.name)
		if n == root {
			rootFiles++
		} else if strings.HasPrefix(n, root+"/") {
			underFiles++
		}
	}
	if rootFiles == 1 && underFiles == 0 {
		return model.PackingPackedFile
	}
	if underFiles > 0 {
		return model.PackingPackedFolder
	}
	if rootFiles == 0 {
		// directory-only layout under a single top-level name
		return model.PackingPackedFolder
	}
	return model.PackingPackedClump
}

func readZipEntries(path string) ([]arcEntry, error) {
	zr, err := zip.OpenReader(path)
	if err != nil {
		return nil, err
	}
	defer zr.Close()
	out := make([]arcEntry, 0, len(zr.File))
	for _, f := range zr.File {
		out = append(out, arcEntry{name: f.Name, isDir: f.FileInfo().IsDir()})
	}
	return out, nil
}

func readRarEntries(path string) ([]arcEntry, error) {
	rc, err := rardecode.OpenReader(path, "")
	if err != nil {
		return nil, err
	}
	defer rc.Close()
	var out []arcEntry
	for {
		h, err := rc.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return out, err
		}
		out = append(out, arcEntry{name: h.Name, isDir: h.IsDir})
		if _, err := io.Copy(io.Discard, rc); err != nil {
			return out, err
		}
	}
	return out, nil
}

// ArchivePackingForPath classifies .zip / .rar by catalog; on failure returns packed clump.
func ArchivePackingForPath(path string) model.PackingType {
	ext := strings.ToLower(filepath.Ext(path))
	switch ext {
	case ".zip":
		ents, err := readZipEntries(path)
		if err != nil {
			return model.PackingPackedClump
		}
		return classifyArchiveLayout(ents)
	case ".rar":
		ents, err := readRarEntries(path)
		if err != nil {
			return model.PackingPackedClump
		}
		return classifyArchiveLayout(ents)
	default:
		return model.PackingNative
	}
}
