package model

import (
	"image"
	"image/color"
)

// FolderNode is one folder in the scanned hierarchy (arch: treemap model).
type FolderNode struct {
	Path   string
	Name   string
	Size   int64
	Share  float64
	IsNode bool
	Kids   []FolderNode
	Error  string
}

// TreeItem is one treemap tile (direct child of current context).
type TreeItem struct {
	Name       string
	Path       string
	Size       int64
	Color      color.RGBA
	IsNode     bool
	DriveShare float64
}

// BlockLayout is a laid-out treemap tile in pixel space.
type BlockLayout struct {
	Name       string
	Path       string
	Size       int64
	Rect       image.Rectangle
	Color      color.RGBA
	IsNode     bool
	DriveShare float64
}
