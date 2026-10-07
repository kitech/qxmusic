// Command ncmffi 通过 cgo 以 buildmode=c-shared 产出 libncm。
//
// 本文件只做三件事：句柄管理、JSON 编解码、错误映射。
// 所有业务逻辑放在 internal/ 下，便于脱离 cgo 单测。
//
// 关键约束（违反即 segfault 或泄漏）：
//
//  1. Go string 绝不跨边界。入参用 C.CString 复制并及时 C.free，
//     出参统一写成 *C.char，由 C.free 回收（对外即 ncm_free_string）。
//  2. C 不得持有 Go 指针。因此句柄是 C.malloc 出来的内存，内容是一个
//     cgo.Handle（本质 uintptr_t），不是 Go 指针。
//  3. 回调只允许单向 Go -> C。且 apiMu 绝不能在回调期间持有，
//     否则下载这类"持锁期间派发事件"的路径会自锁。
//  4. 每个导出函数都要 recover：宿主是 UI 进程，不能被 Go 的 panic 打死。
package main

/*
#cgo CFLAGS: -I${SRCDIR}/../include -Wall -Wextra -Wno-unused-parameter
#include "abi.h"
*/
import "C"

import (
	"encoding/json"
	"runtime/cgo"
	"sync"
	"unsafe"

	"qx163/ncm/internal/auth"
	"qx163/ncm/internal/client"
	"qx163/ncm/internal/errs"
	"qx163/ncm/internal/version"
)

const clientMagic = C.NCM_CLIENT_MAGIC

// apiMu 串行化所有对同一进程内共享状态的访问：netease-music 的全局
// CookieJar、UNM processor、配置都是进程级单例。
var apiMu sync.Mutex

// registry 把 cgo.Handle 映射到会话对象。
var registry sync.Map

// ffiClient 是跨边界的会话句柄内容。
type ffiClient struct {
	inner *client.Client
	auth  *auth.Service

	// cbMu 只保护下面两个字段，与 apiMu 分开持有，避免事件派发时死锁。
	cbMu sync.Mutex
	cb   C.ncm_event_fn
	ud   unsafe.Pointer
}

// lookup 校验并取出句柄对应的会话。magic 校验用于识别野指针，
// 从而让 ncm_free 可以安全地提前返回而不 double free。
func lookup(h *C.ncm_client) *ffiClient {
	if h == nil || uint32(h.magic) != uint32(clientMagic) {
		return nil
	}
	ch := cgo.Handle(h.handle)
	v, ok := registry.Load(ch)
	if !ok {
		return nil
	}
	fc, _ := v.(*ffiClient)
	return fc
}

// guard 捕获 panic，把 C 边界上的崩溃转成 NCM_ERR_INTERNAL。
func guard(fn func() C.int32_t) (ret C.int32_t) {
	defer func() {
		if r := recover(); r != nil {
			ret = C.int32_t(errs.Internal)
		}
	}()
	return fn()
}

// guardVoid 是无返回值导出函数的等价物。
func guardVoid(fn func()) {
	defer func() { _ = recover() }()
	fn()
}

// writeJSON 序列化到 *C.char 出参。调用方负责在失败时不释放。
func writeJSON(out **C.char, v any) C.int32_t {
	if out == nil {
		return C.int32_t(errs.InvalidArg)
	}
	*out = nil
	b, err := json.Marshal(v)
	if err != nil {
		return C.int32_t(errs.Internal)
	}
	*out = C.CString(string(b))
	return C.int32_t(errs.Ok)
}

// badHandle 是无效/已释放句柄的统一响应。
//
// 必须与 NotImplemented 区分开：NULL 句柄是调用方的 bug（UI 没保存好
// client 指针），而 NotImplemented 表示这个接口还没实现。混为一谈会让
// UI 侧把编程错误误判成"功能缺失"而静默降级。
func badHandle(out **C.char) C.int32_t {
	if out != nil {
		*out = nil
	}
	return C.int32_t(errs.InvalidArg)
}

// stub 是 P2 及以后功能的占位实现。ABI 在 P1 一次性冻结，
// 这样 UI 侧可以先按最终签名开发，不必等后端补齐。
func stub(out **C.char) C.int32_t {
	if out != nil {
		*out = nil
	}
	return C.int32_t(errs.NotImplemented)
}

// fail 是"业务逻辑返回了 error"时的统一出口。
//
// 关键点：出错时不分配 out_json，与 ncm.h 第 3 条一致。
// 若这里误分配，调用方的 fetch() 会把上一次的残留内容当成新结果。
func fail(out **C.char, err error) C.int32_t {
	if out != nil {
		*out = nil
	}
	return C.int32_t(errs.StatusOf(err))
}

// emitLogin 派发登录态变化事件。ok=false 时把错误原因也带上，
// 便于 UI 在未登录场景下区分"用户主动退出"与"登录态失效"。
func (fc *ffiClient) emitLogin(ok bool, err error) {
	reason := ""
	if err != nil {
		reason = err.Error()
	}
	fc.inner.Emit(client.EventLogin, map[string]any{
		"logged_in": ok,
		"reason":    reason,
	})
}

