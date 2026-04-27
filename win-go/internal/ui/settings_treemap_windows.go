//go:build windows

package ui

import (
	"encoding/json"
	"fmt"
	"html"
	"image/color"
	"net/url"
	"strconv"
	"strings"
	"syscall"
	"unsafe"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"

	"eraserewrite/win-go/internal/config"
)

func primarySysListView(tv *walk.TableView) win.HWND {
	if tv == nil {
		return 0
	}
	parent := tv.Handle()
	var best win.HWND
	var bestW int32
	for h := win.GetWindow(parent, win.GW_CHILD); h != 0; h = win.GetWindow(h, win.GW_HWNDNEXT) {
		var buf [80]uint16
		n, _ := win.GetClassName(h, &buf[0], 80)
		if n <= 0 {
			continue
		}
		if syscall.UTF16ToString(buf[:n]) != "SysListView32" {
			continue
		}
		var rc win.RECT
		win.GetClientRect(h, &rc)
		w := rc.Right - rc.Left
		if w > bestW {
			bestW = w
			best = h
		}
	}
	return best
}

func treemapHitTestCellOnListView(lv win.HWND) (row, col int, ok bool) {
	if lv == 0 {
		return 0, 0, false
	}
	var pt win.POINT
	win.GetCursorPos(&pt)
	ptc := pt
	if !win.ScreenToClient(lv, &ptc) {
		return 0, 0, false
	}
	var cr win.RECT
	win.GetClientRect(lv, &cr)
	if ptc.X < cr.Left || ptc.X >= cr.Right || ptc.Y < cr.Top || ptc.Y >= cr.Bottom {
		return 0, 0, false
	}
	var hti win.LVHITTESTINFO
	hti.Pt = ptc
	ret := int32(win.SendMessage(lv, win.LVM_SUBITEMHITTEST, 0, uintptr(unsafe.Pointer(&hti))))
	if ret == -1 || hti.IItem < 0 || hti.ISubItem < 0 {
		return 0, 0, false
	}
	return int(hti.IItem), int(hti.ISubItem), true
}

func subitemBoundsDlg96(dlg *walk.Dialog, lv win.HWND, row, col int) walk.Rectangle {
	if dlg == nil || lv == 0 {
		return walk.Rectangle{}
	}
	var r win.RECT
	r.Top = int32(col)
	r.Left = win.LVIR_LABEL
	if win.FALSE == win.SendMessage(lv, win.LVM_GETSUBITEMRECT, uintptr(row), uintptr(unsafe.Pointer(&r))) {
		return walk.Rectangle{}
	}
	tl := win.POINT{X: r.Left, Y: r.Top}
	br := win.POINT{X: r.Right, Y: r.Bottom}
	win.ClientToScreen(lv, &tl)
	win.ClientToScreen(lv, &br)
	hDlg := dlg.AsFormBase().Handle()
	win.ScreenToClient(hDlg, &tl)
	win.ScreenToClient(hDlg, &br)
	return walk.Rectangle{X: int(tl.X), Y: int(tl.Y), Width: int(br.X - tl.X), Height: int(br.Y - tl.Y)}
}

func clampRectToRect(inner, outer walk.Rectangle) (walk.Rectangle, bool) {
	if inner.Width <= 0 || inner.Height <= 0 || outer.Width <= 0 || outer.Height <= 0 {
		return walk.Rectangle{}, false
	}
	x1 := inner.X
	y1 := inner.Y
	x2 := inner.X + inner.Width
	y2 := inner.Y + inner.Height
	ox1 := outer.X
	oy1 := outer.Y
	ox2 := outer.X + outer.Width
	oy2 := outer.Y + outer.Height
	if x2 <= ox1 || x1 >= ox2 || y2 <= oy1 || y1 >= oy2 {
		return walk.Rectangle{}, false
	}
	if x1 < ox1 {
		x1 = ox1
	}
	if y1 < oy1 {
		y1 = oy1
	}
	if x2 > ox2 {
		x2 = ox2
	}
	if y2 > oy2 {
		y2 = oy2
	}
	out := walk.Rectangle{X: x1, Y: y1, Width: x2 - x1, Height: y2 - y1}
	return out, out.Width >= 8 && out.Height >= 8
}

type treemapInlineEditor struct {
	dlg       *walk.Dialog
	tv        *walk.TableView
	gridModel *treemapGridModel
	edited    *config.Treemap
	fontModel []string
	line      *walk.LineEdit
	fontCombo *walk.ComboBox
	lv        win.HWND
	row       int
	editing   bool
	fontEdit  bool
}

func (ie *treemapInlineEditor) ensureListView() bool {
	if ie == nil || ie.tv == nil {
		return false
	}
	if ie.lv != 0 {
		return true
	}
	ie.lv = primarySysListView(ie.tv)
	return ie.lv != 0
}

