//go:build windows

package ui

import (
	"context"
	"fmt"
	"image"
	"image/color"
	"os"
	"os/exec"
	"sync"
	"sync/atomic"
	"unicode/utf8"

	"github.com/lxn/win"
	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"

	"whatrwipe/win-go/internal/art"
	"whatrwipe/win-go/internal/format"
	"whatrwipe/win-go/internal/layout"
	"whatrwipe/win-go/internal/model"
	"whatrwipe/win-go/internal/scan"
	"whatrwipe/win-go/internal/volume"
	"whatrwipe/win-go/internal/winver"
)

// maxTreemapVerticalUnitPt is FS § Treemap → Metrics: vertical unit (vu) must not exceed 45 points.
const maxTreemapVerticalUnitPt = 45

// shabbyLabelFolderNamePt / shabbyLabelFolderDetailsPt: fixed sizes for shabby-tile on-tile labels (FS § Tile Labels).
const shabbyLabelFolderNamePt = 10
const shabbyLabelFolderDetailsPt = 8 // 0.8 × 10 pt per Folder Details style table

type scanKind int

const (
	scanOpen scanKind = iota
	scanUpdate
)

type snap struct {
	root model.FolderNode
	nav  []string
}

type app struct {
	mw    *walk.MainWindow
	chart *walk.CustomWidget
	st    *walk.StatusBarItem

	openAction   *walk.Action
	upAction     *walk.Action
	manageAction *walk.Action
	updateMenu  *walk.Action
	stopMenu    *walk.Action
	openBtn     *walk.ToolButton
	upBtn       *walk.ToolButton
	manageBtn   *walk.ToolButton
	scanBtn     *walk.ToolButton
	volTotalLbl *walk.Label
	volFreeBtn  *walk.PushButton
	aboutAction *walk.Action

	openBmp   *walk.Bitmap
	upBmp     *walk.Bitmap
	manageBmp *walk.Bitmap
	updateBmp *walk.Bitmap
	stopBmp   *walk.Bitmap

	rootNode        model.FolderNode
	navPath         []string
	targetPath      string
	volBarRoot      string // volume root for toolbar (e.g. D:\); empty until a target folder is chosen
	driveTotal      uint64
	driveFree       uint64
	items           []model.TreeItem
	blocks          []model.BlockLayout
	chartBmp        *walk.Bitmap
	chartW, chartH  int
	chartDirty      bool
	treemapComplete bool

	hoverIdx, hoverX, hoverY int
	labelFonts               map[int]*walk.Font
	smallFont                *walk.Font

	scanCtx    context.Context
	scanCancel context.CancelFunc
	scanSeq    uint64
	scanning   atomic.Bool

	pendingUpdateSnap *snap
	inspectHit        int

	tooltipsOnce sync.Once
}

// Run starts the WhatToWipe main window (FS + techspec shell).
func Run() error {
	a := &app{
		hoverIdx:    -1,
		inspectHit:  -1,
		labelFonts:  make(map[int]*walk.Font),
	}

	if err := a.loadToolbarArt(); err != nil {
		return err
	}

	var appIcon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		appIcon = ic
	}

	mwDecl := MainWindow{
		AssignTo: &a.mw,
		Title:    "WhatToWipe",
		Size:     Size{1024, 760},
		OnSizeChanged: func() {
			a.setScanChrome(a.scanning.Load())
		},
		MenuItems: []MenuItem{
			Menu{
				Text: "&File",
				Items: []MenuItem{
					Action{
						AssignTo:    &a.openAction,
						Text:        "&Open a Folder…",
						Image:       a.openBmp,
						Shortcut:    Shortcut{walk.ModControl, walk.KeyO},
						OnTriggered: a.onOpenFolder,
					},
					Separator{},
					Action{
						Text:        "E&xit",
						Shortcut:    Shortcut{walk.ModControl, walk.KeyX},
						OnTriggered: func() { a.mw.Close() },
					},
				},
			},
			Menu{
				Text: "&Inspect",
				Items: []MenuItem{
					Action{
						AssignTo:    &a.upAction,
						Text:        "&Up",
						Image:       a.upBmp,
						Shortcut:    Shortcut{0, walk.KeyBack},
						Enabled:     false,
						OnTriggered: a.onUp,
					},
					Action{
						AssignTo:    &a.manageAction,
						Text:        "&Manage",
						Image:       a.manageBmp,
						Shortcut:    Shortcut{walk.ModControl, walk.KeyM},
						Enabled:     false,
						OnTriggered: a.onManage,
					},
					Separator{},
					Action{
						AssignTo:    &a.updateMenu,
						Text:        "&Update",
						Shortcut:    Shortcut{walk.ModControl, walk.KeyS},
						Visible:     true,
						Enabled:     false,
						OnTriggered: a.onUpdate,
					},
					Action{
						AssignTo:    &a.stopMenu,
						Text:        "&Stop",
						Shortcut:    Shortcut{0, walk.KeyEscape},
						Visible:     false,
						OnTriggered: a.onStop,
					},
				},
			},
			Menu{
				Text: "&Help",
				Items: []MenuItem{
					Action{
						AssignTo:    &a.aboutAction,
						Text:        "&About",
						OnTriggered: a.onAbout,
					},
				},
			},
		},
		StatusBarItems: []StatusBarItem{
			{AssignTo: &a.st, Text: "Choose a target folder"},
		},
		Layout:   VBox{Margins: Margins{}, Spacing: 0},
		Children: a.chartChildren(),
	}
	if appIcon != nil {
		mwDecl.Icon = appIcon
	}
	_, err := mwDecl.Run()

	return err
}

