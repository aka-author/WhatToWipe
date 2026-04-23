//go:build windows

package ui

import (
	"context"
	"fmt"
	"image"
	"image/color"
	"os"
	"os/exec"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"whatrwipe/win-go/internal/art"
	"whatrwipe/win-go/internal/config"
	"whatrwipe/win-go/internal/format"
	"whatrwipe/win-go/internal/layout"
	"whatrwipe/win-go/internal/model"
	"whatrwipe/win-go/internal/scan"
	"whatrwipe/win-go/internal/volume"
	"whatrwipe/win-go/internal/winver"
)

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
	updateMenu   *walk.Action
	stopMenu     *walk.Action
	openBtn      *walk.ToolButton
	upBtn        *walk.ToolButton
	manageBtn    *walk.ToolButton
	scanBtn      *walk.ToolButton
	volTotalLbl  *walk.Label
	volFreeBtn   *walk.PushButton
	aboutAction  *walk.Action

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

	labelFonts map[string]*walk.Font

	scanCtx    context.Context
	scanCancel context.CancelFunc
	scanSeq    uint64
	scanning   atomic.Bool

	pendingUpdateSnap *snap
	inspectHit        int

	tooltipsOnce sync.Once
	lastChartTip string

	treemapCfg config.Treemap

	// Throttle status bar path updates during scan (funcspec: scanning.updateInterval, not in config file).
	scanProgMu      sync.Mutex
	scanProgLatest  string
	scanProgShownAt time.Time
}

// Run starts the WhatToWipe main window (FS + techspec shell).
func Run() error {
	tc, cfgErr := config.LoadOrInitTreemap()
	if cfgErr != nil {
		tc = config.DefaultTreemap()
	}
	a := &app{
		inspectHit: -1,
		labelFonts: make(map[string]*walk.Font),
		treemapCfg: tc,
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
						Text:        "&Explore",
						Image:       a.manageBmp,
						Shortcut:    Shortcut{walk.ModControl, walk.KeyE},
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
			// Left margin 0: align command strip with treemap left edge (same vertical as chart).
			Layout: HBox{Margins: Margins{Left: 0, Top: 6, Right: 0, Bottom: 6}, Spacing: 8},
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
			// Avoid erasing the whole client each paint (reduces flicker when overlaying labels / tooltip on hover).
			ClearsBackground: false,
			PaintPixels:      a.paintTreemap,
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
				b := a.blocks[h]
				if b.Kind != model.TreemapItemFolder || b.IsEmpty {
					return
				}
				a.navPath = append(a.navPath, b.Path)
				a.rebuildTreemap()
			},
			OnMouseMove: func(x, y int, _ walk.MouseButton) {
				h := a.hitTest(x, y)
				if h < 0 || h >= len(a.blocks) {
					if a.chart != nil {
						a.chart.SetCursor(walk.CursorArrow())
					}
					a.setChartTooltip("")
					return
				}
				b := a.blocks[h]
				if a.chart != nil {
					a.chart.SetCursor(a.cursorForBlock(b))
				}
				if a.tileHasLabel(b) {
					a.setChartTooltip("")
					return
				}
				a.setChartTooltip(tileTooltipText(b))
			},
		},
	}
}