//export ncm_new
func ncm_new(configDir *C.char) (ret *C.ncm_client) {
	guard(func() C.int32_t {
		dir := ""
		if configDir != nil {
			dir = C.GoString(configDir)
		}

		fc := &ffiClient{
			inner: client.New(client.Options{ConfigDir: dir}),
			auth:  &auth.Service{},
		}
		fc.inner.SetEventSink(fc.dispatch)

		h := cgo.NewHandle(fc)
		p := C.malloc(C.size_t(unsafe.Sizeof(C.ncm_client{})))
		if p == nil {
			h.Delete()
			return C.int32_t(errs.Internal)
		}
		c := (*C.ncm_client)(p)
		c.handle = C.uintptr_t(h)
		c.magic = C.uint32_t(clientMagic)
		registry.Store(h, fc)
		ret = c
		return C.int32_t(errs.Ok)
	})
	return ret
}

// dispatch 把内部事件转发给订阅者。
//
// 注意时序：本函数自身不取 apiMu，但会被"持有 apiMu 的路径"调用
// （例如 ncm_download 在下载过程中逐次 Emit 进度）。也就是说，回调是在
// apiMu 仍然被持有的情况下执行的。因此 C 回调里同步调用本库的任何函数
// 都会死锁 —— 这就是 ncm.h 第 5 条 ABI 约定要求 UI 侧把回调内容投递到
// 自己的事件循环的原因。
//
// 另：这里只取 cbMu（而不复用 apiMu），否则事件派发与 API 调用会互相等待。
func (fc *ffiClient) dispatch(event int32, payload []byte) {
	fc.cbMu.Lock()
	cb, ud := fc.cb, fc.ud
	fc.cbMu.Unlock()
	if cb == nil {
		return
	}
	s := C.CString(string(payload))
	defer C.free(unsafe.Pointer(s))
	C.ncm_invoke_event(cb, ud, C.int32_t(event), s)
}

//export ncm_free
func ncm_free(h *C.ncm_client) {
	// 必须持有 apiMu：否则 UI 线程在另一个线程执行 ncm_qr_poll 期间
	// 调用 ncm_free，会把句柄内存释放掉，让对方读到野指针。
	apiMu.Lock()
	defer apiMu.Unlock()
	guardVoid(func() {
		if h == nil || uint32(h.magic) != uint32(clientMagic) {
			return
		}
		h.magic = 0 // 先失效，使重复调用变成空操作
		ch := cgo.Handle(h.handle)
		if v, ok := registry.LoadAndDelete(ch); ok {
			if fc, ok := v.(*ffiClient); ok {
				fc.inner.Close()
				fc.cbMu.Lock()
				fc.cb = nil
				fc.cbMu.Unlock()
			}
		}
		ch.Delete()
		C.free(unsafe.Pointer(h))
	})
}

//export ncm_free_string
func ncm_free_string(s *C.char) {
	guardVoid(func() {
		if s == nil {
			return
		}
		C.free(unsafe.Pointer(s))
	})
}

//export ncm_version
func ncm_version() (ret *C.char) {
	guardVoid(func() { ret = C.CString(version.String()) })
	return ret
}

//export ncm_set_event_cb
func ncm_set_event_cb(h *C.ncm_client, cb C.ncm_event_fn, ud unsafe.Pointer) {
	// 与 ncm_free 同理：读句柄内存必须与 ncm_free 互斥。
	apiMu.Lock()
	defer apiMu.Unlock()
	guardVoid(func() {
		fc := lookup(h)
		if fc == nil {
			return
		}
		fc.cbMu.Lock()
		fc.cb, fc.ud = cb, ud
		fc.cbMu.Unlock()
	})
}

// ---- 认证 ----

// ncm_login_cookie 用 Cookie 登录。
//
// 出参: 完整的 Account JSON（anonymous/profile/account）。
// 写完 cookie 后会立即向服务端验证，避免把过期 cookie 当成登录成功。
//
//export ncm_login_cookie
func ncm_login_cookie(h *C.ncm_client, cookie *C.char, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		fc := lookup(h)
		if fc == nil {
			return badHandle(out)
		}
		if cookie == nil {
			return badHandle(out)
		}

		acc, err := fc.auth.LoginWithCookie(C.GoString(cookie))
		if err != nil {
			fc.emitLogin(false, err)
			return fail(out, err)
		}
		fc.emitLogin(true, nil)
		return writeJSON(out, acc)
	})
}

// ncm_qr_begin 获取登录二维码。
//
// 出参: {"unikey":"...","qrcode_url":"http://music.163.com/login?codekey=..&chainId=.."}
// UI 只需把 qrcode_url 渲染成二维码。
//
//export ncm_qr_begin
func ncm_qr_begin(h *C.ncm_client, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		fc := lookup(h)
		if fc == nil {
			return badHandle(out)
		}

		res, err := fc.auth.QrBegin()
		if err != nil {
			return fail(out, err)
		}
		return writeJSON(out, res)
	})
}

