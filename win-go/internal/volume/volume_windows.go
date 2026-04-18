//go:build windows

package volume

import (
	"path/filepath"
	"strings"

	"golang.org/x/sys/windows"
)

// TotalBytes returns total capacity of the volume containing path (techspec / arch: drive share denominator).
func TotalBytes(path string) (uint64, error) {
	total, _, err := DiskSpace(path)
	return total, err
}

// DiskSpace returns total capacity and free space (bytes available to the caller) for the volume containing path.
func DiskSpace(path string) (total, free uint64, err error) {
	vol, err := volumeRoot(path)
	if err != nil {
		return 0, 0, err
	}
	p, err := windows.UTF16PtrFromString(vol)
	if err != nil {
		return 0, 0, err
	}
	var freeB, totalB uint64
	err = windows.GetDiskFreeSpaceEx(p, &freeB, &totalB, nil)
	if err != nil {
		return 0, 0, err
	}
	return totalB, freeB, nil
}

// RootForPath returns the mounted volume root for path (e.g. `D:\` for `D:\foo`).
func RootForPath(p string) (string, error) {
	return volumeRoot(p)
}

func volumeRoot(p string) (string, error) {
	p = filepath.Clean(p)
	v := filepath.VolumeName(p)
	if v == "" {
		return p + `\`, nil
	}
	return v + `\`, nil
}

// DriveLabel returns a short label for the volume (e.g. `D:` for `D:\`; UNC prefix for `\\server\share\`).
func DriveLabel(root string) string {
	root = filepath.Clean(root)
	v := filepath.VolumeName(root)
	if v != "" {
		return v
	}
	if strings.HasPrefix(root, `\\`) {
		return root
	}
	return "?"
}
