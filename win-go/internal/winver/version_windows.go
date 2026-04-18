//go:build windows

package winver

import (
	"fmt"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"
)

var (
	modversion            = syscall.NewLazyDLL("version.dll")
	procGetFileVersionInfoSizeW = modversion.NewProc("GetFileVersionInfoSizeW")
	procGetFileVersionInfoW     = modversion.NewProc("GetFileVersionInfoW")
	procVerQueryValueW          = modversion.NewProc("VerQueryValueW")
)

// FileVersionDotted returns the StringFileInfo FileVersion field for exePath (FS About must match Explorer Details).
func FileVersionDotted(exePath string) (string, error) {
	exePath, err := filepath.Abs(exePath)
	if err != nil {
		return "", err
	}
	p, err := syscall.UTF16PtrFromString(exePath)
	if err != nil {
		return "", err
	}
	var z uint32
	r, _, e := procGetFileVersionInfoSizeW.Call(uintptr(unsafe.Pointer(p)), uintptr(unsafe.Pointer(&z)))
	if r == 0 {
		return "", fmt.Errorf("GetFileVersionInfoSizeW: %w", e)
	}
	if z == 0 {
		return "", fmt.Errorf("empty version info")
	}
	buf := make([]byte, z)
	r, _, e = procGetFileVersionInfoW.Call(uintptr(unsafe.Pointer(p)), 0, uintptr(z), uintptr(unsafe.Pointer(&buf[0])))
	if r == 0 {
		return "", fmt.Errorf("GetFileVersionInfoW: %w", e)
	}

	trans := queryTranslation(buf)
	if trans == "" {
		return "", fmt.Errorf("no translation block")
	}
	sub := `\StringFileInfo\` + trans + `\FileVersion`
	sp, err := syscall.UTF16PtrFromString(sub)
	if err != nil {
		return "", err
	}
	var outPtr uintptr
	var outLen uint32
	r, _, e = procVerQueryValueW.Call(uintptr(unsafe.Pointer(&buf[0])), uintptr(unsafe.Pointer(sp)), uintptr(unsafe.Pointer(&outPtr)), uintptr(unsafe.Pointer(&outLen)))
	if r == 0 {
		return "", fmt.Errorf("VerQueryValueW FileVersion: %w", e)
	}
	if outLen < 2 || outPtr == 0 {
		return "", fmt.Errorf("missing FileVersion string")
	}
	s := syscall.UTF16ToString((*[1 << 20]uint16)(unsafe.Pointer(outPtr))[: outLen/2])
	s = strings.Trim(s, "\x00")
	if s == "" {
		return "", fmt.Errorf("empty FileVersion")
	}
	return s, nil
}

func queryTranslation(buf []byte) string {
	var transPtr uintptr
	var transLen uint32
	sp, err := syscall.UTF16PtrFromString(`\VarFileInfo\Translation`)
	if err != nil {
		return ""
	}
	r, _, _ := procVerQueryValueW.Call(uintptr(unsafe.Pointer(&buf[0])), uintptr(unsafe.Pointer(sp)), uintptr(unsafe.Pointer(&transPtr)), uintptr(unsafe.Pointer(&transLen)))
	if r == 0 || transLen < 4 || transPtr == 0 {
		return ""
	}
	langCP := (*[2]uint16)(unsafe.Pointer(transPtr))
	return fmt.Sprintf("%04X%04X", langCP[0], langCP[1])
}
