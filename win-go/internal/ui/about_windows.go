//go:build windows

// About art: place PNG at codebase/assets/art/about-bunny.png РІР‚вЂќ build.ps1 copies it into assets/art for //go:embed.

package ui

import (
	"bytes"
	"image/png"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"

	aboutart "eraserewrite/win-go/assets/art"
)

type dpiPicker interface {
	DPI() int
}

func ownerDPI(owner walk.Form) int {
	dpi := 96
	if d, ok := owner.(dpiPicker); ok {
		if x := d.DPI(); x > 0 {
			dpi = x
		}
	}
	return dpi
}

// aboutTextLabels returns static Label lines (split explicitly; no edit control, no auto-wrap engine).
func aboutTextLabels(versionDotted string) []Widget {
	w := 300
	return []Widget{
		Label{Text: "Erase && Rewrite is a disk space analyzer.", MaxSize: Size{Width: w, Height: 0}},
		Label{Text: "It scans a folder, shows its subfolders as a treemap,", MaxSize: Size{Width: w, Height: 0}},
		Label{Text: "and helps you decide what to keep, move, or remove.", MaxSize: Size{Width: w, Height: 0}},
		Label{Text: "Version " + versionDotted, MaxSize: Size{Width: w, Height: 0}},
	}
}

func aboutPlainFallback(versionDotted string) string {
	return "Erase & Rewrite is a disk space analyzer.\n\n" +
		"It scans a folder, shows its subfolders as a treemap, and helps you decide what to keep, move, or remove.\n\n" +
		"Version " + versionDotted
}

// aboutImageCellSize returns a 1/96" layout cell that matches the image aspect ratio and fits inside maxWР“вЂ”maxH.
func aboutImageCellSize(img walk.Image, maxW, maxH int) walk.Size {
	if img == nil || maxW <= 0 || maxH <= 0 {
		return walk.Size{Width: maxW, Height: maxH}
	}
	s := img.Size()
	sw, sh := s.Width, s.Height
	if sw <= 0 || sh <= 0 {
		return walk.Size{Width: maxW, Height: maxH}
	}
	sfx := float64(maxW) / float64(sw)
	sfy := float64(maxH) / float64(sh)
	scale := sfx
	if sfy < scale {
		scale = sfy
	}
	if scale > 1 {
		scale = 1
	}
	return walk.Size{
		Width:  int(float64(sw)*scale + 0.5),
		Height: int(float64(sh)*scale + 0.5),
	}
}

func loadAboutArtImage(owner walk.Form) walk.Image {
	dpi := ownerDPI(owner)
	if len(aboutart.AboutBunnyPNG) > 0 {
		im, err := png.Decode(bytes.NewReader(aboutart.AboutBunnyPNG))
		if err == nil {
			if b, err := walk.NewBitmapFromImageForDPI(im, dpi); err == nil {
				return b
			}
		}
	}
	if b, err := walk.NewBitmapFromImageForDPI(aboutBunnyPlaceholder(), dpi); err == nil {
		return b
	}
	return nil
}

func showAboutDialog(owner walk.Form, versionDotted string) {
	art := loadAboutArtImage(owner)
	var dlgIcon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		dlgIcon = ic
	}
	if art == nil {
		walk.MsgBox(owner, "About Erase && Rewrite", aboutPlainFallback(versionDotted), walk.MsgBoxOK|walk.MsgBoxIconInformation)
		return
	}

	var dlg *walk.Dialog
	var ok *walk.PushButton

	const imgMaxW, imgMaxH = 300, 260
	cell := aboutImageCellSize(art, imgMaxW, imgMaxH)
	cellDecl := Size{Width: cell.Width, Height: cell.Height}

	lines := aboutTextLabels(versionDotted)
	children := make([]Widget, 0, 2+len(lines)+1)
	children = append(children, ImageView{
		Image:     art,
		MinSize:   cellDecl,
		MaxSize:   cellDecl,
		Mode:      ImageViewModeShrink,
		Alignment: AlignHCenterVCenter,
	})
	children = append(children, lines...)
	children = append(children, Composite{
		Layout: HBox{MarginsZero: true, Spacing: 8},
		Children: []Widget{
			HSpacer{},
			PushButton{
				AssignTo: &ok,
				Text:     "OK",
				OnClicked: func() {
					if dlg != nil {
						dlg.Accept()
					}
				},
			},
		},
	})

	dlgW := cell.Width + 80
	if dlgW < 360 {
		dlgW = 360
	}
	dlgH := cell.Height + 220
	if dlgH < 300 {
		dlgH = 300
	}

	_, _ = Dialog{
		AssignTo:      &dlg,
		Title:         "About Erase && Rewrite",
		Icon:          dlgIcon,
		FixedSize:     true,
		Size:          Size{Width: dlgW, Height: dlgH},
		DefaultButton: &ok,
		Layout:        VBox{Margins: Margins{16, 16, 16, 16}, Spacing: 8},
		Children:      children,
	}.Run(owner)
}