func (ie *treemapInlineEditor) endEdit(commit bool) {
	if ie == nil || !ie.editing {
		return
	}
	if commit {
		var err error
		if ie.fontEdit {
			err = treemapSetValueColFromString(ie.edited, ie.row, ie.fontCombo.Text())
		} else {
			err = treemapSetValueColFromString(ie.edited, ie.row, ie.line.Text())
		}
		if err != nil {
			walk.MsgBox(ie.dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return
		}
	}
	ie.line.SetVisible(false)
	ie.fontCombo.SetVisible(false)
	ie.editing = false
	ie.fontEdit = false
	ie.row = -1
	ie.gridModel.refreshAll()
}

func (ie *treemapInlineEditor) beginEdit(row int, font bool) {
	if ie == nil || row < 0 || row >= len(treemapGridRowLabels) {
		return
	}
	ie.endEdit(true)
	if !ie.ensureListView() {
		return
	}
	b := subitemBoundsDlg96(ie.dlg, ie.lv, row, 1)
	if b.Width < 10 || b.Height < 10 {
		return
	}
	tvBounds := ie.tv.Bounds()
	cb, ok := clampRectToRect(b, tvBounds)
	if !ok {
		return
	}
	b = cb
	ie.row = row
	ie.editing = true
	ie.fontEdit = font
	if font {
		_ = ie.fontCombo.SetModel(ie.fontModel)
		cur := strings.TrimSpace(ie.edited.TileFontName)
		_ = ie.fontCombo.SetCurrentIndex(-1)
		for i, name := range ie.fontModel {
			if strings.EqualFold(name, cur) {
				_ = ie.fontCombo.SetCurrentIndex(i)
				break
			}
		}
		_ = ie.fontCombo.SetText(cur)
		ie.fontCombo.SetBounds(b)
		ie.fontCombo.SetVisible(true)
		ie.fontCombo.BringToTop()
		_ = ie.fontCombo.SetFocus()
		return
	}
	_ = ie.line.SetText(treemapValueColString(ie.edited, row))
	ie.line.SetBounds(b)
	ie.line.SetVisible(true)
	ie.line.BringToTop()
	_ = ie.line.SetFocus()
	ie.line.SetTextSelection(0, -1)
}

func showTreemapSettingsDialog(owner walk.Form, current config.Treemap, onApply func(config.Treemap)) {
	edited := current
	def := config.DefaultTreemap()

	var dlg *walk.Dialog
	var web *walk.WebView

	gridModel := &treemapGridModel{edited: &edited}
	setFields := func(t config.Treemap) {
		edited = t
		gridModel.edited = &edited
		gridModel.refreshAll()
	}
	saveAndApply := func(closeOnSuccess bool) bool {
		if err := validateTreemap(&edited); err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		p, err := config.ConfigPath()
		if err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		if err := config.SaveTreemap(p, edited); err != nil {
			walk.MsgBox(dlg, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
			return false
		}
		if onApply != nil {
			onApply(edited)
		}
		if closeOnSuccess {
			dlg.Accept()
		}
		return true
	}
	reloadGrid := func() {
		if web == nil {
			return
		}
		labelsJSON, _ := json.Marshal(treemapGridRowLabels)
		values := make([]string, len(treemapGridRowLabels))
		for i := range treemapGridRowLabels {
			values[i] = treemapValueColString(&edited, i)
		}
		valuesJSON, _ := json.Marshal(values)
		htmlDoc := `<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<style>
body{font-family:Segoe UI,Tahoma,Arial,sans-serif;margin:10px}
.toolbar{margin-bottom:8px}
button{margin-right:6px;padding:4px 12px}
table{border-collapse:collapse;width:100%}
th,td{border:1px solid #c8c8c8;padding:6px 8px}
th{background:#f4f4f4;text-align:left}
td.value{background:#fff}
td.value[contenteditable="true"]:focus{outline:2px solid #4a90e2}
</style>
</head>
<body>
<div class="toolbar">
<button onclick="doAction('apply')">Apply</button>
<button onclick="doAction('ok')">OK</button>
<button onclick="doAction('reset')">Reset</button>
<button onclick="doAction('cancel')">Cancel</button>
</div>
<table id="grid"><thead><tr><th style="width:45%">Parameter</th><th style="width:55%">Value</th></tr></thead><tbody></tbody></table>
<script>
const labels=` + string(labelsJSON) + `;
const values=` + string(valuesJSON) + `;
const tbody=document.querySelector('#grid tbody');
for(let i=0;i<labels.length;i++){
  const tr=document.createElement('tr');
  tr.dataset.row=i;
  const c0=document.createElement('td'); c0.textContent=labels[i];
  const c1=document.createElement('td'); c1.className='value'; c1.contentEditable='true'; c1.textContent=values[i] || '';
  tr.appendChild(c0); tr.appendChild(c1); tbody.appendChild(tr);
}
function collectValues(){
  return Array.from(document.querySelectorAll('#grid tbody tr td.value')).map(td => td.textContent.trim());
}
function doAction(kind){
  if(kind==='cancel' || kind==='reset'){
    location.href='app://' + kind;
    return;
  }
  const payload=encodeURIComponent(JSON.stringify(collectValues()));
  location.href='app://' + kind + '?data=' + payload;
}
</script>
</body></html>`
		_ = web.SetURL("data:text/html," + url.QueryEscape(htmlDoc))
	}

	var icon *walk.Icon
	if ic, err := loadEmbeddedAppIcon(); err == nil {
		icon = ic
	}

	decl := Dialog{
		AssignTo:      &dlg,
		Title:         "Settings",
		Icon:          icon,
		MinSize:       Size{Width: 720, Height: 560},
		Size:          Size{Width: 820, Height: 640},
		Layout:        VBox{Margins: Margins{12, 12, 12, 12}, Spacing: 8},
		Children: []Widget{
			WebView{
				AssignTo:      &web,
				StretchFactor: 1,
				OnNavigating: func(e *walk.WebViewNavigatingEventData) {
					u := e.Url()
					if !strings.HasPrefix(strings.ToLower(strings.TrimSpace(u)), "app://") {
						return
					}
					e.SetCanceled(true)
					pu, err := url.Parse(u)
					if err != nil {
						return
					}
					action := strings.ToLower(strings.TrimSpace(pu.Host))
					switch action {
					case "cancel":
						dlg.Cancel()
					case "reset":
						setFields(def)
						reloadGrid()
					case "apply", "ok":
						raw := pu.Query().Get("data")
						decoded, err := url.QueryUnescape(raw)
						if err != nil {
							walk.MsgBox(dlg, "Settings", "Invalid editor payload.", walk.MsgBoxOK|walk.MsgBoxIconError)
							return
						}
						var vals []string
						if err := json.Unmarshal([]byte(decoded), &vals); err != nil {
							walk.MsgBox(dlg, "Settings", "Cannot parse edited values.", walk.MsgBoxOK|walk.MsgBoxIconError)
							return
						}
						if len(vals) != len(treemapGridRowLabels) {
							walk.MsgBox(dlg, "Settings", "Edited row count mismatch.", walk.MsgBoxOK|walk.MsgBoxIconError)
							return
						}
						next := edited
						for i := range vals {
							if err := treemapSetValueColFromString(&next, i, vals[i]); err != nil {
								msg := "Invalid value for " + html.EscapeString(treemapGridRowLabels[i]) + ": " + err.Error()
								walk.MsgBox(dlg, "Settings", msg, walk.MsgBoxOK|walk.MsgBoxIconError)
								return
							}
						}
						edited = next
						gridModel.edited = &edited
						if saveAndApply(action == "ok") && action != "ok" {
							reloadGrid()
						}
					}
				},
			},
		},
	}

	if err := decl.Create(owner); err != nil {
		walk.MsgBox(owner, "Settings", err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
		return
	}
	setFields(current)
	reloadGrid()
	_ = dlg.Run()
}

func showColorDialog(hwnd win.HWND, current string) string {
	var r, g, b uint8
	_, _ = fmt.Sscanf(strings.TrimSpace(current), "#%02x%02x%02x", &r, &g, &b)
	initColor := win.COLORREF(uint32(r) | uint32(g)<<8 | uint32(b)<<16)
	var customColors [16]win.COLORREF
	cc := win.CHOOSECOLOR{
		LStructSize:  uint32(unsafe.Sizeof(win.CHOOSECOLOR{})),
		HwndOwner:    hwnd,
		RgbResult:    initColor,
		LpCustColors: &customColors,
		Flags:        win.CC_FULLOPEN | win.CC_RGBINIT,
	}
	if !win.ChooseColor(&cc) {
		return ""
	}
	return fmt.Sprintf("#%02X%02X%02X", byte(cc.RgbResult&0xFF), byte((cc.RgbResult>>8)&0xFF), byte((cc.RgbResult>>16)&0xFF))
}

func parsePercentOrRatio(s string) (float64, error) {
	s = strings.TrimSpace(strings.ToLower(s))
	if s == "" {
		return 0, fmt.Errorf("empty value")
	}
	if strings.HasSuffix(s, "%") {
		v, err := strconv.ParseFloat(strings.TrimSpace(strings.TrimSuffix(s, "%")), 64)
		if err != nil {
			return 0, err
		}
		return v / 100.0, nil
	}
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0, err
	}
	if v > 1 {
		return v / 100.0, nil
	}
	return v, nil
}

func parseHexColor(s string) (color.RGBA, error) {
	s = strings.TrimSpace(strings.TrimPrefix(strings.TrimSpace(s), "#"))
	if len(s) != 6 {
		return color.RGBA{}, fmt.Errorf("must be 6 hex digits")
	}
	var r, g, b uint8
	if _, err := fmt.Sscanf(s, "%02x%02x%02x", &r, &g, &b); err != nil {
		return color.RGBA{}, fmt.Errorf("invalid hex color")
	}
	return color.RGBA{R: r, G: g, B: b, A: 255}, nil
}

func formatRGBHex(c color.RGBA) string {
	return fmt.Sprintf("#%02X%02X%02X", c.R, c.G, c.B)
}