// ncm_qr_poll 轮询扫码状态。
//
// 返回码语义（务必按 ncm.h 第 3 条与 ncm.hpp 的约定理解）:
//   - NCM_OK            -> 800/801/802/803，出参含 "status" 字段
//   - NCM_ERR_CAPTCHA   -> 8821 触发风控，不分配出参（hpp 会补 {"status":8821}）
//   - 其它非 0          -> 传输层/解析失败
//
//export ncm_qr_poll
func ncm_qr_poll(h *C.ncm_client, unikey *C.char, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		fc := lookup(h)
		if fc == nil {
			return badHandle(out)
		}
		if unikey == nil {
			return badHandle(out)
		}

		res, err := fc.auth.QrPoll(C.GoString(unikey))
		if err != nil {
			// 8821 属于风控：状态码已放进 res.Status，这里只负责
			// 返回正确的 ncm_status 并丢弃出参（见函数注释）。
			return fail(out, err)
		}

		if res.Status == auth.QrOK {
			// 登录成功，顺带取一次账号信息让 UI 不用再调一次 ncm_account。
			if acc, aerr := fc.auth.Account(); aerr == nil {
				fc.emitLogin(true, nil)
				return writeJSON(out, struct {
					auth.QrPollResult
					Account auth.Account `json:"account"`
				}{res, acc})
			}
			fc.emitLogin(true, nil)
		}
		return writeJSON(out, res)
	})
}

// ncm_account 获取当前登录用户信息。
//
//export ncm_account
func ncm_account(h *C.ncm_client, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		fc := lookup(h)
		if fc == nil {
			return badHandle(out)
		}

		acc, err := fc.auth.Account()
		if err != nil {
			return fail(out, err)
		}
		return writeJSON(out, acc)
	})
}

// ncm_logout 注销登录，并清除本地 MUSIC_U。
//
//export ncm_logout
func ncm_logout(h *C.ncm_client) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		fc := lookup(h)
		if fc == nil {
			return C.int32_t(errs.InvalidArg)
		}
		if err := fc.auth.Logout(); err != nil {
			fc.emitLogin(false, err)
			return C.int32_t(errs.StatusOf(err))
		}
		fc.emitLogin(false, nil)
		return C.int32_t(errs.Ok)
	})
}

// ---- 目录 ----

//export ncm_daily_recommend
func ncm_daily_recommend(h *C.ncm_client, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		return stub(out)
	})
}

//export ncm_playlist_tracks
func ncm_playlist_tracks(h *C.ncm_client, id C.int64_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if id <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

//export ncm_album_tracks
func ncm_album_tracks(h *C.ncm_client, id C.int64_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if id <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

//export ncm_search
func ncm_search(h *C.ncm_client, keywords *C.char, limit C.int32_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if keywords == nil {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		if limit <= 0 {
			limit = 30
		}
		return stub(out)
	})
}

// ---- 播放 ----

//export ncm_resolve_source
func ncm_resolve_source(h *C.ncm_client, songID C.int64_t, quality C.int32_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if songID <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		if !validQuality(quality) {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

//export ncm_download
func ncm_download(h *C.ncm_client, songID C.int64_t, quality C.int32_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if songID <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

//export ncm_outer_url
func ncm_outer_url(h *C.ncm_client, songID C.int64_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if songID <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

//export ncm_clear_cache
func ncm_clear_cache(h *C.ncm_client) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return C.int32_t(errs.InvalidArg)
		}
		return C.int32_t(errs.NotImplemented)
	})
}

// ---- 歌词 ----

//export ncm_lyric
func ncm_lyric(h *C.ncm_client, songID C.int64_t, out **C.char) C.int32_t {
	apiMu.Lock()
	defer apiMu.Unlock()
	return guard(func() C.int32_t {
		if lookup(h) == nil {
			return badHandle(out)
		}
		if songID <= 0 {
			if out != nil {
				*out = nil
			}
			return C.int32_t(errs.InvalidArg)
		}
		return stub(out)
	})
}

// validQuality 校验音质枚举。母带/环绕声三档在 netease-music 的 brMap 中
// 缺失，会静默回落到 320000；这里提前拦截，避免 UI 以为自己拿到了母带。
func validQuality(q C.int32_t) bool {
	switch q {
	case C.NCM_QUALITY_STANDARD, C.NCM_QUALITY_HIGHER, C.NCM_QUALITY_EXHIGH,
		C.NCM_QUALITY_LOSSLESS, C.NCM_QUALITY_HIRES, C.NCM_QUALITY_JYEFFECT,
		C.NCM_QUALITY_SKY, C.NCM_QUALITY_JYMASTER:
		return true
	}
	return false
}

// main 不会被调用：buildmode=c-shared 下产物是动态库，没有可执行入口。
// 但 cgo 仍要求包内有 func main。
func main() {}