func (a *app) onInspectContext() {
	if a.inspectHit < 0 || a.inspectHit >= len(a.blocks) {
		return
	}
	b := a.blocks[a.inspectHit]
	p := b.Path
	if p == "" {
		if cur := a.resolveCurrent(); cur != nil {
			p = cur.Path
		}
	}
	if p == "" {
		return
	}
	if b.Kind == model.TreemapItemFile {
		if b.IsExecFile {
			return
		}
		a.openFileInDefaultApp(p)
		return
	}
	a.openInExplorer(p)
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

// noteScanProgress updates the status path at most every scanning.updateInterval (funcspec default 0.5 sec; not user-configurable).
func (a *app) noteScanProgress(scanID uint64, path string) {
	minGap := config.ScanPathStatusUpdate
	a.scanProgMu.Lock()
	a.scanProgLatest = path
	ready := a.scanProgShownAt.IsZero() || time.Since(a.scanProgShownAt) >= minGap
	a.scanProgMu.Unlock()
	if !ready {
		return
	}
	a.mw.Synchronize(func() {
		if scanID != atomic.LoadUint64(&a.scanSeq) {
			return
		}
		a.scanProgMu.Lock()
		a.scanProgShownAt = time.Now()
		msg := a.scanProgLatest
		a.scanProgMu.Unlock()
		a.setStatus(msg)
	})
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

func (a *app) openFileInDefaultApp(path string) {
	// Use explorer directly to avoid spawning a transient console window from cmd /c start.
	cmd := exec.Command("explorer", path)
	if err := cmd.Start(); err != nil {
		walk.MsgBox(a.mw, "WhatToWipe", "Could not open file:\n"+err.Error(), walk.MsgBoxIconError)
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

	a.scanProgMu.Lock()
	a.scanProgLatest = ""
	a.scanProgShownAt = time.Time{}
	a.scanProgMu.Unlock()

	go func(scanID uint64, root string, sk scanKind) {
		node, errCount, cancelled := scan.ScanTree(a.scanCtx, root, func(p string) {
			a.noteScanProgress(scanID, p)
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
	_ = a.volTotalLbl.SetText("Total at " + letter + " " + format.ObjectSize(int64(tot)))
	_ = a.volFreeBtn.SetText("Free at " + letter + " " + format.ObjectSize(int64(fr)))
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

	if len(cur.Kids) == 0 && len(cur.Files) == 0 {
		a.items = nil
		a.blocks = nil
		a.chartDirty = true
		if a.chart != nil {
			a.chart.Invalidate()
		}
		return
	}

	a.items = scan.BuildTreemapItems(cur, a.driveTotal, a.treemapCfg)
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
			dpi := a.chart.DPI()
			area := image.Rect(0, 0, bounds.Width, bounds.Height)
			minW := int(win.MulDiv(int32(a.treemapCfg.MinTileWidthPt), int32(dpi), 72))
			minH := int(win.MulDiv(int32(a.treemapCfg.MinTileHeightPt), int32(dpi), 72))
			a.blocks = a.blocksForViewport(area, minW, minH)
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
	// Full opaque background each frame so ClearsBackground=false does not leave shabby-tooltip ghosts.
	bg, _ := walk.NewSolidColorBrush(walk.RGB(250, 250, 252))
	if bg != nil {
		_ = canvas.FillRectanglePixels(bg, bounds)
		bg.Dispose()
	}
	if err := canvas.DrawImagePixels(a.chartBmp, walk.Point{}); err != nil {
		return err
	}

	a.drawBlockLabels(canvas)
	return nil
}

func (a *app) drawBlockLabels(canvas *walk.Canvas) {
	for _, b := range a.blocks {
		a.drawTileLabelAuto(canvas, b)
	}
}

type labelMode int

const (
	labelModeHidden labelMode = iota
	labelModeHorizWithDetails
	labelModeHorizNoDetails
	labelModeVertWithDetails
	labelModeVertNoDetails
)

type labelLayout struct {
	inner              image.Rectangle
	nameFont, metaFont *walk.Font
	nameLH, metaLH     int
	gap                int
	shareText          string
	showShare          bool
	contentW, contentH int
}

func (a *app) tileLabelPolicy(b model.BlockLayout) (labelMode, model.BlockLayout, int) {
	b = a.withExternalRect(b)
	if b.Rect.Empty() {
		return labelModeHidden, b, 0
	}
	maxPt := a.treemapCfg.HeadingMaxFontSizePt
	minPt := a.treemapCfg.HeadingMinFontSizePt
	if maxPt <= 0 {
		maxPt = 30
	}
	if minPt <= 0 {
		minPt = 7
	}
	if minPt > maxPt {
		minPt = maxPt
	}
	for pt := maxPt; pt >= minPt; pt-- {
		if a.tileLabelFits(b, pt, false, true) {
			return labelModeHorizWithDetails, b, pt
		}
	}
	for pt := maxPt; pt >= minPt; pt-- {
		if a.tileLabelFits(b, pt, false, false) {
			return labelModeHorizNoDetails, b, pt
		}
	}
	for pt := maxPt; pt >= minPt; pt-- {
		if a.tileLabelFits(b, pt, true, true) {
			return labelModeVertWithDetails, b, pt
		}
	}
	for pt := maxPt; pt >= minPt; pt-- {
		if a.tileLabelFits(b, pt, true, false) {
			return labelModeVertNoDetails, b, pt
		}
	}
	return labelModeHidden, b, minPt
}

func (a *app) tileHasLabel(b model.BlockLayout) bool {
	mode, _, _ := a.tileLabelPolicy(b)
	return mode != labelModeHidden
}

func tileTooltipText(b model.BlockLayout) string {
	// funcspec: Name + Size only
	return fmt.Sprintf("%s\n%s", b.Name, format.ObjectSize(b.Size))
}

func (a *app) setChartTooltip(text string) {
	if a.chart == nil {
		return
	}
	if text == a.lastChartTip {
		return
	}
	_ = a.chart.SetToolTipText(text)
	a.lastChartTip = text
}

func (a *app) minTilePx() (int, int) {
	dpi := 96
	if a.chart != nil {
		dpi = a.chart.DPI()
	}
	minW := int(win.MulDiv(int32(a.treemapCfg.MinTileWidthPt), int32(dpi), 72))
	minH := int(win.MulDiv(int32(a.treemapCfg.MinTileHeightPt), int32(dpi), 72))
	if minW < 1 {
		minW = 1
	}
	if minH < 1 {
		minH = 1
	}
	return minW, minH
}

func (a *app) tilePaddingPx(rotated bool) (left, top, right, bottom int) {
	dpi := 96
	if a.chart != nil {
		dpi = a.chart.DPI()
	}
	nz := func(v, def int) int {
		if v <= 0 {
			return def
		}
		return v
	}
	left = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingLeftPt, 10)), int32(dpi), 72))
	// FS Padding and Clipping:
	// - Horizontal label: left/top paddings are configured, right/bottom are 0.
	// - Vertical label: left/bottom paddings are configured, right/top are 0.
	if rotated {
		top = 0
		right = 0
		bottom = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingBottomPt, 10)), int32(dpi), 72))
	} else {
		top = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingTopPt, 10)), int32(dpi), 72))
		right = 0
		bottom = 0
	}
	return left, top, right, bottom
}

