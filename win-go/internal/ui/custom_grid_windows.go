//go:build windows

package ui

import (
	"fmt"
	"syscall"
	"strings"
	"sync"
	"unsafe"

	"github.com/lxn/walk"
	"github.com/lxn/win"
)

var (
	user32DLL          = syscall.NewLazyDLL("user32.dll")
	procGetWindowTextW = user32DLL.NewProc("GetWindowTextW")
	procSetWindowTextW = user32DLL.NewProc("SetWindowTextW")
)

type customWin32GridHost struct {
	parent      *walk.Composite
	hwnd        win.HWND
	oldWndProc  uintptr
	states      []RowState
	showError   func(string)
	clearError  func()
	activeRow   int
	activeEdit  win.HWND
	activeBtn   win.HWND
	editOldProc uintptr
	activeKind  EditorKind
	baseFont    win.HFONT
}

var (
	gridHostsByHWND sync.Map // key: uintptr(hwnd), value: *customWin32GridHost
	editHostsByHWND sync.Map // key: uintptr(hwnd), value: *customWin32GridHost
)

func newCustomWin32GridHost(parent *walk.Composite, states []RowState, showError func(string), clearError func()) (*customWin32GridHost, error) {
	if parent == nil {
		return nil, fmt.Errorf("nil grid parent")
	}
	if showError == nil {
		showError = func(string) {}
	}
	if clearError == nil {
		clearError = func() {}
	}
	host := &customWin32GridHost{
		parent:     parent,
		states:     states,
		showError:  showError,
		clearError: clearError,
		activeRow:  -1,
	}
	if err := host.createListView(); err != nil {
		return nil, err
	}
	return host, nil
}

func (h *customWin32GridHost) createListView() error {
	rc := h.parent.ClientBoundsPixels()
	style := uint32(win.WS_CHILD | win.WS_VISIBLE | win.WS_TABSTOP | win.WS_BORDER | win.LVS_REPORT | win.LVS_SHOWSELALWAYS | win.LVS_SINGLESEL)
	hwnd := win.CreateWindowEx(
		0,
		utf16Ptr("SysListView32"),
		nil,
		style,
		0, 0, int32(rc.Width), int32(rc.Height),
		h.parent.Handle(),
		0,
		win.GetModuleHandle(nil),
		nil,
	)
	if hwnd == 0 {
		return fmt.Errorf("CreateWindowEx(SysListView32) failed")
	}
	h.hwnd = hwnd
	h.baseFont = win.HFONT(win.SendMessage(h.hwnd, win.WM_GETFONT, 0, 0))
	h.insertColumns()
	h.populateRows()
	gridHostsByHWND.Store(uintptr(h.hwnd), h)
	h.oldWndProc = win.SetWindowLongPtr(h.hwnd, win.GWLP_WNDPROC, syscallListViewWndProc)
	return nil
}

func (h *customWin32GridHost) insertColumns() {
	insertCol := func(idx int32, title string, w int32) {
		col := win.LVCOLUMN{
			Mask:      win.LVCF_TEXT | win.LVCF_WIDTH | win.LVCF_SUBITEM,
			Cx:        w,
			PszText:   utf16Ptr(title),
			CchTextMax: int32(len(title)),
			ISubItem: idx,
		}
		win.SendMessage(h.hwnd, win.LVM_INSERTCOLUMN, uintptr(idx), uintptr(unsafe.Pointer(&col)))
	}
	insertCol(0, "Parameter", 260)
	insertCol(1, "Value", 190)
}

func (h *customWin32GridHost) populateRows() {
	for i := range h.states {
		param := ""
		val := h.states[i].PendingValue
		if h.states[i].Schema != nil {
			param = h.states[i].Schema.Label
		}
		item := win.LVITEM{
			Mask:  win.LVIF_TEXT,
			IItem: int32(i),
			PszText: utf16Ptr(param),
		}
		win.SendMessage(h.hwnd, win.LVM_INSERTITEM, 0, uintptr(unsafe.Pointer(&item)))
		_ = h.setSubitemText(i, 1, val)
	}
}

