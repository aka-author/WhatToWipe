//go:build windows

//go:generate go run ./tools/genicons
//go:generate go run github.com/josephspurrier/goversioninfo/cmd/goversioninfo@v1.4.1 -64=true

package main

import (
	"os"
	"runtime"

	"github.com/lxn/walk"

	"eraserewrite/win-go/internal/ui"
)

func init() {
	runtime.LockOSThread()
}

func main() {
	if err := ui.Run(); err != nil {
		walk.MsgBox(nil, "Erase & Rewrite", err.Error(), walk.MsgBoxIconError)
		os.Exit(1)
	}
}