func (a *app) cursorForBlock(b model.BlockLayout) walk.Cursor {
	switch b.Kind {
	case model.TreemapItemFolder:
		if b.IsEmpty {
			return walk.CursorNo()
		}
		return walk.CursorHand()
	case model.TreemapItemFile:
		if b.IsExecFile {
			return walk.CursorNo()
		}
		return walk.CursorHand()
	default:
		return walk.CursorArrow()
	}
}

func (a *app) externalTileRect(b model.BlockLayout) image.Rectangle {
	if a.chart == nil {
		return b.Rect
	}
	bounds := a.chart.ClientBoundsPixels()
	if bounds.Width <= 0 || bounds.Height <= 0 {
		return b.Rect
	}
	chartRect := image.Rect(0, 0, bounds.Width, bounds.Height)
	return b.Rect.Intersect(chartRect)
}

func (a *app) withExternalRect(b model.BlockLayout) model.BlockLayout {
	b.Rect = a.externalTileRect(b)
	return b
}

func (a *app) drawTileLabelAuto(canvas *walk.Canvas, b model.BlockLayout) {
	mode, vb, pt := a.tileLabelPolicy(b)
	if mode == labelModeHidden {
		return
	}
	switch mode {
	case labelModeHorizWithDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), pt, false, true)
	case labelModeHorizNoDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), pt, false, false)
	case labelModeVertWithDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), pt, true, true)
	case labelModeVertNoDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), pt, true, false)
	}
}