func (h *customWin32GridHost) layout() {
	if h.hwnd == 0 || h.parent == nil {
		return
	}
	rc := h.parent.ClientBoundsPixels()
	win.SetWindowPos(h.hwnd, 0, 0, 0, int32(rc.Width), int32(rc.Height), win.SWP_NOZORDER)
	if rc.Width > 0 {
		right := int32(190)
		if rc.Width < 560 {
			right = 160
		}
		left := int32(rc.Width) - right - 8
		if left < 180 {
			left = 180
		}
		win.SendMessage(h.hwnd, win.LVM_SETCOLUMNWIDTH, 0, uintptr(left))
		win.SendMessage(h.hwnd, win.LVM_SETCOLUMNWIDTH, 1, uintptr(right))
	}
	h.repositionEditor()
}

func (h *customWin32GridHost) destroy() {
	h.cancelActive()
	if h.hwnd != 0 {
		win.SetWindowLongPtr(h.hwnd, win.GWLP_WNDPROC, h.oldWndProc)
		gridHostsByHWND.Delete(uintptr(h.hwnd))
		win.DestroyWindow(h.hwnd)
		h.hwnd = 0
	}
}

func (h *customWin32GridHost) setSubitemText(row, sub int, text string) error {
	buf := append([]uint16{}, syscall.StringToUTF16(text)...)
	it := win.LVITEM{
		IItem:    int32(row),
		ISubItem: int32(sub),
		PszText:  &buf[0],
	}
	if win.SendMessage(h.hwnd, win.LVM_SETITEMTEXT, uintptr(row), uintptr(unsafe.Pointer(&it))) == 0 {
		return fmt.Errorf("LVM_SETITEMTEXT failed")
	}
	return nil
}

func (h *customWin32GridHost) activateEditor(row int) {
	if row < 0 || row >= len(h.states) {
		return
	}
	if h.activeEdit != 0 && h.activeRow == row {
		return
	}
	if !h.commitActive() {
		return
	}
	rect, ok := h.valueCellRect(row)
	if !ok {
		return
	}
	schema := h.states[row].Schema
	if schema != nil {
		switch schema.Kind {
		case KindColor:
			h.openColorEditor(row, rect)
			return
		case KindDropdown:
			h.openDropdownEditor(row, rect)
			return
		}
	}
	h.openTextEditor(row, rect)
}

func (h *customWin32GridHost) openTextEditor(row int, rect win.RECT) {
	txt := h.states[row].PendingValue
	hEdit := win.CreateWindowEx(
		win.WS_EX_CLIENTEDGE,
		utf16Ptr("EDIT"),
		utf16Ptr(txt),
		win.WS_CHILD|win.WS_VISIBLE|win.WS_TABSTOP|win.ES_AUTOHSCROLL,
		rect.Left, rect.Top, rect.Right-rect.Left, rect.Bottom-rect.Top,
		h.hwnd,
		0,
		win.GetModuleHandle(nil),
		nil,
	)
	if hEdit == 0 {
		return
	}
	h.activeRow = row
	h.activeEdit = hEdit
	h.activeKind = KindText
	win.SendMessage(hEdit, win.WM_SETFONT, uintptr(h.baseFont), 1)
	editHostsByHWND.Store(uintptr(hEdit), h)
	h.editOldProc = win.SetWindowLongPtr(hEdit, win.GWLP_WNDPROC, syscallEditWndProc)
	win.SetFocus(hEdit)
	win.SendMessage(hEdit, win.EM_SETSEL, 0, ^uintptr(0))
}

func (h *customWin32GridHost) openDropdownEditor(row int, rect win.RECT) {
	schema := h.states[row].Schema
	if schema == nil {
		return
	}
	hCombo := win.CreateWindowEx(
		0,
		utf16Ptr("COMBOBOX"),
		nil,
		win.WS_CHILD|win.WS_VISIBLE|win.WS_TABSTOP|win.CBS_DROPDOWN|win.WS_VSCROLL,
		rect.Left, rect.Top, rect.Right-rect.Left, maxInt32(220, (rect.Bottom-rect.Top)*8),
		h.hwnd,
		0,
		win.GetModuleHandle(nil),
		nil,
	)
	if hCombo == 0 {
		return
	}
	for _, opt := range schema.Options {
		win.SendMessage(hCombo, win.CB_ADDSTRING, 0, uintptr(unsafe.Pointer(utf16Ptr(opt))))
	}
	cur := strings.TrimSpace(h.states[row].PendingValue)
	idx := int32(-1)
	for i, opt := range schema.Options {
		if strings.EqualFold(strings.TrimSpace(opt), cur) {
			idx = int32(i)
			break
		}
	}
	if idx >= 0 {
		win.SendMessage(hCombo, win.CB_SETCURSEL, uintptr(idx), 0)
	} else {
		setWindowText(hCombo, utf16Ptr(cur))
	}
	h.activeRow = row
	h.activeEdit = hCombo
	h.activeKind = KindDropdown
	win.SendMessage(hCombo, win.WM_SETFONT, uintptr(h.baseFont), 1)
	editHostsByHWND.Store(uintptr(hCombo), h)
	h.editOldProc = win.SetWindowLongPtr(hCombo, win.GWLP_WNDPROC, syscallEditWndProc)
	win.SetFocus(hCombo)
}

