package format

import "fmt"

// ObjectSize formats folder/file size per FS "Folder Details" / file object size rules:
// one fractional digit, space, suffix TB|GB|MB|KB; zero is "0.0 KB".
func ObjectSize(n int64) string {
	if n == 0 {
		return "0.0 KB"
	}
	const (
		KB = 1024
		MB = KB * 1024
		GB = MB * 1024
		TB = GB * 1024
	)
	var v float64
	var suf string
	switch {
	case n >= TB:
		v = float64(n) / float64(TB)
		suf = "TB"
	case n >= GB:
		v = float64(n) / float64(GB)
		suf = "GB"
	case n >= MB:
		v = float64(n) / float64(MB)
		suf = "MB"
	default:
		v = float64(n) / float64(KB)
		suf = "KB"
	}
	return fmt.Sprintf("%.1f %s", v, suf)
}
