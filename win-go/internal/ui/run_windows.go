//go:build windows

package ui

import (
	"context"
	"encoding/json"
	"fmt"
	"image"
	"image/color"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"eraserewrite/win-go/internal/art"
	"eraserewrite/win-go/internal/config"
	"eraserewrite/win-go/internal/format"
	"eraserewrite/win-go/internal/layout"
	"eraserewrite/win-go/internal/model"
	"eraserewrite/win-go/internal/scan"
	"eraserewrite/win-go/internal/volume"
	"eraserewrite/win-go/internal/winver"
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
	volFreeLbl   *walk.Label
	volFreeBtn   *walk.PushButton
	aboutAction  *walk.Action
	settingsAction *walk.Action

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

	labelFonts  map[string]*walk.Font
	labelCache  []labelChoice
	labelSolved []bool
	// Cache key for label resolution (depends on tile geometry, viewport, and DPI).
	labelCacheW, labelCacheH, labelCacheDPI int
	labelCacheValid                         bool
	labelSolveNext                          int
	labelSolving                            bool
	labelSolveSeq                           uint64
	measureCanvas                           *walk.Canvas
	textMeasureCache                        map[string]int

	scanCtx    context.Context
	scanCancel context.CancelFunc
	scanSeq    uint64
	scanning   atomic.Bool
	scanCursorPrev  walk.Cursor
	scanCursorSaved bool

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

// Run starts the Erase & Rewrite main window (FS + techspec shell).
func Run() error {
	tc, cfgErr := config.LoadOrInitTreemap()
	if cfgErr != nil {
		tc = config.DefaultTreemap()
	}
	a := &app{
		inspectHit:       -1,
		labelFonts:       make(map[string]*walk.Font),
		textMeasureCache: make(map[string]int),
		treemapCfg:       tc,
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
		Title:    "Erase & Rewrite",
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
						Text:        "&Open a FolderРІР‚В¦",
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
						Text:        "&Explore\u2026",
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
				Text: "&Tools",
				Items: []MenuItem{
					Action{
						AssignTo:    &a.settingsAction,
						Text:        "&Settings\u2026",
						OnTriggered: a.onSettings,
					},
				},
			},
			Menu{
				Text: "&Help",
				Items: []MenuItem{
					Action{
						AssignTo:    &a.aboutAction,
						Text:        "&About\u2026",
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
					Text:        "Total at -: -",
					MaxSize:     Size{Width: 520, Height: 0},
					ToolTipText: "Total capacity of the volume",
				},
				Composite{
					Layout: HBox{Margins: Margins{}, Spacing: 4},
					Children: []Widget{
						Label{
							AssignTo:    &a.volFreeLbl,
							Text:        "Free at -:",
							ToolTipText: "Free space on the volume",
						},
						PushButton{
							AssignTo:    &a.volFreeBtn,
							Text:        "-",
							Enabled:     false,
							ToolTipText: "Free space on the volume. Click to refresh.",
							OnClicked: func() {
								a.onVolBarFreeRefresh()
							},
						},
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
					a.runTreemapContextMenu(x, y)
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
				if a.scanning.Load() || a.labelSolving {
					if a.chart != nil {
						a.chart.SetCursor(walk.CursorWait())
					}
					a.setChartTooltip("")
					return
				}
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
				if a.tileNeedsTooltipAt(h) {
					a.setChartTooltip(tileTooltipText(b))
					return
				}
				a.setChartTooltip("")
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
		a.openAssociatedOrAlert(p)
		return
	}
	a.explorePathInShell(p, false)
}

func (a *app) runTreemapContextMenu(clientX, clientY int) {
	if a.chart == nil || a.scanning.Load() || !a.treemapComplete {
		a.inspectHit = -1
		return
	}
	h := a.hitTest(clientX, clientY)
	a.inspectHit = h
	if h < 0 || h >= len(a.blocks) {
		return
	}
	b := a.blocks[h]

	menu, err := walk.NewMenu()
	if err != nil {
		return
	}
	defer menu.Dispose()

	switch b.Kind {
	case model.TreemapItemFolder:
		act := walk.NewAction()
		_ = act.SetText("Explore\u2026")
		act.Triggered().Attach(func() { a.onInspectContext() })
		if err := menu.Actions().Add(act); err != nil {
			return
		}
	case model.TreemapItemFile:
		act := walk.NewAction()
		_ = act.SetText("Open\u2026")
		_ = act.SetEnabled(!b.IsExecFile)
		act.Triggered().Attach(func() {
			if b.IsExecFile {
				return
			}
			a.onInspectContext()
		})
		if err := menu.Actions().Add(act); err != nil {
			return
		}
	default:
		return
	}
	menu.RunAtClient(a.chart, clientX, clientY)
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
	a.explorePathInShell(cur.Path, true)
}

func (a *app) onAbout() {
	ver := appVersionDotted()
	showAboutDialog(a.mw, ver)
}

func (a *app) onSettings() {
	showMinimalTestDialog(a.mw)
}

func appVersionDotted() string {
	const fallback = "0.0.0.0"
	exe, err := os.Executable()
	if err != nil {
		exe = ""
	}
	if exe != "" {
		if v, e := winver.FileVersionDotted(exe); e == nil {
			return v
		}
	}
	if v, e := versionFromVersionInfoJSON(exe); e == nil && v != "" {
		return v
	}
	return fallback
}

func versionFromVersionInfoJSON(exe string) (string, error) {
	type versionInfo struct {
		StringFileInfo struct {
			FileVersion string `json:"FileVersion"`
		} `json:"StringFileInfo"`
	}
	candidates := []string{}
	if exe != "" {
		candidates = append(candidates,
			filepath.Join(filepath.Dir(exe), "versioninfo.json"),
			filepath.Join(filepath.Dir(exe), "..", "versioninfo.json"),
		)
	}
	candidates = append(candidates, filepath.Join("win-go", "versioninfo.json"))
	for _, p := range candidates {
		b, err := os.ReadFile(filepath.Clean(p))
		if err != nil {
			continue
		}
		var vi versionInfo
		if err := json.Unmarshal(b, &vi); err != nil {
			continue
		}
		if vi.StringFileInfo.FileVersion != "" {
			return vi.StringFileInfo.FileVersion, nil
		}
	}
	return "", os.ErrNotExist
}

func (a *app) unsetTreemapToInitial() {
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
	a.invalidateLabelCache()
	if a.chart != nil {
		a.chart.Invalidate()
	}
	a.setStatus("Choose a target folder")
	a.refreshVolumeToolbar()
	a.setScanChrome(a.scanning.Load())
}

func (a *app) busyPointerTwoSeconds() {
	if a.mw == nil {
		return
	}
	prev := a.mw.Cursor()
	a.mw.SetCursor(walk.CursorWait())
	time.AfterFunc(2*time.Second, func() {
		if a.mw == nil {
			return
		}
		a.mw.Synchronize(func() {
			if prev != nil {
				a.mw.SetCursor(prev)
			} else {
				a.mw.SetCursor(nil)
			}
		})
	})
}

// explorePathInShell implements *Checking a Folder of Interest* then opening the folder in the
// shell (funcspec: Exploring the Context Folder / Exploring a Subfolder). unsetOnFailure mirrors
// negative outcome for Inspect РІвЂ вЂ™ Explore (treemap unset); tile Explore leaves the treemap unchanged.
func (a *app) explorePathInShell(path string, unsetTreemapOnFailure bool) {
	if a.mw == nil {
		return
	}
	path = filepath.Clean(path)
	fi, err := os.Stat(path)
	if err != nil || !fi.IsDir() {
		a.showErrorAlert("004", SpecErr004Message(path))
		if unsetTreemapOnFailure {
			a.unsetTreemapToInitial()
		}
		return
	}
	cmd := exec.Command("explorer", path)
	if err := cmd.Start(); err != nil {
		a.showErrorAlert("003", specErr003)
		if unsetTreemapOnFailure {
			a.unsetTreemapToInitial()
		}
		return
	}
	a.busyPointerTwoSeconds()
}

// openAssociatedOrAlert opens a file via the default association (explorer hand-off on Windows).
func (a *app) openAssociatedOrAlert(path string) {
	if a.mw == nil {
		return
	}
	path = filepath.Clean(path)
	fi, err := os.Stat(path)
	if err != nil {
		if os.IsNotExist(err) {
			a.showErrorAlert("004", SpecErr004Message(path))
		} else {
			a.showErrorAlert("003", specErr003)
		}
		return
	}
	if fi.IsDir() {
		a.showErrorAlert("003", specErr003)
		return
	}
	cmd := exec.Command("explorer", path)
	if err := cmd.Start(); err != nil {
		a.showErrorAlert("003", specErr003)
		return
	}
	a.busyPointerTwoSeconds()
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
					a.showInterruptionAlert(specInterruptionScanningInterrupted)
					return
				}
				if sk == scanOpen || sk == scanUpdate {
					a.showInterruptionAlert(specInterruptionScanningInterrupted)
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
					a.showErrorAlert("001", specErr001)
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
				a.showErrorAlert("001", specErr001)
				return
			}

			if sk == scanOpen && a.targetPath != "" {
				if _, err := os.Stat(a.targetPath); err != nil && os.IsNotExist(err) {
					a.showErrorAlert("004", SpecErr004Message(a.targetPath))
					a.unsetTreemapToInitial()
					return
				}
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
	busyCursor := scanning || a.labelSolving
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
		// FS: unavailable commands disabled РІР‚вЂќ Update only after a successful target scan.
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
	if a.chart != nil {
		if busyCursor {
			a.chart.SetCursor(walk.CursorWait())
		} else {
			a.chart.SetCursor(walk.CursorArrow())
		}
	}
	if a.mw != nil {
		if busyCursor {
			if !a.scanCursorSaved {
				a.scanCursorPrev = a.mw.Cursor()
				a.scanCursorSaved = true
			}
			a.mw.SetCursor(walk.CursorWait())
		} else if a.scanCursorSaved {
			if a.scanCursorPrev != nil {
				a.mw.SetCursor(a.scanCursorPrev)
			} else {
				a.mw.SetCursor(nil)
			}
			a.scanCursorPrev = nil
			a.scanCursorSaved = false
		}
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
		_ = a.volTotalLbl.SetText("Total at -: -")
		if a.volFreeLbl != nil {
			_ = a.volFreeLbl.SetText("Free at -:")
		}
		_ = a.volFreeBtn.SetText("-")
		return
	}
	letter := normalizeDriveIndicatorToken(volume.DriveLabel(a.volBarRoot))
	tot, fr, err := volume.DiskSpace(a.volBarRoot)
	if err != nil {
		_ = a.volTotalLbl.SetText("Total at " + letter + ": -")
		if a.volFreeLbl != nil {
			_ = a.volFreeLbl.SetText("Free at " + letter + ":")
		}
		_ = a.volFreeBtn.SetText("-")
		return
	}
	a.driveTotal, a.driveFree = tot, fr
	_ = a.volTotalLbl.SetText("Total at " + letter + ": " + format.ObjectSize(int64(tot)))
	if a.volFreeLbl != nil {
		_ = a.volFreeLbl.SetText("Free at " + letter + ":")
	}
	_ = a.volFreeBtn.SetText(format.ObjectSize(int64(fr)))
}

func normalizeDriveIndicatorToken(s string) string {
	s = strings.TrimSpace(s)
	if s == "" {
		return "?"
	}
	// Prevent duplicated colon in labels like "Total at C::".
	s = strings.TrimRight(s, ":")
	if s == "" {
		return "?"
	}
	return s
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
		a.invalidateLabelCache()
		a.chartDirty = true
		if a.chart != nil {
			a.chart.Invalidate()
		}
		return
	}

	if len(cur.Kids) == 0 && len(cur.Files) == 0 {
		a.items = nil
		a.blocks = nil
		a.invalidateLabelCache()
		a.chartDirty = true
		if a.chart != nil {
			a.chart.Invalidate()
		}
		return
	}

	a.items = scan.BuildTreemapItems(cur, a.driveTotal, a.treemapCfg)
	a.blocks = nil
	a.invalidateLabelCache()
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
			a.invalidateLabelCache()
			for _, b := range a.blocks {
				fillRect(img, b.Rect, b.Color)
				strokeRect(img, b.Rect, color.RGBA{40, 40, 45, 255})
			}
		} else {
			a.blocks = nil
			a.invalidateLabelCache()
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
	a.ensureLabelSolveInitialized()
	for i, b := range a.blocks {
		if i < 0 || i >= len(a.labelCache) || i >= len(a.labelSolved) || !a.labelSolved[i] {
			continue
		}
		a.drawTileLabelAuto(canvas, b, a.labelCache[i])
	}
}

type labelMode int

const (
	labelModeHidden labelMode = iota
	labelModeHorizWithDetails
	labelModeHorizNoDetails
	labelModeHorizWithDetailsShort
	labelModeHorizNoDetailsShort
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

type labelChoice struct {
	mode        labelMode
	heading     string
	pt          int
	withDetails bool
}

func (a *app) invalidateLabelCache() {
	atomic.AddUint64(&a.labelSolveSeq, 1)
	a.labelCache = nil
	a.labelSolved = nil
	a.labelCacheValid = false
	a.labelCacheW, a.labelCacheH, a.labelCacheDPI = 0, 0, 0
	a.labelSolveNext = 0
	a.labelSolving = false
	a.textMeasureCache = make(map[string]int)
}

func (a *app) resolveTileLabel(b model.BlockLayout) labelChoice {
	b = a.withExternalRect(b)
	if b.Rect.Empty() {
		return labelChoice{mode: labelModeHidden}
	}
	maxPt := a.treemapCfg.HeadingMaxFontSizePt
	minPt := a.treemapCfg.HeadingMinFontSizePt
	if maxPt <= 0 {
		maxPt = 30
	}
	if minPt <= 0 {
		minPt = 8
	}
	if minPt > maxPt {
		minPt = maxPt
	}
	// Simplified flow:
	// 1) binary-search full form over font sizes
	// 2) if full form fails, binary-search shortening at smallest font
	// 3) if detailed shortening fails, binary-search shortening in brief form at smallest font
	// 4) if still fails, show dummy
	if pt, ok := a.findBestFontSizeForMode(b, b.Name, minPt, maxPt, true); ok {
		return labelChoice{mode: labelModeHorizWithDetails, heading: b.Name, pt: pt, withDetails: true}
	}
	if heading, ok := a.findBestShortenedHeadingAtMinFont(b, b.Name, minPt, true); ok {
		return labelChoice{mode: labelModeHorizWithDetailsShort, heading: heading, pt: minPt, withDetails: true}
	}
	if a.tileLabelFits(b, b.Name, minPt, false) {
		return labelChoice{mode: labelModeHorizNoDetails, heading: b.Name, pt: minPt, withDetails: false}
	}
	if heading, ok := a.findBestShortenedHeadingAtMinFont(b, b.Name, minPt, false); ok {
		return labelChoice{mode: labelModeHorizNoDetailsShort, heading: heading, pt: minPt, withDetails: false}
	}
	return labelChoice{mode: labelModeHidden, pt: minPt}
}

// findBestFontSizeForMode finds the largest fitting font for a fixed heading/mode.
// It uses bounded binary search so label resolution time is predictable per tile.
func (a *app) findBestFontSizeForMode(b model.BlockLayout, heading string, minPt, maxPt int, withDetails bool) (int, bool) {
	if minPt <= 0 || maxPt <= 0 || minPt > maxPt {
		return 0, false
	}
	if !a.tileLabelFits(b, heading, minPt, withDetails) {
		return 0, false
	}
	if a.tileLabelFits(b, heading, maxPt, withDetails) {
		return maxPt, true
	}
	lo, hi := minPt, maxPt
	best := minPt
	for lo <= hi {
		mid := lo + (hi-lo)/2
		if a.tileLabelFits(b, heading, mid, withDetails) {
			best = mid
			lo = mid + 1
		} else {
			hi = mid - 1
		}
	}
	return best, true
}

// findBestShortenedHeadingAtMinFont finds the least-shortened heading that still fits at minPt.
// It checks candidates from best quality (least shortened) to worst quality.
func (a *app) findBestShortenedHeadingAtMinFont(b model.BlockLayout, heading string, minPt int, withDetails bool) (string, bool) {
	if minPt <= 0 {
		return "", false
	}
	ph := a.labelPlaceholder()
	r := []rune(heading)
	phRunes := []rune(ph)
	if len(r) == 0 || len(phRunes) == 0 || len(r) <= len(phRunes) {
		return "", false
	}
	maxKeep := len(r) - len(phRunes)
	if maxKeep <= 0 {
		return "", false
	}
	if !a.tileLabelFits(b, ph, minPt, withDetails) {
		return "", false
	}
	for keep := maxKeep; keep >= 0; keep-- {
		candidate := shortenedHeadingByKeep(heading, ph, keep)
		if a.tileLabelFits(b, candidate, minPt, withDetails) {
			return candidate, true
		}
	}
	return "", false
}

func shortenedHeadingByKeep(s, placeholder string, keep int) string {
	r := []rune(s)
	n := len(r)
	if keep <= 0 {
		return placeholder
	}
	if keep >= n {
		return s
	}
	leftKeep := keep / 2
	rightKeep := keep - leftKeep
	if leftKeep+rightKeep > n {
		return s
	}
	var b strings.Builder
	if leftKeep > 0 {
		b.WriteString(string(r[:leftKeep]))
	}
	b.WriteString(placeholder)
	if rightKeep > 0 {
		b.WriteString(string(r[n-rightKeep:]))
	}
	return b.String()
}

func (a *app) ensureLabelSolveInitialized() {
	if a.chart == nil {
		a.labelCache = nil
		a.labelSolved = nil
		a.labelCacheValid = false
		return
	}
	b := a.chart.ClientBoundsPixels()
	dpi := a.chart.DPI()
	if a.labelCacheValid &&
		len(a.labelCache) == len(a.blocks) &&
		len(a.labelSolved) == len(a.blocks) &&
		a.labelCacheW == b.Width &&
		a.labelCacheH == b.Height &&
		a.labelCacheDPI == dpi {
		return
	}
	if len(a.blocks) == 0 {
		a.labelCache = nil
		a.labelSolved = nil
		a.labelCacheValid = true
		a.labelCacheW, a.labelCacheH, a.labelCacheDPI = b.Width, b.Height, dpi
		return
	}
	a.labelCache = make([]labelChoice, len(a.blocks))
	a.labelSolved = make([]bool, len(a.blocks))
	a.labelCacheValid = true
	a.labelCacheW, a.labelCacheH, a.labelCacheDPI = b.Width, b.Height, dpi
	a.labelSolveNext = 0
	a.startLabelSolve()
}

func (a *app) startLabelSolve() {
	if a.chart == nil || len(a.blocks) == 0 || !a.labelCacheValid {
		return
	}
	a.labelSolving = true
	a.setScanChrome(a.scanning.Load())
	a.labelSolveNext = 0
	a.withMeasureCanvas(func() {
		for a.labelSolveNext < len(a.labelCache) && a.labelSolveNext < len(a.blocks) {
			i := a.labelSolveNext
			a.labelCache[i] = a.resolveTileLabel(a.blocks[i])
			a.labelSolved[i] = true
			a.labelSolveNext++
		}
	})
	a.labelSolving = false
	a.setScanChrome(a.scanning.Load())
	if a.chart != nil {
		a.chart.Invalidate()
	}
}

func (a *app) withMeasureCanvas(fn func()) {
	if fn == nil {
		return
	}
	if a.chart == nil {
		fn()
		return
	}
	c, err := a.chart.CreateCanvas()
	if err != nil {
		fn()
		return
	}
	prev := a.measureCanvas
	a.measureCanvas = c
	fn()
	a.measureCanvas = prev
	c.Dispose()
}

func fontCacheKey(f *walk.Font) string {
	return fmt.Sprintf("%p", f)
}

func (a *app) measureTextWidthCached(text string, font *walk.Font) (int, bool) {
	if a.chart == nil || font == nil {
		return 0, false
	}
	key := "w|" + fontCacheKey(font) + "|" + text
	if v, ok := a.textMeasureCache[key]; ok {
		return v, true
	}
	c := a.measureCanvas
	owned := false
	if c == nil {
		var err error
		c, err = a.chart.CreateCanvas()
		if err != nil {
			return 0, false
		}
		owned = true
	}
	b, _, err := c.MeasureTextPixels(text, font, walk.Rectangle{Width: 4000, Height: 400}, walk.TextCalcRect|walk.TextSingleLine)
	if owned {
		c.Dispose()
	}
	if err != nil {
		return 0, false
	}
	a.textMeasureCache[key] = b.Width
	return b.Width, true
}

func (a *app) measureTextHeightCached(text string, font *walk.Font) (int, bool) {
	if a.chart == nil || font == nil {
		return 0, false
	}
	key := "h|" + fontCacheKey(font) + "|" + text
	if v, ok := a.textMeasureCache[key]; ok {
		return v, true
	}
	c := a.measureCanvas
	owned := false
	if c == nil {
		var err error
		c, err = a.chart.CreateCanvas()
		if err != nil {
			return 0, false
		}
		owned = true
	}
	b, _, err := c.MeasureTextPixels(text, font, walk.Rectangle{Width: 4000, Height: 400}, walk.TextCalcRect|walk.TextSingleLine)
	if owned {
		c.Dispose()
	}
	if err != nil {
		return 0, false
	}
	a.textMeasureCache[key] = b.Height
	return b.Height, true
}

func (a *app) tileNeedsTooltipAt(index int) bool {
	a.ensureLabelSolveInitialized()
	if !a.labelCacheValid {
		// Before cache is ready, prefer showing tooltip over hiding useful info.
		return true
	}
	if index < 0 || index >= len(a.labelCache) || index >= len(a.labelSolved) {
		return true
	}
	if !a.labelSolved[index] {
		return true
	}
	ch := a.labelCache[index]
	if ch.mode == labelModeHidden {
		return true
	}
	return strings.Contains(ch.heading, a.labelPlaceholder())
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

func (a *app) tilePaddingPx() (left, top, right, bottom int) {
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
	left = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingLeftPt, 5)), int32(dpi), 72))
	top = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingTopPt, 5)), int32(dpi), 72))
	right = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingRightPt, 5)), int32(dpi), 72))
	bottom = int(win.MulDiv(int32(nz(a.treemapCfg.TilePaddingBottomPt, 5)), int32(dpi), 72))
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
		return walk.CursorArrow()
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

