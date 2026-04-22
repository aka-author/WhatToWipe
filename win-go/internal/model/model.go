package model

import (
	"image"
	"image/color"
)

// PackingType is how a file is classified for archives (funcspec glossary / Treemap colors).
type PackingType int8

const (
	PackingNative PackingType = iota
	PackingPackedFile
	PackingPackedFolder
	PackingPackedClump
)

// FileEntry is one file directly inside a folder (not recursive).
type FileEntry struct {
	Name    string
	Path    string
	Size    int64
	Packing PackingType
}

// FolderNode is one folder in the scanned hierarchy (arch: treemap model).
type FolderNode struct {
	Path   string
	Name   string
	Size   int64
	Share  float64
	IsNode bool
	Files  []FileEntry
	Kids   []FolderNode
	Error  string
}

// Treemap item kind (tile represents folder, file, or aggregated clump).
const (
	TreemapItemFolder = iota
	TreemapItemFile
	TreemapItemClump
)

// TreeItem is one treemap tile for the current context folder.
type TreeItem struct {
	Name       string
	Path       string
	Size       int64
	Color      color.RGBA
	TextColor  color.RGBA
	IsNode     bool
	DriveShare float64
	Kind       int
}

// BlockLayout is a laid-out treemap tile in pixel space.
type BlockLayout struct {
	Name       string
	Path       string
	Size       int64
	Rect       image.Rectangle
	Color      color.RGBA
	TextColor  color.RGBA
	IsNode     bool
	DriveShare float64
	Kind       int
}