func (a *app) chartChildren() []Widget {
	tbPad := Size{Width: 32, Height: 32}
	return []Widget{
		Composite{
			Layout: HBox{Margins: Margins{Left: 8, Top: 6, Right: 8, Bottom: 6}, Spacing: 8},
			Children: []Widget{
				ToolButton{
					AssignTo:    &a.openBtn,
					Image:       a.openBmp,
					MinSize:     tbPad,
					MaxSize:     tbPad,
					ToolTipText: "Open a folder",
					OnClicked:   a.onOpenFolder,
				},
				ToolButton{
					AssignTo:    &a.upBtn,
					Image:       a.upBmp,
					MinSize:     tbPad,
					MaxSize:     tbPad,
					ToolTipText: "Go up",
					OnClicked:   a.onUp,
				},
				ToolButton{
					AssignTo:    &a.manageBtn,
					Image:       a.manageBmp,
					MinSize:     tbPad,
					MaxSize:     tbPad,
					ToolTipText: "Open in file manager",
					OnClicked:   a.onManage,
				},
				ToolButton{
					AssignTo:    &a.scanBtn,
					Image:       a.updateBmp,
					MinSize:     tbPad,
					MaxSize:     tbPad,
					Enabled:     false,
					ToolTipText: "Update the folder data",
					OnClicked:   a.onScanToolbar,
				},
				Label{
					AssignTo:    &a.volTotalLbl,
					Text:        "Total: —",
					MaxSize:     Size{Width: 520, Height: 0},
					ToolTipText: "Total capacity of the drive that contains the current target folder. Read-only. Until you open a folder, no drive is shown (multiple drives are possible).",
				},
				PushButton{
					AssignTo:    &a.volFreeBtn,
					Text:        "Free: —",
					Enabled:     false,
					ToolTipText: "Free space on that same drive. After you open a target folder, click to refresh if files change outside the app.",
					OnClicked: func() {
						a.onVolBarFreeRefresh()
					},
				},
				HSpacer{},
			},
		},
		CustomWidget{
			AssignTo:            &a.chart,
			Style:               win.WS_TABSTOP,
			StretchFactor:       1,
			AlwaysConsumeSpace:  true,
			InvalidatesOnResize: true,
			ClearsBackground:    true,
			PaintPixels:         a.paintTreemap,
			ContextMenuItems: []MenuItem{
				Action{
					Text:        "Inspect",
					OnTriggered: a.onInspectContext,
				},
			},
			OnBoundsChanged: func() {
				a.chartDirty = true
				if a.chart != nil {
					a.chart.Invalidate()
				}
			},
			OnKeyDown: func(key walk.Key) {
				switch key {
				case walk.KeyBack:
					a.onUp()
				case walk.KeyEscape:
					if a.scanning.Load() {
						a.onStop()
					}
				}
			},
			OnMouseDown: func(x, y int, button walk.MouseButton) {
				if button == walk.RightButton {
					h := a.hitTest(x, y)
					if h >= 0 && h < len(a.blocks) {
						a.inspectHit = h
					} else {
						a.inspectHit = -1
					}
					return
				}
				if button != walk.LeftButton {
					return
				}
				if a.scanning.Load() {
					return
				}
				h := a.hitTest(x, y)
				if h < 0 || h >= len(a.blocks) {
					return
				}
				if !a.blocks[h].IsNode {
					return
				}
				a.navPath = append(a.navPath, a.blocks[h].Path)
				a.rebuildTreemap()
			},
			OnMouseMove: func(x, y int, button walk.MouseButton) {
				i := a.hitTest(x, y)
				if i == a.hoverIdx {
					return
				}
				a.hoverIdx = i
				a.hoverX, a.hoverY = x, y
				if a.chart != nil {
					a.chart.Invalidate()
				}
			},
		},
	}
}

