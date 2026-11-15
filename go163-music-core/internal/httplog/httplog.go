// Package httplog 在 http.DefaultTransport 上挂一个日志 RoundTripper，
// 用于观察歌单列表 / 歌曲列表等 NCM 请求的 URL、响应码与下载长度。
//
// netease-music 库经 go-musicfox/requests 发请求，后者用 &http.Client{}
// （Transport 为空 = http.DefaultTransport），因此 Install 一处即可覆盖
// qlmusic 与 qskmusic 两个前端（都经 libncm 调用）。
package httplog

import (
	"context"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

var once sync.Once

// enabled 请求日志开关：默认开启；可用 ncm_set_http_log() 或环境变量
// NCM_HTTP_LOG=0/off/false 动态关闭。
var enabled atomic.Bool

/* 超时矩阵（2026-11 定案，数值为常见实践值 ×2）：
 *   A1 元数据 API : 总 60s（连接 10s / TLS 10s / 响应头 20s）
 *   A2 取链 API   : 总 30s（路径含 /player/url）
 *   媒体/CDN      : 不设总超时（下载侧靠停滞超时）
 * 可用 NCM_HTTP_TIMEOUT / NCM_HTTP_TIMEOUT_RESOLVE（秒）覆盖（0 关闭），
 * 或 ncm_set_http_timeout() / ncm_set_http_timeout_resolve() 动态调整。 */
var (
	timeoutTotal   atomic.Int64
	timeoutResolve atomic.Int64
)

func init() {
	enabled.Store(true)
	timeoutTotal.Store(int64(60 * time.Second))
	timeoutResolve.Store(int64(30 * time.Second))
	if v, ok := os.LookupEnv("NCM_HTTP_TIMEOUT"); ok {
		if n, err := strconv.Atoi(strings.TrimSpace(v)); err == nil && n >= 0 {
			timeoutTotal.Store(int64(time.Duration(n) * time.Second))
		}
	}
	if v, ok := os.LookupEnv("NCM_HTTP_TIMEOUT_RESOLVE"); ok {
		if n, err := strconv.Atoi(strings.TrimSpace(v)); err == nil && n >= 0 {
			timeoutResolve.Store(int64(time.Duration(n) * time.Second))
		}
	}
}

// SetTimeout 调整元数据 API 总超时（0 = 不限制）。
func SetTimeout(d time.Duration) { timeoutTotal.Store(int64(d)) }

// SetTimeoutResolve 调整取链 API 总超时（0 = 不限制）。
func SetTimeoutResolve(d time.Duration) { timeoutResolve.Store(int64(d)) }

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
		base, _ := http.DefaultTransport.(*http.Transport)
		var rt http.RoundTripper = http.DefaultTransport
		if base != nil {
			tr := base.Clone()
			tr.DialContext = (&net.Dialer{
				Timeout:   10 * time.Second,
				KeepAlive: 30 * time.Second,
			}).DialContext
			tr.TLSHandshakeTimeout = 10 * time.Second
			tr.ResponseHeaderTimeout = 20 * time.Second
			tr.IdleConnTimeout = 90 * time.Second
			rt = tr
		}
		http.DefaultTransport = &transport{base: rt}
	})
}

type transport struct {
	base http.RoundTripper
}

func (t *transport) RoundTrip(req *http.Request) (*http.Response, error) {
	/* 统一超时注入：库内部用 &http.Client{}（无 Timeout），
	 * 这里给每个请求补 context deadline（已有 deadline 则不覆盖）。 */
	timeoutNow := time.Duration(timeoutTotal.Load())
	if strings.Contains(req.URL.Path, "/player/url") {
		timeoutNow = time.Duration(timeoutResolve.Load()) /* 取链 fail-fast */
	}
	limit := timeoutNow
	/* 仅 API 请求（*.music.163.com）注入超时；媒体/CDN 不设上限 */
	if !isAPICall(req.URL.Hostname()) {
		limit = 0
	}
	cancel := func() {}
	if limit > 0 {
		if _, has := req.Context().Deadline(); !has {
			ctx, c := context.WithTimeout(req.Context(), limit)
			req = req.Clone(ctx)
			cancel = c
		}
	}
	start := time.Now()
	resp, err := t.base.RoundTrip(req)
	if err != nil {
		cancel()
		if errors.Is(err, context.DeadlineExceeded) {
			log.Printf("[http] %s %s TIMEOUT after %s",
				req.Method, req.URL.String(), limit)
			return resp, fmt.Errorf("请求超时(%s): %w", limit, err)
		}
		if enabled.Load() {
			log.Printf("[http] %s %s err=%v (%dms)",
				req.Method, req.URL.String(), err,
				time.Since(start).Milliseconds())
		}
		return resp, err
	}
	body := &countingBody{rc: resp.Body}
	if enabled.Load() {
		body.method = req.Method
		body.url = req.URL.String()
		body.code = resp.StatusCode
		body.start = start
	}
	body.cancel = &cancel
	resp.Body = body
	return resp, nil
}

// countingBody 统计实际读取字节（= 下载长度），EOF/Close 时输出日志。
/* isAPICall 判断是否网易云 API 主机（不含媒体 CDN）。 */
func isAPICall(host string) bool {
	return strings.Contains(host, "music.163.com")
}

type countingBody struct {
	cancel *func()
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
	if b.cancel != nil {
		(*b.cancel)()
	}
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