// drawTreemapTileLabel draws a tile label in horizontal or rotated orientation.
func (a *app) drawTreemapTileLabel(canvas *walk.Canvas, b model.BlockLayout, dpi, namePt int, rotated, withDetails bool) {
	lay, ok := a.computeLabelLayout(b, dpi, namePt, rotated, withDetails)
	if !ok {
		return
	}
	fg := rgbaToWalkColor(b.TextColor)
	if !rotated {
		y := lay.inner.Min.Y
		drawLine := func(text string, font *walk.Font, lh int, clr walk.Color) {
			rc := walk.Rectangle{X: lay.inner.Min.X, Y: y, Width: lay.inner.Dx(), Height: lh}
			_ = canvas.DrawTextPixels(text, font, clr, rc, walk.TextSingleLine|walk.TextTop)
			y += lh
		}
		drawLine(b.Name, lay.nameFont, lay.nameLH, fg)
		if withDetails {
			y += lay.gap
			drawLine(format.ObjectSize(b.Size), lay.metaFont, lay.metaLH, fg)
			if lay.showShare {
				drawLine(lay.shareText, lay.metaFont, lay.metaLH, fg)
			}
		}
		return
	}
	// Render horizontal label with transparent background, then rotate to satisfy vertical-orientation rule.
	w := max(lay.contentW, 1)
	h := max(lay.contentH, 1)
	tmp, err := walk.NewBitmapWithTransparentPixelsForDPI(walk.Size{Width: w, Height: h}, dpi)
	if err != nil {
		return
	}
	defer tmp.Dispose()
	tmpCanvas, err := walk.NewCanvasFromImage(tmp)
	if err != nil {
		return
	}
	defer tmpCanvas.Dispose()
	ty := 0
	drawTmp := func(text string, font *walk.Font, lh int) {
		rc := walk.Rectangle{X: 0, Y: ty, Width: w, Height: lh}
		_ = tmpCanvas.DrawTextPixels(text, font, fg, rc, walk.TextSingleLine|walk.TextTop)
		ty += lh
	}
	drawTmp(b.Name, lay.nameFont, lay.nameLH)
	if withDetails {
		ty += lay.gap
		drawTmp(format.ObjectSize(b.Size), lay.metaFont, lay.metaLH)
		if lay.showShare {
			drawTmp(lay.shareText, lay.metaFont, lay.metaLH)
		}
	}
	im, err := tmp.ToImage()
	if err != nil {
		return
	}
	rot := rotateRGBA90CCW(im)
	rotBmp, err := walk.NewBitmapFromImageForDPI(rot, dpi)
	if err != nil {
		return
	}
	defer rotBmp.Dispose()
	rw, rh := rot.Rect.Dx(), rot.Rect.Dy()
	x := lay.inner.Min.X
	y := lay.inner.Max.Y - rh
	if y < lay.inner.Min.Y {
		y = lay.inner.Min.Y
	}
	_ = canvas.DrawImagePixels(rotBmp, walk.Point{X: x, Y: y})
}

func rotateRGBA90CCW(src *image.RGBA) *image.RGBA {
	b := src.Bounds()
	w, h := b.Dx(), b.Dy()
	dst := image.NewRGBA(image.Rect(0, 0, h, w))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			dst.Set(y, w-1-x, src.RGBAAt(x+b.Min.X, y+b.Min.Y))
		}
	}
	return dst
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func (a *app) tileLabelFits(b model.BlockLayout, headingPt int, rotated, withDetails bool) bool {
	if a.chart == nil {
		return false
	}
	lay, ok := a.computeLabelLayout(b, a.chart.DPI(), headingPt, rotated, withDetails)
	if !ok {
		return false
	}
	innerW := lay.inner.Dx()
	innerH := lay.inner.Dy()
	if rotated {
		return lay.contentH <= innerW && lay.contentW <= innerH
	}
	return lay.contentW <= innerW && lay.contentH <= innerH
}