func (a *app) onInspectContext() {
	if a.inspectHit < 0 || a.inspectHit >= len(a.blocks) {
		return
	}
	a.openInExplorer(a.blocks[a.inspectHit].Path)
}

func (a *app) loadToolbarArt() error {
	var err error
	if a.openBmp, err = art.OpenFolderBitmap(); err != nil {
		return err
	}
	if a.upBmp, err = art.UpInCircleBitmap(); err != nil {
		return err
	}
	if a.manageBmp, err = art.EyeBitmap(); err != nil {
		return err
	}
	if a.updateBmp, err = art.UpdatePlayBitmap(); err != nil {
		return err
	}
	if a.stopBmp, err = art.StopSolidBitmap(); err != nil {
		return err
	}
	return nil
}

func (a *app) setStatus(s string) {
	if a.st != nil {
		a.st.SetText(s)
	}
}

func (a *app) onOpenFolder() {
	if a.scanning.Load() {
		return
	}
	dlg := walk.FileDialog{Title: "Open a folder"}
	ok, err := dlg.ShowBrowseFolder(a.mw)
	if err != nil {
		a.setStatus("Could not open folder dialog: " + err.Error())
		return
	}
	if !ok || dlg.FilePath == "" {
		return
	}
	a.pendingUpdateSnap = nil
	a.startScan(dlg.FilePath, scanOpen)
}

func (a *app) onUpdate() {
	if a.scanning.Load() || a.targetPath == "" || !a.treemapComplete {
		return
	}
	a.pendingUpdateSnap = &snap{root: cloneFolder(a.rootNode), nav: append([]string(nil), a.navPath...)}
	a.startScan(a.targetPath, scanUpdate)
}

func (a *app) onStop() {
	if !a.scanning.Load() {
		return
	}
	if a.scanCancel != nil {
		a.scanCancel()
	}
}

func (a *app) onScanToolbar() {
	if a.scanning.Load() {
		a.onStop()
	} else {
		a.onUpdate()
	}
}

func (a *app) onUp() {
	if a.scanning.Load() || len(a.navPath) == 0 {
		return
	}
	a.navPath = a.navPath[:len(a.navPath)-1]
	a.rebuildTreemap()
}

func (a *app) onManage() {
	cur := a.resolveCurrent()
	if cur == nil {
		return
	}
	a.openInExplorer(cur.Path)
}

func (a *app) onAbout() {
	exe, err := os.Executable()
	if err != nil {
		exe = ""
	}
	ver := "0.0.0.0"
	if exe != "" {
		if v, e := winver.FileVersionDotted(exe); e == nil {
			ver = v
		}
	}
	showAboutDialog(a.mw, ver)
}

func (a *app) openInExplorer(path string) {
	cmd := exec.Command("explorer", path)
	if err := cmd.Start(); err != nil {
		walk.MsgBox(a.mw, "WhatToWipe", "Could not open file manager:\n"+err.Error(), walk.MsgBoxIconError)
	}
}

