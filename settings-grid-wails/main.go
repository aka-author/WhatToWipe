package main

import (
	"context"
	"embed"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/options/windows"
)

//go:embed all:frontend/dist
var assets embed.FS

func main() {
	app := NewSettingsApp()
	err := wails.Run(&options.App{
		Title:  "Settings",
		Width:  1200,
		Height: 760,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		Windows: &windows.Options{},
		OnStartup: func(ctx context.Context) {
			_ = ctx
		},
		Bind: []interface{}{
			app,
		},
	})
	if err != nil {
		panic(err)
	}
}
