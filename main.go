package main

import (
	"embed"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"

	"AudioPipe/bridge"
)

//go:embed all:app/dist
var assets embed.FS

func main() {
	// 创建应用程序结构的实例
	app := bridge.NewApp()

	// 创建应用程序实例并设置选项
	err := wails.Run(&options.App{
		Title:  "AudioPipe",
		Width:  1024,
		Height: 768,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 27, G: 38, B: 54, A: 1},
		OnStartup:        app.Startup,
		Bind: []interface{}{
			app,
		},
	})

	if err != nil {
		println("[ERROR] ", err.Error())
	}
}