func (a *app) startScan(folder string, kind scanKind) {
	if a.scanning.Load() {
		return
	}
	a.scanCtx, a.scanCancel = context.WithCancel(context.Background())
	seq := atomic.AddUint64(&a.scanSeq, 1)

	a.scanning.Store(true)
	a.setScanChrome(true)

	if kind == scanOpen {
		a.rootNode = model.FolderNode{}
		a.navPath = nil
		a.items = nil
		a.blocks = nil
		a.chartDirty = true
		a.treemapComplete = false
		a.targetPath = folder
		if a.chart != nil {
			a.chart.Invalidate()
		}
	}
	// scanUpdate: keep the previous complete treemap visible until this scan finishes (techspec UX-01).

	if r, err := volume.RootForPath(folder); err == nil {
		a.volBarRoot = r
	} else {
		a.volBarRoot = ""
	}
	dt, fr, volErr := volume.DiskSpace(folder)
	if volErr != nil {
		dt, fr = 0, 0
	}
	a.driveTotal, a.driveFree = dt, fr

	go func(scanID uint64, root string, sk scanKind) {
		node, errCount, cancelled := scan.ScanTree(a.scanCtx, root, func(p string) {
			a.mw.Synchronize(func() {
				if scanID != atomic.LoadUint64(&a.scanSeq) {
					return
				}
				a.setStatus(p)
			})
		})

		a.mw.Synchronize(func() {
			defer a.refreshVolumeToolbar()
			if scanID != atomic.LoadUint64(&a.scanSeq) {
				return
			}
			a.scanning.Store(false)
			a.scanCancel = nil
			a.setScanChrome(false)

			if cancelled {
				if sk == scanUpdate && a.pendingUpdateSnap != nil {
					a.rootNode = cloneFolder(a.pendingUpdateSnap.root)
					a.navPath = append([]string(nil), a.pendingUpdateSnap.nav...)
					a.pendingUpdateSnap = nil
					a.treemapComplete = true
					a.rebuildTreemap()
					a.setStatus(a.statusForContext())
					return
				}
				a.pendingUpdateSnap = nil
				a.rootNode = model.FolderNode{}
				a.navPath = nil
				a.items = nil
				a.blocks = nil
				a.targetPath = ""
				a.volBarRoot = ""
				a.driveTotal, a.driveFree = 0, 0
				a.treemapComplete = false
				a.chartDirty = true
				if a.chart != nil {
					a.chart.Invalidate()
				}
				a.setStatus("Choose a target folder")
				return
			}

			if node.Error != "" && len(node.Kids) == 0 && node.Size == 0 {
				if sk == scanUpdate && a.pendingUpdateSnap != nil {
					a.rootNode = cloneFolder(a.pendingUpdateSnap.root)
					a.navPath = append([]string(nil), a.pendingUpdateSnap.nav...)
					a.pendingUpdateSnap = nil
					a.treemapComplete = true
					a.chartDirty = true
					if a.chart != nil {
						a.chart.Invalidate()
					}
					a.rebuildTreemap()
					a.setStatus(a.statusForContext())
					walk.MsgBox(a.mw, "WhatToWipe", "Could not read folder:\n"+node.Error, walk.MsgBoxIconError)
					return
				}
				a.pendingUpdateSnap = nil
				a.rootNode = model.FolderNode{}
				a.navPath = nil
				a.targetPath = ""
				a.volBarRoot = ""
				a.driveTotal, a.driveFree = 0, 0
				a.treemapComplete = false
				a.chartDirty = true
				if a.chart != nil {
					a.chart.Invalidate()
				}
				a.setStatus("Choose a target folder")
				walk.MsgBox(a.mw, "WhatToWipe", "Could not read folder:\n"+node.Error, walk.MsgBoxIconError)
				return
			}

			scan.AnnotateShares(&node, a.driveTotal)
			a.rootNode = node
			a.navPath = nil
			a.treemapComplete = true
			a.pendingUpdateSnap = nil
			a.rebuildTreemap()
			msg := a.statusForContext()
			if errCount > 0 {
				msg = fmt.Sprintf("Scan finished with %d paths that could not be read. %s", errCount, msg)
			}
			a.setStatus(msg)
		})
	}(seq, folder, kind)
}

func (a *app) setScanChrome(scanning bool) {
	// WM_SIZE/OnSizeChanged can fire while MainWindow is still being constructed; menu actions are nil until defer completes.
	if a.openAction != nil && a.upAction != nil && a.manageAction != nil && a.updateMenu != nil && a.stopMenu != nil && a.mw != nil {
		a.tooltipsOnce.Do(func() {
			_ = a.openAction.SetToolTip("Open a folder")
			_ = a.upAction.SetToolTip("Go up")
			_ = a.manageAction.SetToolTip("Open in file manager")
			_ = a.updateMenu.SetToolTip("Update the folder data")
			_ = a.stopMenu.SetToolTip("Stop scanning folders")
		})
	}
	if a.openAction != nil {
		a.openAction.SetEnabled(!scanning)
	}
	if a.openBtn != nil {
		a.openBtn.SetEnabled(!scanning)
	}
	if a.updateMenu != nil {
		_ = a.updateMenu.SetVisible(!scanning)
		// FS: unavailable commands disabled — Update only after a successful target scan.
		canUpdate := !scanning && a.treemapComplete && a.targetPath != ""
		_ = a.updateMenu.SetEnabled(canUpdate)
	}
	if a.stopMenu != nil {
		_ = a.stopMenu.SetVisible(scanning)
	}
	if a.scanBtn != nil {
		if scanning {
			_ = a.scanBtn.SetImage(a.stopBmp)
			_ = a.scanBtn.SetToolTipText("Stop scanning folders")
			a.scanBtn.SetEnabled(true)
		} else {
			_ = a.scanBtn.SetImage(a.updateBmp)
			_ = a.scanBtn.SetToolTipText("Update the folder data")
			a.scanBtn.SetEnabled(a.treemapComplete && a.targetPath != "")
		}
	}
	cur := a.resolveCurrent()
	up := len(a.navPath) > 0 && !scanning && a.treemapComplete
	if a.upAction != nil {
		a.upAction.SetEnabled(up)
	}
	if a.upBtn != nil {
		a.upBtn.SetEnabled(up)
	}
	manage := cur != nil && !scanning && a.rootNode.Path != ""
	if a.manageAction != nil {
		a.manageAction.SetEnabled(manage)
	}
	if a.manageBtn != nil {
		a.manageBtn.SetEnabled(manage)
	}
	if a.volFreeBtn != nil {
		hasVol := a.volBarRoot != "" && a.targetPath != ""
		a.volFreeBtn.SetEnabled(hasVol)
	}
	a.refreshVolumeToolbar()
}

