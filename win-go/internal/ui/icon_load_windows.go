//go:build windows

package ui

import (
	"os"

	"github.com/lxn/walk"
)

func loadEmbeddedAppIcon() (*walk.Icon, error) {
	if len(appICOEmbed) == 0 {
		return nil, os.ErrNotExist
	}
	f, err := os.CreateTemp("", "eraserewrite-app-*.ico")
	if err != nil {
		return nil, err
	}
	path := f.Name()
	if _, err := f.Write(appICOEmbed); err != nil {
		f.Close()
		os.Remove(path)
		return nil, err
	}
	if err := f.Close(); err != nil {
		os.Remove(path)
		return nil, err
	}
	ic, err := walk.NewIconFromFile(path)
	_ = os.Remove(path)
	return ic, err
}
