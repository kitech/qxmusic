// Package httplog 在 http.DefaultTransport 上挂一个日志 RoundTripper，
// 用于观察歌单列表 / 歌曲列表等 NCM 请求的 URL、响应码与下载长度。
//
// netease-music 库经 go-musicfox/requests 发请求，后者用 &http.Client{}
// （Transport 为空 = http.DefaultTransport），因此 Install 一处即可覆盖
// qlmusic 与 qskmusic 两个前端（都经 libncm 调用）。
package httplog

import (
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

var once sync.Once

// enabled 请求日志开关：默认开启；可用 ncm_set_http_log() 或环境变量
// NCM_HTTP_LOG=0/off/false 动态关闭。
var enabled atomic.Bool

func init() {
	enabled.Store(true)
}

// SetEnabled 动态开启/关闭请求日志（默认开启）。
func SetEnabled(on bool) {
	enabled.Store(on)
}

// Enabled 返回当前开关状态。
func Enabled() bool {
	return enabled.Load()
}

// Install 包装 http.DefaultTransport；可重复调用，只生效一次。
func Install() {
	once.Do(func() {
		if v, ok := os.LookupEnv("NCM_HTTP_LOG"); ok {
			switch strings.ToLower(strings.TrimSpace(v)) {
			case "0", "off", "false", "no":
				enabled.Store(false)
			case "1", "on", "true", "yes":
				enabled.Store(true)
			}
		}
		http.DefaultTransport = &transport{base: http.DefaultTransport}
	})
}

type transport struct {
	base http.RoundTripper
}

func (t *transport) RoundTrip(req *http.Request) (*http.Response, error) {
	if !enabled.Load() {
		return t.base.RoundTrip(req)
	}
	start := time.Now()
	resp, err := t.base.RoundTrip(req)
	if err != nil {
		log.Printf("[http] %s %s err=%v (%dms)",
			req.Method, req.URL.String(), err,
			time.Since(start).Milliseconds())
		return resp, err
	}
	resp.Body = &countingBody{
		rc:     resp.Body,
		method: req.Method,
		url:    req.URL.String(),
		code:   resp.StatusCode,
		start:  start,
	}
	return resp, nil
}

// countingBody 统计实际读取字节（= 下载长度），EOF/Close 时输出日志。
type countingBody struct {
	rc     io.ReadCloser
	n      int64
	method string
	url    string
	code   int
	start  time.Time
	logged sync.Once
}

func (b *countingBody) Read(p []byte) (int, error) {
	n, err := b.rc.Read(p)
	b.n += int64(n)
	if err != nil {
		b.report()
	}
	return n, err
}

func (b *countingBody) Close() error {
	b.report()
	return b.rc.Close()
}

func (b *countingBody) report() {
	b.logged.Do(func() {
		log.Printf("[http] %s %s -> %d bytes=%d (%dms)",
			b.method, b.url, b.code, b.n,
			time.Since(b.start).Milliseconds())
	})
}