func (a *app) onVolBarFreeRefresh() {
	if a.volBarRoot == "" || a.targetPath == "" {
		return
	}
	a.refreshVolumeToolbar()
}

func (a *app) refreshVolumeToolbar() {
	if a.volTotalLbl == nil || a.volFreeBtn == nil {
		return
	}
	if a.volBarRoot == "" || a.targetPath == "" {
		_ = a.volTotalLbl.SetText("Total: —")
		_ = a.volFreeBtn.SetText("Free: —")
		return
	}
	letter := volume.DriveLabel(a.volBarRoot)
	tot, fr, err := volume.DiskSpace(a.volBarRoot)
	if err != nil {
		_ = a.volTotalLbl.SetText("Total at " + letter + " —")
		_ = a.volFreeBtn.SetText("Free at " + letter + " —")
		return
	}
	a.driveTotal, a.driveFree = tot, fr
	_ = a.volTotalLbl.SetText("Total at "+letter+" "+format.ObjectSize(int64(tot)))
	_ = a.volFreeBtn.SetText("Free at "+letter+" "+format.ObjectSize(int64(fr)))
}

func (a *app) statusForContext() string {
	cur := a.resolveCurrent()
	if cur == nil {
		return "Choose a target folder"
	}
	return cur.Path
}

func (a *app) resolveCurrent() *model.FolderNode {
	if a.rootNode.Path == "" {
		return nil
	}
	cur := &a.rootNode
	for _, p := range a.navPath {
		found := false
		for i := range cur.Kids {
			if cur.Kids[i].Path == p {
				cur = &cur.Kids[i]
				found = true
				break
			}
		}
		if !found {
			return cur
		}
	}
	return cur
}

func (a *app) rebuildTreemap() {
	cur := a.resolveCurrent()
	a.setScanChrome(a.scanning.Load())

	if cur != nil && a.rootNode.Path != "" && !a.scanning.Load() {
		a.setStatus(cur.Path)
	}

	if cur == nil {
		a.items = nil
		a.blocks = nil
		a.chartDirty = true
		if a.chart != nil {
			a.chart.Invalidate()
		}
		return
	}

	if len(cur.Kids) == 0 {
		a.items = nil
		a.blocks = nil
		a.chartDirty = true
		if a.chart != nil {
			a.chart.Invalidate()
		}
		return
	}

	a.items = scan.BuildTreeItems(cur.Kids)
	a.blocks = nil
	a.chartDirty = true
	if a.chart != nil {
		a.chart.Invalidate()
		_ = a.chart.SetFocus()
	}
}