func (a *app) computeLabelLayout(b model.BlockLayout, dpi, namePt int, rotated, withDetails bool) (labelLayout, bool) {
	var lay labelLayout
	metaPt := int(float64(namePt)*a.ratioOr(a.treemapCfg.DetailsFontSizeRatio, 0.8) + 0.5)
	if metaPt < 1 {
		metaPt = 1
	}
	lay.nameFont = a.ensureLabelFont(namePt)
	lay.metaFont = a.ensureLabelFont(metaPt)
	if lay.nameFont == nil || lay.metaFont == nil {
		return lay, false
	}
	padL, padT, padR, padB := a.tilePaddingPx(rotated)
	lay.inner = image.Rect(b.Rect.Min.X+padL, b.Rect.Min.Y+padT, b.Rect.Max.X-padR, b.Rect.Max.Y-padB)
	if lay.inner.Empty() {
		return lay, false
	}
	lay.nameLH = int(float64(namePt)*a.ratioOr(a.treemapCfg.HeadingLineHeight, 1.2)*float64(dpi)/72.0 + 0.5)
	lay.metaLH = int(float64(metaPt)*a.ratioOr(a.treemapCfg.DetailsLineHeight, 1.5)*float64(dpi)/72.0 + 0.5)
	if h, ok := measureTextHeightPx(a.chart, "Agjpyq", lay.nameFont); ok && h > 0 && lay.nameLH < h+2 {
		lay.nameLH = h + 2
	}
	if h, ok := measureTextHeightPx(a.chart, "Agjpyq", lay.metaFont); ok && h > 0 && lay.metaLH < h+2 {
		lay.metaLH = h + 2
	}
	lay.gap = int(float64(metaPt)*a.ratioOr(a.treemapCfg.AboveDetailsRatio, 1.0)*float64(dpi)/72.0 + 0.5)
	lay.shareText, lay.showShare = formatShareLine(b.DriveShare)
	if !withDetails {
		lay.showShare = false
	}
	nameW, ok := measureTextWidthPx(a.chart, b.Name, lay.nameFont)
	if !ok {
		return lay, false
	}
	lay.contentW = nameW
	lay.contentH = lay.nameLH
	if withDetails {
		sizeW, ok := measureTextWidthPx(a.chart, format.ObjectSize(b.Size), lay.metaFont)
		if !ok {
			return lay, false
		}
		if sizeW > lay.contentW {
			lay.contentW = sizeW
		}
		lay.contentH += lay.gap + lay.metaLH
		if lay.showShare {
			shareW, ok := measureTextWidthPx(a.chart, lay.shareText, lay.metaFont)
			if !ok {
				return lay, false
			}
			if shareW > lay.contentW {
				lay.contentW = shareW
			}
			lay.contentH += lay.metaLH
		}
	}
	return lay, true
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
	if len(n.Files) > 0 {
		c.Files = append([]model.FileEntry(nil), n.Files...)
	}
	if len(n.Kids) > 0 {
		c.Kids = make([]model.FolderNode, len(n.Kids))
		for i := range n.Kids {
			c.Kids[i] = cloneFolder(n.Kids[i])
		}
	}
	return c
}