func (a *app) drawTileLabelAuto(canvas *walk.Canvas, b model.BlockLayout, choice labelChoice) {
	if choice.mode == labelModeHidden {
		vb := a.withExternalRect(b)
		pt := choice.pt
		if pt <= 0 {
			pt = a.treemapCfg.HeadingMinFontSizePt
			if pt <= 0 {
				pt = 8
			}
		}
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), a.labelDummy(), pt, false)
		return
	}
	vb := a.withExternalRect(b)
	switch choice.mode {
	case labelModeHorizWithDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), choice.heading, choice.pt, choice.withDetails)
	case labelModeHorizNoDetails:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), choice.heading, choice.pt, choice.withDetails)
	case labelModeHorizWithDetailsShort:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), choice.heading, choice.pt, choice.withDetails)
	case labelModeHorizNoDetailsShort:
		a.drawTreemapTileLabel(canvas, vb, a.chart.DPI(), choice.heading, choice.pt, choice.withDetails)
	}
}

func (a *app) drawTreemapTileLabel(canvas *walk.Canvas, b model.BlockLayout, dpi int, heading string, namePt int, withDetails bool) {
	lay, ok := a.computeLabelLayout(b, dpi, heading, namePt, withDetails)
	if !ok {
		return
	}
	fg := rgbaToWalkColor(b.TextColor)
	y := lay.inner.Min.Y
	drawLine := func(text string, font *walk.Font, lh int, clr walk.Color) {
		rc := walk.Rectangle{X: lay.inner.Min.X, Y: y, Width: lay.inner.Dx(), Height: lh}
		_ = canvas.DrawTextPixels(text, font, clr, rc, walk.TextSingleLine|walk.TextTop)
		y += lh
	}
	drawLine(heading, lay.nameFont, lay.nameLH, fg)
	if withDetails {
		y += lay.gap
		drawLine(format.ObjectSize(b.Size), lay.metaFont, lay.metaLH, fg)
		if lay.showShare {
			drawLine(lay.shareText, lay.metaFont, lay.metaLH, fg)
		}
	}
}

