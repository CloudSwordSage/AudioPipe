package bridge

import (
	"context"
	"fmt"
)

// APP 应用程序结构
type App struct {
	ctx context.Context
}

// NewApp 创建一个新的 App 应用程序结构
func NewApp() *App {
	return &App{}
}

// 应用程序启动时调用Startup。上下文已保存
// 所以我们可以调用运行时方法
func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx
}

// Greet 返回给定姓名的问候语
func (a *App) Greet(name string) string {
	return fmt.Sprintf("你好, %s, 该你表演了！", name)
}
