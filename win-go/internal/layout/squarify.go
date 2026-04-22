package layout

import (
	"image"

	"whatrwipe/win-go/internal/model"
)

// Squarified lays out treemap tiles from child metrics (samples squarify, adapted to model types).
func Squarified(items []model.TreeItem, area image.Rectangle, minTileW, minTileH int) []model.BlockLayout {
	if len(items) == 0 {
		return nil
	}
	if minTileW < 1 {
		minTileW = 1
	}
	if minTileH < 1 {
		minTileH = 1
	}
	return squarifyRecursive(items, area, minTileW, minTileH)
}

func squarifyRecursive(items []model.TreeItem, area image.Rectangle, minTileW, minTileH int) []model.BlockLayout {
	if len(items) == 0 {
		return nil
	}
	if area.Dx() <= 0 || area.Dy() <= 0 {
		return []model.BlockLayout{blockFromItem(items[0], area)}
	}
	if len(items) > 1 && (area.Dx() <= 1 || area.Dy() <= 1) {
		return []model.BlockLayout{blockFromItem(items[0], area)}
	}
	if len(items) == 1 {
		return []model.BlockLayout{blockFromItem(items[0], area)}
	}

	var total int64
	for _, it := range items {
		total += max64(it.Size, 1)
	}
	if total <= 0 {
		return nil
	}

	leftSum := int64(0)
	split := 0
	for i := range items {
		if i == len(items)-1 {
			break
		}
		leftSum += max64(items[i].Size, 1)
		split = i + 1
		if leftSum*2 >= total {
			break
		}
	}
	if split <= 0 {
		split = 1
	}
	if split >= len(items) {
		split = len(items) - 1
	}

	aItems := items[:split]
	bItems := items[split:]

	aSum := int64(0)
	for _, it := range aItems {
		aSum += max64(it.Size, 1)
	}

	var aRect, bRect image.Rectangle
	tryVertical := area.Dx() >= area.Dy()
	if area.Dx() < 2*minTileW {
		tryVertical = false
	}
	if area.Dy() < 2*minTileH {
		tryVertical = true
	}
	if area.Dx() < 2*minTileW && area.Dy() < 2*minTileH {
		return []model.BlockLayout{blockFromItem(items[0], area)}
	}

	if tryVertical {
		wA := int((int64(area.Dx()) * aSum) / total)
		if wA < minTileW {
			wA = minTileW
		}
		maxWA := area.Dx() - minTileW
		if wA > maxWA {
			wA = maxWA
		}
		aRect = image.Rect(area.Min.X, area.Min.Y, area.Min.X+wA, area.Max.Y)
		bRect = image.Rect(area.Min.X+wA, area.Min.Y, area.Max.X, area.Max.Y)
	} else {
		hA := int((int64(area.Dy()) * aSum) / total)
		if hA < minTileH {
			hA = minTileH
		}
		maxHA := area.Dy() - minTileH
		if hA > maxHA {
			hA = maxHA
		}
		aRect = image.Rect(area.Min.X, area.Min.Y, area.Max.X, area.Min.Y+hA)
		bRect = image.Rect(area.Min.X, area.Min.Y+hA, area.Max.X, area.Max.Y)
	}

	out := make([]model.BlockLayout, 0, len(items))
	out = append(out, squarifyRecursive(aItems, aRect, minTileW, minTileH)...)
	out = append(out, squarifyRecursive(bItems, bRect, minTileW, minTileH)...)
	return out
}

func blockFromItem(it model.TreeItem, area image.Rectangle) model.BlockLayout {
	return model.BlockLayout{
		Name:       it.Name,
		Path:       it.Path,
		Size:       it.Size,
		Rect:       area,
		Color:      it.Color,
		TextColor:  it.TextColor,
		IsNode:     it.IsNode,
		DriveShare: it.DriveShare,
		Kind:       it.Kind,
	}
}

func max64(a, b int64) int64 {
	if a > b {
		return a
	}
	return b
}