func (a *app) tileLabelFits(b model.BlockLayout, heading string, headingPt int, withDetails bool) bool {
	if a.chart == nil {
		return false
	}
	lay, ok := a.computeLabelLayout(b, a.chart.DPI(), heading, headingPt, withDetails)
	if !ok {
		return false
	}
	return lay.contentW <= lay.inner.Dx() && lay.contentH <= lay.inner.Dy()
}

func (a *app) computeLabelLayout(b model.BlockLayout, dpi int, heading string, namePt int, withDetails bool) (labelLayout, bool) {
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
	padL, padT, padR, padB := a.tilePaddingPx()
	lay.inner = image.Rect(b.Rect.Min.X+padL, b.Rect.Min.Y+padT, b.Rect.Max.X-padR, b.Rect.Max.Y-padB)
	if lay.inner.Empty() {
		return lay, false
	}
	lay.nameLH = int(float64(namePt)*a.ratioOr(a.treemapCfg.HeadingLineHeight, 1.2)*float64(dpi)/72.0 + 0.5)
	lay.metaLH = int(float64(metaPt)*a.ratioOr(a.treemapCfg.DetailsLineHeight, 1.5)*float64(dpi)/72.0 + 0.5)
	if h, ok := a.measureTextHeightCached("Agjpyq", lay.nameFont); ok && h > 0 && lay.nameLH < h+2 {
		lay.nameLH = h + 2
	}
	if h, ok := a.measureTextHeightCached("Agjpyq", lay.metaFont); ok && h > 0 && lay.metaLH < h+2 {
		lay.metaLH = h + 2
	}
	lay.gap = int(float64(metaPt)*a.ratioOr(a.treemapCfg.AboveDetailsRatio, 1.0)*float64(dpi)/72.0 + 0.5)
	lay.shareText, lay.showShare = formatShareLine(b.DriveShare)
	if !withDetails {
		lay.showShare = false
	}
	nameW, ok := a.measureTextWidthCached(heading, lay.nameFont)
	if !ok {
		return lay, false
	}
	lay.contentW = nameW
	lay.contentH = lay.nameLH
	if withDetails {
		sizeW, ok := a.measureTextWidthCached(format.ObjectSize(b.Size), lay.metaFont)
		if !ok {
			return lay, false
		}
		if sizeW > lay.contentW {
			lay.contentW = sizeW
		}
		lay.contentH += lay.gap + lay.metaLH
		if lay.showShare {
			shareW, ok := a.measureTextWidthCached(lay.shareText, lay.metaFont)
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

func prioritizedShortHeadings(s string, placeholder string) []string {
	placeholder = strings.TrimSpace(placeholder)
	if placeholder == "" {
		placeholder = "..."
	}
	r := []rune(s)
	n := len(r)
	ph := []rune(placeholder)
	phLen := len(ph)
	if phLen == 0 {
		ph = []rune("...")
		phLen = len(ph)
	}
	if n == 0 {
		return nil
	}
	if n <= phLen {
		return nil
	}
	appendUnique := func(out []string, seen map[string]struct{}, v string) []string {
		v = strings.TrimSpace(v)
		if v == "" || v == s {
			return out
		}
		if _, ok := seen[v]; ok {
			return out
		}
		seen[v] = struct{}{}
		return append(out, v)
	}
	build := func(leftKeep, rightKeep int) string {
		if leftKeep < 0 {
			leftKeep = 0
		}
		if rightKeep < 0 {
			rightKeep = 0
		}
		if leftKeep+rightKeep >= n {
			return s
		}
		var b strings.Builder
		if leftKeep > 0 {
			b.WriteString(string(r[:leftKeep]))
		}
		b.WriteString(placeholder)
		if rightKeep > 0 {
			b.WriteString(string(r[n-rightKeep:]))
		}
		return b.String()
	}
	keepTotal := n - phLen
	if keepTotal <= 0 {
		return []string{placeholder}
	}
	leftBalanced := keepTotal / 2
	rightBalanced := keepTotal - leftBalanced

	// Priority order:
	// 1) balanced middle cut, 2) prefix-biased, 3) suffix-biased,
	// 4) stronger balanced cut, 5) stronger prefix cut, 6) placeholder only.
	seen := make(map[string]struct{}, 8)
	out := make([]string, 0, 6)
	out = appendUnique(out, seen, build(leftBalanced, rightBalanced))
	out = appendUnique(out, seen, build(keepTotal*2/3, keepTotal/3))
	out = appendUnique(out, seen, build(keepTotal/3, keepTotal*2/3))
	out = appendUnique(out, seen, build(leftBalanced/2, rightBalanced/2))
	out = appendUnique(out, seen, build(maxInt(1, keepTotal/4), maxInt(1, keepTotal/6)))
	out = appendUnique(out, seen, placeholder)
	return out
}

func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func (a *app) labelPlaceholder() string {
	s := strings.TrimSpace(a.treemapCfg.LabelPlaceholder)
	if s == "" {
		return "..."
	}
	return s
}

func (a *app) labelDummy() string {
	s := strings.TrimSpace(a.treemapCfg.LabelDummy)
	if s == "" {
		return "..."
	}
	return s
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
	if len(a.items) == 0 {
		return nil
	}
	return layout.Squarified(a.items, area, minW, minH)
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
