//go:build windows

//go:generate go run ./tools/genicons
//go:generate go run github.com/josephspurrier/goversioninfo/cmd/goversioninfo@v1.4.1 -64=true

package main

import (
	"os"

	"github.com/lxn/walk"

	"whatrwipe/win-go/internal/ui"
)

func main() {
	if err := ui.Run(); err != nil {
		walk.MsgBox(nil, "WhatToWipe", err.Error(), walk.MsgBoxIconError)
		os.Exit(1)
	}
}