func (h *customWin32GridHost) openColorEditor(row int, rect win.RECT) {
	btnW := int32(28)
	ew := rect.Right - rect.Left - btnW - 2
	if ew < 70 {
		ew = 70
	}
	txt := h.states[row].PendingValue
	hEdit := win.CreateWindowEx(
		win.WS_EX_CLIENTEDGE,
		utf16Ptr("EDIT"),
		utf16Ptr(txt),
		win.WS_CHILD|win.WS_VISIBLE|win.WS_TABSTOP|win.ES_AUTOHSCROLL,
		rect.Left, rect.Top, ew, rect.Bottom-rect.Top,
		h.hwnd,
		0,
		win.GetModuleHandle(nil),
		nil,
	)
	if hEdit == 0 {
		return
	}
	hBtn := win.CreateWindowEx(
		0,
		utf16Ptr("BUTTON"),
		utf16Ptr("..."),
		win.WS_CHILD|win.WS_VISIBLE|win.WS_TABSTOP,
		rect.Left+ew+2, rect.Top, btnW, rect.Bottom-rect.Top,
		h.hwnd,
		0,
		win.GetModuleHandle(nil),
		nil,
	)
	if hBtn == 0 {
		win.DestroyWindow(hEdit)
		return
	}
	h.activeRow = row
	h.activeEdit = hEdit
	h.activeBtn = hBtn
	h.activeKind = KindColor
	win.SendMessage(hEdit, win.WM_SETFONT, uintptr(h.baseFont), 1)
	win.SendMessage(hBtn, win.WM_SETFONT, uintptr(h.baseFont), 1)
	editHostsByHWND.Store(uintptr(hEdit), h)
	h.editOldProc = win.SetWindowLongPtr(hEdit, win.GWLP_WNDPROC, syscallEditWndProc)
	win.SetFocus(hEdit)
	win.SendMessage(hEdit, win.EM_SETSEL, 0, ^uintptr(0))
}

func (h *customWin32GridHost) pickColorIntoActiveEdit() {
	row := h.activeRow
	if row < 0 || row >= len(h.states) {
		return
	}
	current := h.editorText()
	next := showColorDialog(h.hwnd, current)
	if next == "" {
		return
	}
	setWindowText(h.activeEdit, utf16Ptr(next))
	win.SetFocus(h.activeEdit)
}

func (h *customWin32GridHost) valueCellRect(row int) (win.RECT, bool) {
	var rc win.RECT
	rc.Top = 1 // subitem
	rc.Left = win.LVIR_BOUNDS
	ok := win.SendMessage(h.hwnd, win.LVM_GETSUBITEMRECT, uintptr(row), uintptr(unsafe.Pointer(&rc)))
	if ok == 0 {
		return win.RECT{}, false
	}
	return rc, true
}

func (h *customWin32GridHost) repositionEditor() {
	if h.activeEdit == 0 || h.activeRow < 0 {
		return
	}
	rc, ok := h.valueCellRect(h.activeRow)
	if !ok {
		h.cancelActive()
		return
	}
	hh := rc.Bottom - rc.Top
	if h.activeKind == KindDropdown {
		hh = maxInt32(220, hh*8)
	}
	win.SetWindowPos(h.activeEdit, 0, rc.Left, rc.Top, rc.Right-rc.Left, hh, win.SWP_NOZORDER|win.SWP_NOACTIVATE)
	if h.activeKind == KindColor && h.activeBtn != 0 {
		btnW := int32(28)
		ew := rc.Right - rc.Left - btnW - 2
		if ew < 70 {
			ew = 70
		}
		win.SetWindowPos(h.activeEdit, 0, rc.Left, rc.Top, ew, rc.Bottom-rc.Top, win.SWP_NOZORDER|win.SWP_NOACTIVATE)
		win.SetWindowPos(h.activeBtn, 0, rc.Left+ew+2, rc.Top, btnW, rc.Bottom-rc.Top, win.SWP_NOZORDER|win.SWP_NOACTIVATE)
	}
}

