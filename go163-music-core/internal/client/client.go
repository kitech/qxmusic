// Package client 是与 C ABI 解耦的会话容器。它不认识 cgo 类型，
// 便于单独编写单元测试；FFI 胶水层负责翻译。
package client

import (
	"encoding/json"
	"sync"
)

// 事件编号，与 include/ncm.h 中的 ncm_event 保持一致。
const (
	EventProgress = 1
	EventLogin    = 2
	EventPlayFail = 3
)

// SourceType 表示播放源优先级，与 ncm_source_type 保持一致。
type SourceType int32

const (
	SourceRemote     SourceType = 0
	SourceCached     SourceType = 1
	SourceDownloaded SourceType = 2
)

func (t SourceType) String() string {
	switch t {
	case SourceDownloaded:
		return "downloaded"
	case SourceCached:
		return "cached"
	default:
		return "remote"
	}
}

// Options 是构造 Client 所需的配置。
type Options struct {
	// ConfigDir 是配置与缓存根目录。为空时由各实现自行决定默认值。
	ConfigDir string

	// UNMProxyURL 为空表示不启用外部 UNM 代理。
	// 进程内 UNM 不需要该项。
	UNMProxyURL string
}

// EventSink 接收对外事件。payload 是已经序列化好的 JSON。
// 实现方必须是非阻塞的：它运行在库内部持有的锁之外，但仍在
// 库的 goroutine 上。
type EventSink func(event int32, payload []byte)

// Client 持有一个会话的共享状态。
//
// 并发约定：导出函数通过 FFI 层的全局互斥量串行化访问本对象，
// Client 自身只对可变字段（事件回调、关闭标记）加锁。
type Client struct {
	opts Options

	mu     sync.RWMutex
	sink   EventSink
	closed bool
}

// New 创建 Client。
func New(opts Options) *Client {
	return &Client{opts: opts}
}

// ConfigDir 返回构造时传入的配置目录。
func (c *Client) ConfigDir() string { return c.opts.ConfigDir }

// UNMProxyURL 返回构造时传入的 UNM 代理地址，可能为空串。
func (c *Client) UNMProxyURL() string { return c.opts.UNMProxyURL }

// SetEventSink 设置事件回调，传 nil 表示取消订阅。
func (c *Client) SetEventSink(fn EventSink) {
	c.mu.Lock()
	c.sink = fn
	c.mu.Unlock()
}

// Emit 序列化并派发一个事件。序列化失败时静默丢弃 —— 事件通道不应该
// 因为 payload 构造问题而影响主流程。
func (c *Client) Emit(event int32, payload any) {
	c.mu.RLock()
	sink, closed := c.sink, c.closed
	c.mu.RUnlock()
	if sink == nil || closed {
		return
	}
	b, err := json.Marshal(payload)
	if err != nil {
		return
	}
	sink(event, b)
}

// Close 标记会话关闭并丢弃事件回调。重复调用是安全的。
func (c *Client) Close() {
	c.mu.Lock()
	c.closed = true
	c.sink = nil
	c.mu.Unlock()
}

// Closed 报告会话是否已关闭。
func (c *Client) Closed() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.closed
}