func (a *app) ensureLabelFont(px int) *walk.Font {
	if px < 6 {
		px = 6
	}
	face := a.treemapCfg.TileFontName
	if face == "" {
		face = "Segoe UI"
	}
	key := fmt.Sprintf("%s:%d", face, px)
	if f, ok := a.labelFonts[key]; ok && f != nil {
		return f
	}
	f, err := walk.NewFont(face, px, 0)
	if err != nil {
		return nil
	}
	a.labelFonts[key] = f
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

func (a *app) blocksForViewport(area image.Rectangle, minW, minH int) []model.BlockLayout {
	nonClumps, baseClump := a.viewportItems()
	if len(nonClumps) == 0 && baseClump == nil {
		return nil
	}
	if minW < 1 {
		minW = 1
	}
	if minH < 1 {
		minH = 1
	}
	// Recompute full viewport state every paint so resize always rebuilds tile/clump balance.
	targetSlots := len(a.items)
	if targetSlots <= 0 {
		targetSlots = len(nonClumps)
		if baseClump != nil {
			targetSlots++
		}
	}
	viewportCapacity := (area.Dx() / minW) * (area.Dy() / minH)
	if viewportCapacity < 1 {
		viewportCapacity = 1
	}
	if targetSlots > viewportCapacity {
		targetSlots = viewportCapacity
	}
	if targetSlots <= 0 {
		return nil
	}
	clumpNeeded := baseClump != nil || len(nonClumps) > targetSlots
	nonSlots := targetSlots
	if clumpNeeded {
		nonSlots--
	}
	if nonSlots < 0 {
		nonSlots = 0
	}
	if nonSlots > len(nonClumps) {
		nonSlots = len(nonClumps)
	}

	visibleNon := append([]model.TreeItem(nil), nonClumps[:nonSlots]...)
	hidden := append([]model.TreeItem(nil), nonClumps[nonSlots:]...)
	clump := a.aggregateClump(hidden, baseClump)

	renderItems := make([]model.TreeItem, 0, len(visibleNon)+1)
	renderItems = append(renderItems, visibleNon...)
	if clump != nil {
		renderItems = append(renderItems, *clump)
	}
	if len(renderItems) == 0 {
		return nil
	}

	blocks := layout.Squarified(renderItems, area, minW, minH)
	if clump == nil {
		return blocks
	}
	br := bottomRightBlockIndex(blocks, area)
	clumpIdx := indexOfClumpBlock(blocks)
	if br >= 0 && clumpIdx >= 0 && br != clumpIdx {
		host := treeItemFromBlock(blocks[br])
		blocks[br] = blockFromTreeItem(*clump, blocks[br].Rect)
		blocks[clumpIdx] = blockFromTreeItem(host, blocks[clumpIdx].Rect)
	}
	return blocks
}

func (a *app) viewportItems() ([]model.TreeItem, *model.TreeItem) {
	var baseClump *model.TreeItem
	nonClumps := make([]model.TreeItem, 0, len(a.items))
	for _, it := range a.items {
		if it.Kind != model.TreemapItemClump {
			nonClumps = append(nonClumps, it)
			continue
		}
		if baseClump == nil {
			c := it
			baseClump = &c
		} else {
			baseClump.Size += it.Size
			baseClump.DriveShare += it.DriveShare
		}
	}
	return nonClumps, baseClump
}

func (a *app) aggregateClump(hidden []model.TreeItem, baseClump *model.TreeItem) *model.TreeItem {
	if baseClump == nil && len(hidden) == 0 {
		return nil
	}
	clump := model.TreeItem{
		Name:      "Other",
		Kind:      model.TreemapItemClump,
		Color:     a.treemapCfg.NativeClumpBg,
		TextColor: a.treemapCfg.NativeClumpText,
	}
	if baseClump != nil {
		clump = *baseClump
	}
	if clump.Name == "" {
		clump.Name = "Other"
	}
	clump.Kind = model.TreemapItemClump
	clump.Path = ""
	clump.IsNode = false
	for _, it := range hidden {
		clump.Size += it.Size
		clump.DriveShare += it.DriveShare
		if isPackedVisual(it, a.treemapCfg) {
			clump.Color = a.treemapCfg.PackedClumpBg
			clump.TextColor = a.treemapCfg.PackedClumpText
		}
	}
	return &clump
}

func bottomRightBlockIndex(blocks []model.BlockLayout, area image.Rectangle) int {
	if len(blocks) == 0 {
		return -1
	}
	corner := image.Pt(area.Max.X-1, area.Max.Y-1)
	for i, b := range blocks {
		if corner.In(b.Rect) {
			return i
		}
	}
	best := 0
	for i := 1; i < len(blocks); i++ {
		bi, bb := blocks[i], blocks[best]
		if bi.Rect.Max.Y > bb.Rect.Max.Y || (bi.Rect.Max.Y == bb.Rect.Max.Y && bi.Rect.Max.X > bb.Rect.Max.X) {
			best = i
		}
	}
	return best
}

func indexOfClumpBlock(blocks []model.BlockLayout) int {
	for i, b := range blocks {
		if b.Kind == model.TreemapItemClump {
			return i
		}
	}
	return -1
}

func treeItemFromBlock(b model.BlockLayout) model.TreeItem {
	return model.TreeItem{
		Name:       b.Name,
		Path:       b.Path,
		Size:       b.Size,
		Color:      b.Color,
		TextColor:  b.TextColor,
		IsNode:     b.IsNode,
		IsEmpty:    b.IsEmpty,
		IsExecFile: b.IsExecFile,
		DriveShare: b.DriveShare,
		Kind:       b.Kind,
	}
}

func blockFromTreeItem(it model.TreeItem, r image.Rectangle) model.BlockLayout {
	return model.BlockLayout{
		Name:       it.Name,
		Path:       it.Path,
		Size:       it.Size,
		Rect:       r,
		Color:      it.Color,
		TextColor:  it.TextColor,
		IsNode:     it.IsNode,
		IsEmpty:    it.IsEmpty,
		IsExecFile: it.IsExecFile,
		DriveShare: it.DriveShare,
		Kind:       it.Kind,
	}
}

func isPackedVisual(it model.TreeItem, cfg config.Treemap) bool {
	return (it.Color == cfg.PackedFileBg && it.TextColor == cfg.PackedFileText) ||
		(it.Color == cfg.PackedFolderBg && it.TextColor == cfg.PackedFolderText) ||
		(it.Color == cfg.PackedClumpBg && it.TextColor == cfg.PackedClumpText)
}

func formatShareLine(share float64) (string, bool) {
	if share < 0 {
		share = 0
	}
	pct := share * 100
	s1 := fmt.Sprintf("%.1f", pct)
	if s1 == "0.0" {
		s2 := fmt.Sprintf("%.2f", pct)
		if s2 == "0.00" {
			return "", false
		}
		return s2 + "%", true
	}
	s1 = strings.TrimSuffix(s1, ".0")
	return s1 + "%", true
}

func (a *app) ratioOr(v, def float64) float64 {
	if v <= 0 {
		return def
	}
	return v
}

func measureTextWidthPx(widget walk.Widget, text string, font *walk.Font) (int, bool) {
	if widget == nil || font == nil {
		return 0, false
	}
	c, err := widget.CreateCanvas()
	if err != nil {
		return 0, false
	}
	defer c.Dispose()
	b, _, err := c.MeasureTextPixels(text, font, walk.Rectangle{Width: 4000, Height: 400}, walk.TextCalcRect|walk.TextSingleLine)
	if err != nil {
		return 0, false
	}
	return b.Width, true
}

func measureTextHeightPx(widget walk.Widget, text string, font *walk.Font) (int, bool) {
	if widget == nil || font == nil {
		return 0, false
	}
	c, err := widget.CreateCanvas()
	if err != nil {
		return 0, false
	}
	defer c.Dispose()
	b, _, err := c.MeasureTextPixels(text, font, walk.Rectangle{Width: 4000, Height: 400}, walk.TextCalcRect|walk.TextSingleLine)
	if err != nil {
		return 0, false
	}
	return b.Height, true
}

func rgbaToWalkColor(c color.RGBA) walk.Color {
	return walk.RGB(byte(c.R), byte(c.G), byte(c.B))
}