func (h *customWin32GridHost) editorText() string {
	if h.activeEdit == 0 {
		return ""
	}
	buf := make([]uint16, 2048)
	if h.activeKind == KindDropdown {
		idx := int32(win.SendMessage(h.activeEdit, win.CB_GETCURSEL, 0, 0))
		if idx >= 0 {
			win.SendMessage(h.activeEdit, win.CB_GETLBTEXT, uintptr(idx), uintptr(unsafe.Pointer(&buf[0])))
			return strings.TrimSpace(syscall.UTF16ToString(buf))
		}
	}
	n := getWindowText(h.activeEdit, &buf[0], int32(len(buf)))
	if n <= 0 {
		return ""
	}
	return strings.TrimSpace(syscall.UTF16ToString(buf[:n]))
}

func (h *customWin32GridHost) commitActive() bool {
	if h.activeEdit == 0 || h.activeRow < 0 || h.activeRow >= len(h.states) {
		return true
	}
	val := h.editorText()
	st := &h.states[h.activeRow]
	old := st.LastGood
	st.PendingValue = val
	if err := validateField(st); err != nil {
		h.showError(err.Error())
		st.PendingValue = old
		setWindowText(h.activeEdit, utf16Ptr(old))
		win.SetFocus(h.activeEdit)
		return false
	}
	st.LastGood = val
	_ = h.setSubitemText(h.activeRow, 1, val)
	h.clearError()
	h.closeActiveEditor()
	return true
}

func (h *customWin32GridHost) cancelActive() {
	if h.activeEdit == 0 || h.activeRow < 0 || h.activeRow >= len(h.states) {
		return
	}
	st := &h.states[h.activeRow]
	st.PendingValue = st.LastGood
	_ = h.setSubitemText(h.activeRow, 1, st.LastGood)
	h.closeActiveEditor()
	h.clearError()
}

func (h *customWin32GridHost) closeActiveEditor() {
	if h.activeEdit != 0 {
		win.SetWindowLongPtr(h.activeEdit, win.GWLP_WNDPROC, h.editOldProc)
		editHostsByHWND.Delete(uintptr(h.activeEdit))
		win.DestroyWindow(h.activeEdit)
	}
	if h.activeBtn != 0 {
		win.DestroyWindow(h.activeBtn)
	}
	h.activeEdit = 0
	h.activeBtn = 0
	h.activeRow = -1
	h.editOldProc = 0
	h.activeKind = KindText
	win.SetFocus(h.hwnd)
}

func (h *customWin32GridHost) currentRow() int {
	row := int(win.SendMessage(h.hwnd, win.LVM_GETNEXTITEM, ^uintptr(0), uintptr(win.LVNI_SELECTED)))
	return row
}

func (h *customWin32GridHost) setCurrentRow(row int) {
	if row < 0 || row >= len(h.states) {
		return
	}
	item := win.LVITEM{
		StateMask: win.LVIS_SELECTED | win.LVIS_FOCUSED,
		State:     win.LVIS_SELECTED | win.LVIS_FOCUSED,
	}
	win.SendMessage(h.hwnd, win.LVM_SETITEMSTATE, uintptr(row), uintptr(unsafe.Pointer(&item)))
	win.SendMessage(h.hwnd, win.LVM_ENSUREVISIBLE, uintptr(row), 0)
}