func (a *app) paintTreemap(canvas *walk.Canvas, _ walk.Rectangle) error {
	if a.chart == nil {
		return nil
	}
	bounds := a.chart.ClientBoundsPixels()
	if bounds.Width <= 0 || bounds.Height <= 0 {
		bounds = canvas.BoundsPixels()
	}
	if bounds.Width <= 0 || bounds.Height <= 0 {
		return nil
	}

	if a.chartDirty || a.chartBmp == nil || a.chartW != bounds.Width || a.chartH != bounds.Height {
		img := image.NewRGBA(image.Rect(0, 0, bounds.Width, bounds.Height))
		fillBG(img, color.RGBA{250, 250, 252, 255})

		if len(a.items) > 0 {
			area := image.Rect(0, 0, bounds.Width, bounds.Height)
			a.blocks = layout.Squarified(a.items, area)
			for _, b := range a.blocks {
				fillRect(img, b.Rect, b.Color)
				strokeRect(img, b.Rect, color.RGBA{40, 40, 45, 255})
			}
		} else {
			a.blocks = nil
		}
		strokeRect(img, image.Rect(0, 0, bounds.Width, bounds.Height), color.RGBA{25, 25, 30, 255})

		if a.chartBmp != nil {
			a.chartBmp.Dispose()
		}
		bmp, err := walk.NewBitmapFromImage(img)
		if err != nil {
			return err
		}
		a.chartBmp = bmp
		a.chartW = bounds.Width
		a.chartH = bounds.Height
		a.chartDirty = false
	}

	if a.chartBmp == nil {
		return nil
	}
	if err := canvas.DrawImagePixels(a.chartBmp, walk.Point{}); err != nil {
		return err
	}

	a.drawBlockLabels(canvas)
	a.drawShabbyTooltip(canvas, bounds)
	return nil
}

func (a *app) drawBlockLabels(canvas *walk.Canvas) {
	dpi := 96
	if a.chart != nil {
		dpi = a.chart.DPI()
	}
	for _, b := range a.blocks {
		if a.tileIsFancy(b, dpi, canvas) {
			a.drawFancyTile(canvas, b, dpi)
		} else {
			a.drawShabbyTile(canvas, b, dpi)
		}
	}
}

// treemapClampedFontSizes maps FS vertical unit (pt) to Folder Name / Folder Details point sizes.
func (a *app) treemapClampedFontSizes(vuPt float64) (vu, meta int) {
	vu = int(vuPt + 0.5)
	if vu > maxTreemapVerticalUnitPt {
		vu = maxTreemapVerticalUnitPt
	}
	if vu < 6 {
		vu = 6
	}
	meta = int(float64(vu)*0.8 + 0.5)
	if meta > maxTreemapVerticalUnitPt {
		meta = maxTreemapVerticalUnitPt
	}
	if meta < 6 {
		meta = 6
	}
	return vu, meta
}

// treemapLabelMinHeightPx is the minimum tile height (px) for the FS fancy label stack:
// pad, Folder Name (1 vu), indent 0.5 vu before Folder Details, then two 0.8 vu lines.
func (a *app) treemapLabelMinHeightPx(dpi, vu, meta int) int {
	if a.chart == nil {
		return 1 << 30
	}
	nameFont := a.ensureLabelFont(vu)
	metaFont := a.ensureLabelFont(meta)
	if nameFont == nil || metaFont == nil {
		return 1 << 30
	}
	nameLH := textLineHeightPx(a.chart, nameFont)
	metaLH := textLineHeightPx(a.chart, metaFont)
	gap := int(0.5*float64(vu)*float64(dpi)/72.0 + 0.5) // FS: Folder Details indent above 0.5 vu
	const padY = 6
	return padY + nameLH + gap + metaLH + metaLH
}

func (a *app) tileIsFancy(b model.BlockLayout, dpi int, canvas *walk.Canvas) bool {
	if a.chart == nil {
		return false
	}
	vuPt := a.verticalUnitPt(b, dpi, canvas)
	if vuPt < 10 {
		return false
	}
	vu, meta := a.treemapClampedFontSizes(vuPt)
	return b.Rect.Dy() >= a.treemapLabelMinHeightPx(dpi, vu, meta)
}

func (a *app) verticalUnitPt(b model.BlockLayout, dpi int, canvas *walk.Canvas) float64 {
	wPx := b.Rect.Dx()
	ext := utf8.RuneCountInString(b.Name) + 2
	if ext < 1 {
		ext = 1
	}
	wPt := float64(wPx) * 72.0 / float64(dpi)
	hu := wPt / float64(ext)

	var best float64 = 8
	bestDiff := 1e9
	// Only font sizes ≤ maxTreemapVerticalUnitPt are legal for vu (FS); never search above it.
	for s := 4; s <= maxTreemapVerticalUnitPt; s++ {
		font := a.ensureLabelFont(s)
		if font == nil {
			continue
		}
		bounds, _, err := canvas.MeasureTextPixels("M", font, walk.Rectangle{Width: 4000, Height: 400}, walk.TextCalcRect|walk.TextSingleLine)
		if err != nil {
			continue
		}
		mPt := float64(bounds.Width) * 72.0 / float64(dpi)
		d := absf(mPt - hu)
		if d < bestDiff {
			bestDiff = d
			best = float64(s)
		}
	}
	if best > maxTreemapVerticalUnitPt {
		return float64(maxTreemapVerticalUnitPt)
	}
	return best
}

