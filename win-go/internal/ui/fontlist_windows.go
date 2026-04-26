//go:build windows

package ui

import (
	"fmt"
	"sort"
	"strings"
	"syscall"
	"unsafe"

	"github.com/lxn/win"
	"golang.org/x/sys/windows"
)

type fontEnumData struct {
	names map[string]struct{}
}

func logfontFaceName(lf *win.LOGFONT) string {
	var b strings.Builder
	for _, u := range lf.LfFaceName {
		if u == 0 {
			break
		}
		b.WriteRune(rune(u))
	}
	return strings.TrimSpace(b.String())
}

func enumFontFamExProc(elfe uintptr, _, _ uintptr, lParam uintptr) uintptr {
	if elfe == 0 {
		return 1
	}
	lf := (*win.LOGFONT)(unsafe.Pointer(elfe))
	name := logfontFaceName(lf)
	if name == "" {
		return 1
	}
	data := (*fontEnumData)(unsafe.Pointer(lParam))
	data.names[name] = struct{}{}
	return 1
}

// installedFontFamilyNames returns sorted unique font family names from GDI.
func installedFontFamilyNames() ([]string, error) {
	hdc := win.GetDC(0)
	if hdc == 0 {
		return nil, fmt.Errorf("GetDC(0) failed")
	}
	defer win.ReleaseDC(0, hdc)

	var lf win.LOGFONT
	lf.LfCharSet = win.DEFAULT_CHARSET

	data := &fontEnumData{names: make(map[string]struct{})}
	cb := syscall.NewCallback(enumFontFamExProc)

	gdi32 := windows.NewLazySystemDLL("gdi32.dll")
	proc := gdi32.NewProc("EnumFontFamiliesExW")
	r1, _, callErr := proc.Call(uintptr(hdc), uintptr(unsafe.Pointer(&lf)), cb, uintptr(unsafe.Pointer(data)), 0)
	if int32(r1) == -1 {
		return nil, fmt.Errorf("EnumFontFamiliesExW failed: %v", callErr)
	}

	out := make([]string, 0, len(data.names))
	for n := range data.names {
		out = append(out, n)
	}
	sort.Strings(out)
	return out, nil
}

func mergeFontModel(installed []string, mustInclude ...string) []string {
	m := make(map[string]struct{})
	for _, n := range installed {
		n = strings.TrimSpace(n)
		if n != "" {
			m[n] = struct{}{}
		}
	}
	for _, n := range mustInclude {
		n = strings.TrimSpace(n)
		if n != "" {
			m[n] = struct{}{}
		}
	}
	out := make([]string, 0, len(m))
	for n := range m {
		out = append(out, n)
	}
	sort.Strings(out)
	return out
}