func (h *customWin32GridHost) handleListViewMessage(msg uint32, wParam, lParam uintptr) uintptr {
	switch msg {
	case win.WM_LBUTTONDOWN:
		res := win.CallWindowProc(h.oldWndProc, h.hwnd, msg, wParam, lParam)
		x := int16(lParam & 0xFFFF)
		y := int16((lParam >> 16) & 0xFFFF)
		info := win.LVHITTESTINFO{Pt: win.POINT{X: int32(x), Y: int32(y)}}
		row := int(win.SendMessage(h.hwnd, win.LVM_SUBITEMHITTEST, 0, uintptr(unsafe.Pointer(&info))))
		if row >= 0 && row < len(h.states) && info.ISubItem == 1 {
			h.activateEditor(row)
		} else if h.activeEdit != 0 {
			_ = h.commitActive()
		}
		return res
	case win.WM_KEYDOWN:
		switch uint32(wParam) {
		case win.VK_F2, win.VK_RETURN:
			if h.activeEdit == 0 {
				if row := h.currentRow(); row >= 0 {
					h.activateEditor(row)
					return 0
				}
			}
		case win.VK_UP:
			if h.activeEdit == 0 {
				row := h.currentRow()
				if row > 0 {
					h.setCurrentRow(row - 1)
				}
				return 0
			}
		case win.VK_DOWN:
			if h.activeEdit == 0 {
				row := h.currentRow()
				if row >= 0 && row < len(h.states)-1 {
					h.setCurrentRow(row + 1)
				}
				return 0
			}
		}
	case win.WM_COMMAND:
		if h.activeKind == KindDropdown && h.activeEdit != 0 && win.HWND(lParam) == h.activeEdit {
			code := uint16((wParam >> 16) & 0xFFFF)
			if code == win.CBN_SELCHANGE {
				_ = h.commitActive()
				return 0
			}
		}
		if h.activeKind == KindColor && h.activeBtn != 0 && win.HWND(lParam) == h.activeBtn {
			code := uint16((wParam >> 16) & 0xFFFF)
			if code == win.BN_CLICKED {
				h.pickColorIntoActiveEdit()
				return 0
			}
		}
	case win.WM_VSCROLL, win.WM_HSCROLL, win.WM_SIZE:
		res := win.CallWindowProc(h.oldWndProc, h.hwnd, msg, wParam, lParam)
		h.repositionEditor()
		return res
	}
	return win.CallWindowProc(h.oldWndProc, h.hwnd, msg, wParam, lParam)
}

func (h *customWin32GridHost) handleEditMessage(msg uint32, wParam, lParam uintptr) uintptr {
	switch msg {
	case win.WM_KEYDOWN:
		switch uint32(wParam) {
		case win.VK_RETURN:
			_ = h.commitActive()
			return 0
		case win.VK_ESCAPE:
			h.cancelActive()
			return 0
		case win.VK_UP:
			curr := h.activeRow
			if h.commitActive() {
				next := curr - 1
				if next >= 0 {
					h.setCurrentRow(next)
					h.activateEditor(next)
				}
			}
			return 0
		case win.VK_DOWN:
			curr := h.activeRow
			if h.commitActive() {
				next := curr + 1
				if next < len(h.states) {
					h.setCurrentRow(next)
					h.activateEditor(next)
				}
			}
			return 0
		}
	case win.WM_KILLFOCUS:
		_ = h.commitActive()
		return 0
	}
	return win.CallWindowProc(h.editOldProc, h.activeEdit, msg, wParam, lParam)
}

var syscallListViewWndProc uintptr
var syscallEditWndProc uintptr

func init() {
	syscallListViewWndProc = syscall.NewCallback(listViewWndProc)
	syscallEditWndProc = syscall.NewCallback(editWndProc)
}

func listViewWndProc(hwnd win.HWND, msg uint32, wParam, lParam uintptr) uintptr {
	if v, ok := gridHostsByHWND.Load(uintptr(hwnd)); ok {
		if h, ok := v.(*customWin32GridHost); ok {
			return h.handleListViewMessage(msg, wParam, lParam)
		}
	}
	return win.DefWindowProc(hwnd, msg, wParam, lParam)
}

func editWndProc(hwnd win.HWND, msg uint32, wParam, lParam uintptr) uintptr {
	if v, ok := editHostsByHWND.Load(uintptr(hwnd)); ok {
		if h, ok := v.(*customWin32GridHost); ok {
			return h.handleEditMessage(msg, wParam, lParam)
		}
	}
	return win.DefWindowProc(hwnd, msg, wParam, lParam)
}

func utf16Ptr(s string) *uint16 {
	p, _ := syscall.UTF16PtrFromString(s)
	return p
}

func getWindowText(hwnd win.HWND, buf *uint16, max int32) int32 {
	r, _, _ := procGetWindowTextW.Call(uintptr(hwnd), uintptr(unsafe.Pointer(buf)), uintptr(max))
	return int32(r)
}

func setWindowText(hwnd win.HWND, text *uint16) {
	procSetWindowTextW.Call(uintptr(hwnd), uintptr(unsafe.Pointer(text)))
}

func maxInt32(a, b int32) int32 {
	if a > b {
		return a
	}
	return b
}