func (a *app) drawFancyTile(canvas *walk.Canvas, b model.BlockLayout, dpi int) {
	vu, metaPt := a.treemapClampedFontSizes(a.verticalUnitPt(b, dpi, canvas))
	a.drawTreemapTileLabel(canvas, b, dpi, vu, metaPt)
}

func (a *app) drawShabbyTile(canvas *walk.Canvas, b model.BlockLayout, dpi int) {
	a.drawTreemapTileLabel(canvas, b, dpi, shabbyLabelFolderNamePt, shabbyLabelFolderDetailsPt)
}

// drawTreemapTileLabel draws the three-line tile label (Folder Name + gap + two Folder Details lines).
// namePt is the Folder Name font size in points; metaPt is Folder Details (0.8 × vu in FS, passed explicitly).
func (a *app) drawTreemapTileLabel(canvas *walk.Canvas, b model.BlockLayout, dpi, namePt, metaPt int) {
	nameFont := a.ensureLabelFont(namePt)
	metaFont := a.ensureLabelFont(metaPt)
	if nameFont == nil || metaFont == nil {
		return
	}
	padX, padY := 6, 6
	innerW := b.Rect.Dx() - 2*padX
	nameLH := textLineHeightPx(a.chart, nameFont)
	metaLH := textLineHeightPx(a.chart, metaFont)
	y := b.Rect.Min.Y + padY
	drawLine := func(text string, font *walk.Font, lh int, clr walk.Color) {
		if y+lh > b.Rect.Max.Y {
			return
		}
		rc := walk.Rectangle{X: b.Rect.Min.X + padX, Y: y, Width: innerW, Height: lh}
		_ = canvas.DrawTextPixels(text, font, clr, rc, walk.TextSingleLine|walk.TextTop|walk.TextWordEllipsis)
		y += lh
	}
	drawLine(b.Name, nameFont, nameLH, walk.RGB(18, 18, 22))
	// FS § Styles: Folder Details indent above 0.5 vu (vu = Folder Name font size in points).
	y += int(0.5*float64(namePt)*float64(dpi)/72.0 + 0.5)
	drawLine(format.ObjectSize(b.Size), metaFont, metaLH, walk.RGB(35, 35, 40))
	drawLine(fmtPercent(b.DriveShare), metaFont, metaLH, walk.RGB(55, 55, 62))
}

func (a *app) drawShabbyTooltip(canvas *walk.Canvas, bounds walk.Rectangle) {
	if a.hoverIdx < 0 || a.hoverIdx >= len(a.blocks) {
		return
	}
	b := a.blocks[a.hoverIdx]
	dpi := 96
	if a.chart != nil {
		dpi = a.chart.DPI()
	}
	if a.tileIsFancy(b, dpi, canvas) {
		return
	}
	font := a.ensureSmallFont()
	if font == nil {
		return
	}
	line1 := trimName(b.Name, 48)
	line2 := format.ObjectSize(b.Size)
	line3 := fmtPercent(b.DriveShare)
	metaFont := a.ensureLabelFont(maxInt(6, font.PointSize()-3))
	if metaFont == nil {
		metaFont = font
	}
	nameLH := textLineHeightPx(a.chart, font)
	metaLH := textLineHeightPx(a.chart, metaFont)
	bw := maxInt(200, minInt(400, 16+utf8.RuneCountInString(line1)*7))
	pad := 8
	bh := pad*2 + nameLH + 2*metaLH

	x := a.hoverX + 14
	y := a.hoverY + 12
	if x+bw > bounds.Width-8 {
		x = bounds.Width - bw - 8
	}
	if y+bh > bounds.Height-8 {
		y = bounds.Height - bh - 8
	}
	if x < 8 {
		x = 8
	}
	if y < 8 {
		y = 8
	}

	brush, _ := walk.NewSolidColorBrush(walk.RGB(255, 250, 205))
	if brush != nil {
		_ = canvas.FillRectanglePixels(brush, walk.Rectangle{X: x, Y: y, Width: bw, Height: bh})
		brush.Dispose()
	}
	pen, _ := walk.NewCosmeticPen(walk.PenSolid, walk.RGB(90, 80, 40))
	if pen != nil {
		_ = canvas.DrawRectanglePixels(pen, walk.Rectangle{X: x, Y: y, Width: bw, Height: bh})
		pen.Dispose()
	}

	yy := y + pad
	_ = canvas.DrawTextPixels(line1, font, walk.RGB(25, 22, 15), walk.Rectangle{X: x + pad, Y: yy, Width: bw - 2*pad, Height: nameLH}, walk.TextWordEllipsis|walk.TextSingleLine|walk.TextTop)
	yy += nameLH
	_ = canvas.DrawTextPixels(line2, metaFont, walk.RGB(35, 32, 22), walk.Rectangle{X: x + pad, Y: yy, Width: bw - 2*pad, Height: metaLH}, walk.TextSingleLine|walk.TextWordEllipsis|walk.TextTop)
	yy += metaLH
	_ = canvas.DrawTextPixels(line3, metaFont, walk.RGB(55, 50, 35), walk.Rectangle{X: x + pad, Y: yy, Width: bw - 2*pad, Height: metaLH}, walk.TextSingleLine|walk.TextWordEllipsis|walk.TextTop)
}

func (a *app) hitTest(x, y int) int {
	if a.chart == nil || len(a.blocks) == 0 {
		return -1
	}
	pt := image.Pt(x, y)
	for i, b := range a.blocks {
		if pt.In(b.Rect) {
			return i
		}
	}
	return -1
}

func cloneFolder(n model.FolderNode) model.FolderNode {
	c := n
	if len(n.Kids) > 0 {
		c.Kids = make([]model.FolderNode, len(n.Kids))
		for i := range n.Kids {
			c.Kids[i] = cloneFolder(n.Kids[i])
		}
	}
	return c
}

func textLineHeightPx(widget walk.Widget, font *walk.Font) int {
	dpi := 96
	if widget != nil {
		dpi = widget.DPI()
	}
	if font == nil {
		return 14
	}
	px := int(win.MulDiv(int32(font.PointSize()), int32(dpi), 72))
	if px < 8 {
		px = 8
	}
	return px + maxInt(4, px/4)
}

func (a *app) ensureSmallFont() *walk.Font {
	if a.smallFont != nil {
		return a.smallFont
	}
	f, err := walk.NewFont("Segoe UI", 8, 0)
	if err != nil {
		return nil
	}
	a.smallFont = f
	return a.smallFont
}

func (a *app) ensureLabelFont(px int) *walk.Font {
	if px < 6 {
		px = 6
	}
	if f, ok := a.labelFonts[px]; ok && f != nil {
		return f
	}
	f, err := walk.NewFont("Segoe UI", px, 0)
	if err != nil {
		return nil
	}
	a.labelFonts[px] = f
	return f
}

func fillRect(img *image.RGBA, r image.Rectangle, c color.RGBA) {
	r = r.Intersect(img.Rect)
	for y := r.Min.Y; y < r.Max.Y; y++ {
		for x := r.Min.X; x < r.Max.X; x++ {
			img.SetRGBA(x, y, c)
		}
	}
}

func strokeRect(img *image.RGBA, r image.Rectangle, c color.RGBA) {
	r = r.Intersect(img.Rect)
	if r.Empty() {
		return
	}
	for x := r.Min.X; x < r.Max.X; x++ {
		img.SetRGBA(x, r.Min.Y, c)
		img.SetRGBA(x, r.Max.Y-1, c)
	}
	for y := r.Min.Y; y < r.Max.Y; y++ {
		img.SetRGBA(r.Min.X, y, c)
		img.SetRGBA(r.Max.X-1, y, c)
	}
}

func fillBG(img *image.RGBA, c color.RGBA) {
	for y := 0; y < img.Rect.Dy(); y++ {
		for x := 0; x < img.Rect.Dx(); x++ {
			img.SetRGBA(x, y, c)
		}
	}
}

func fmtPercent(share float64) string {
	if share < 0 {
		share = 0
	}
	return fmt.Sprintf("%.1f%%", share*100)
}

func trimName(s string, max int) string {
	s = trimSpaceRunes(s)
	if utf8.RuneCountInString(s) <= max {
		return s
	}
	if max < 4 {
		rs := []rune(s)
		if len(rs) > max {
			return string(rs[:max])
		}
		return s
	}
	rs := []rune(s)
	if len(rs) <= max {
		return s
	}
	return string(rs[:max-3]) + "..."
}

func trimSpaceRunes(s string) string {
	// minimal trim
	for len(s) > 0 && (s[0] == ' ' || s[0] == '\t') {
		s = s[1:]
	}
	return s
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func absf(x float64) float64 {
	if x < 0 {
		return -x
	}
	return x
}
