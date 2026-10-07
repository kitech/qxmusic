// Package auth 实现登录认证：Cookie 登录与二维码登录。
//
// 为什么不用 netease-music 的 service.LoginQRService：
//
//  1. 它的 GetKey() 在 JSON 解析失败时调用 log.Fatalf
//     (service/login_qr_service.go:38)，而 log.Fatalf 会 os.Exit(1)。
//     本库运行在宿主 UI 进程内，一条畸形响应就会把 UI 干掉。
//  2. CheckQR() 把 8821 原样返回，但整个包里没有任何地方把它映射成
//     "需要行为验证码"。上层只判断 code != 200 就一律当失败，
//     UI 无法区分风控与网络故障，两者的用户操作完全不同。
//  3. util.GetCsrfToken 内部同样有 log.Fatalf（util/common.go:65）。
//
// 因此这里的接口全部返回 error，不存在任何会终止进程的路径。
package auth

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/cookiejar"
	"net/url"
	"strings"
	"time"

	"github.com/go-musicfox/netease-music/util"

	"qx163/ncm/internal/errs"
)

const (
	apiUnikey  = "https://music.163.com/weapi/login/qrcode/unikey"
	apiQRCheck = "https://music.163.com/weapi/login/qrcode/client/login"
	apiLogout  = "https://music.163.com/weapi/logout"
	apiAccount = "https://music.163.com/api/nuser/account/get"

	// qaURLBase 是二维码落地页前缀，最终 URL 形如
	//   http://music.163.com/login?codekey=<unikey>&chainId=<chainId>
	qaURLBase = "http://music.163.com/login"

	musicHost = "https://music.163.com"
)

// 二维码状态码，与 ncm_qr_status 逐值对应。
const (
	QrExpired     = 800
	QrWaitScan    = 801
	QrWaitConfirm = 802
	QrOK          = 803
	QrCaptcha     = 8821
)

// QrBeginResult 是 ncm_qr_begin 的出参。
type QrBeginResult struct {
	UniKey    string `json:"unikey"`
	QrcodeURL string `json:"qrcode_url"`
}

// QrPollResult 是 ncm_qr_poll 的出参。
//
// Status 取值见下方 Qr* 常量，恒非 0：出现 Status==0 一定意味着
// 这次调用返回了 error（HTTP 层失败或响应无法解析），此时 FFI 层
// 不会分配 out_json，所以这个零值不会真的抵达 UI。
//
// 8821 必须原样传出：它意味着账号触发了行为验证码，需要 UI 提示用户
// 去网页端过验证，而不是简单重试。
type QrPollResult struct {
	Status  int32  `json:"status"`
	Message string `json:"message,omitempty"`
}

// Account 是账号信息。
type Account struct {
	Anonymous bool        `json:"anonymous"`
	Profile   Profile     `json:"profile"`
	Account   AccountInfo `json:"account"`
}

type Profile struct {
	UserID      int64  `json:"userId"`
	Nickname    string `json:"nickname"`
	AvatarURL   string `json:"avatarUrl"`
	Description string `json:"description,omitempty"`
	VipType     int32  `json:"vipType"`
}

type AccountInfo struct {
	AnonymityUser bool  `json:"anonymityUser"`
	AvatarURL     string `json:"avatarUrl"`
	UserID        int64 `json:"userId"`
	Status        int32 `json:"status"`
}

// Service 是一组无状态的认证操作。共享 netease-music 的全局 CookieJar，
// 因此需要由调用方（FFI 层的 apiMu）串行化。
type Service struct{}

// jar 返回全局 CookieJar；首次调用会顺带生成 sDeviceId。
func jar() http.CookieJar { return util.GetGlobalCookieJar() }

// StatusFromQR 把网易云的二维码状态码映射为 (status, error)。
//
// 800/801/802/803 全部是正常流程中的状态，必须返回 nil error ——
// 其中 800（二维码失效）是流程的正常终态，UI 需要拿到它才能提示"请重新获取"
// 并重新调用 ncm_qr_begin；当成错误会让 UI 无法区分"该重新扫码"与"网络故障"。
//
// 唯一的异常是 8821：它意味着账号触发了行为验证码，需要用户去网页端验证，
// 重试没有意义。
func StatusFromQR(code float64) (status int32, err error) {
	switch int(code) {
	case QrExpired, QrWaitScan, QrWaitConfirm, QrOK:
		return int32(code), nil
	case QrCaptcha:
		return QrCaptcha, errs.ErrCaptcha
	}
	return int32(code), fmt.Errorf("%w: 无法识别的二维码状态码 %v", errs.ErrUnknownCode, code)
}

// QrBegin 获取 unikey 并拼出二维码内容。
//
// 刻意不解析二维码图片：渲染属于 UI 的职责，本库只提供 qrcode_url 字符串。
func (s *Service) QrBegin() (QrBeginResult, error) {
	data := map[string]any{
		"type":         1,
		"noCheckToken": true,
	}

	code, body, err := util.CallWeapi(apiUnikey, data)
	if err != nil {
		return QrBeginResult{}, fmt.Errorf("请求 unikey 失败: %w", err)
	}
	if int(code) != 200 {
		return QrBeginResult{}, fmt.Errorf("%w: unikey 接口返回 code=%v body=%s",
			errs.ErrUpstream, code, truncate(body))
	}

	unikey, err := parseUniKey(body)
	if err != nil {
		return QrBeginResult{}, err
	}

	// chainId 是新版本新增的必填参数，缺失会导致扫码后一直 801。
	chainID := util.GenerateChainID(jar())

	return QrBeginResult{UniKey: unikey, QrcodeURL: buildQRURL(unikey, chainID)}, nil
}

// buildQRURL 拼出二维码落地页。
//
// 注意用 QueryEscape 而不是字符串拼接：unikey 来自网络响应，
// 直接拼进查询串会让响应内容有机会注入额外查询参数。
func buildQRURL(unikey, chainID string) string {
	return qaURLBase + "?codekey=" + url.QueryEscape(unikey) +
		"&chainId=" + url.QueryEscape(chainID)
}

// parseUniKey 兼容两种响应形态：顶层 unikey 与 data.unikey。
// 直接 json.Unmarshal 到 struct 会漏掉后者。
func parseUniKey(body []byte) (string, error) {
	var flat struct {
		UniKey string `json:"unikey"`
	}
	if err := json.Unmarshal(body, &flat); err == nil && flat.UniKey != "" {
		return flat.UniKey, nil
	}

	var nested struct {
		Data struct {
			UniKey string `json:"unikey"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &nested); err == nil && nested.Data.UniKey != "" {
		return nested.Data.UniKey, nil
	}

	return "", fmt.Errorf("%w: unikey 响应中没有 unikey 字段: %s",
		errs.ErrBadResponse, truncate(body))
}

// QrPoll 轮询扫码状态。
//
// 返回值约定（与 include/ncm.h 配合）：
//   - 800/801/802/803 -> (结果, nil)，由 ncm_qr_poll 返回 NCM_OK，
//     具体状态通过 out_json 的 "status" 字段传达。
//   - 8821            -> (结果, errs.ErrCaptcha)，ncm_qr_poll 返回
//     NCM_ERR_CAPTCHA 且不分配 out_json；ncm.hpp 会补成 {"status":8821}。
//   - 其它            -> 传输层错误。
//
// 每次调用前先注入反风控 cookie（os/NMTID）。缺了这一步，短时间内
// 高频轮询很容易触发 8821。
func (s *Service) QrPoll(unikey string) (QrPollResult, error) {
	if strings.TrimSpace(unikey) == "" {
		return QrPollResult{}, errs.ErrInvalidArg
	}

	j := jar()
	util.ApplyRequestStrategy(j)

	data := map[string]any{
		"type":         1,
		"noCheckToken": true,
		"key":          unikey,
	}

	_, body, err := util.CallWeapi(apiQRCheck, data, j)
	if err != nil {
		return QrPollResult{}, fmt.Errorf("轮询二维码失败: %w", err)
	}

	var envelope struct {
		Code    float64 `json:"code"`
		Message string  `json:"message"`
	}
	// 解析失败不致命：code 缺失时下面会落到"无法识别的状态码"。
	_ = json.Unmarshal(body, &envelope)

	status, qerr := StatusFromQR(envelope.Code)
	return QrPollResult{Status: status, Message: envelope.Message}, qerr
}

// LoginWithCookie 用 Cookie 登录。
//
// 只把 cookie 写进 jar 就返回成功是不够的 —— 一个过期的或格式错误的
// cookie 也会被 jar 接受。所以这里写完后立刻调 Account() 验证，
// 让 UI 拿到确定的成功或失败。
func (s *Service) LoginWithCookie(raw string) (Account, error) {
	cookies, err := parseCookieHeader(raw)
	if err != nil {
		return Account{}, err
	}
	if !hasMusicU(cookies) {
		// MUSIC_U 是唯一代表登录态的 cookie，缺了必然未登录。
		return Account{}, errs.ErrMissingMusicU
	}

	u, err := url.Parse(musicHost)
	if err != nil {
		return Account{}, fmt.Errorf("%w: %v", errs.ErrInternal, err)
	}
	jar().SetCookies(u, cookies)

	acc, err := s.Account()
	if err != nil {
		// 验证失败要清掉刚写入的 cookie，否则下次不带 cookie 的调用
		// 会带着一个残缺的登录态，看起来像"时好时坏"。
		expireCookies(u, cookies)
		return Account{}, err
	}
	return acc, nil
}

// Account 获取当前登录用户信息。
func (s *Service) Account() (Account, error) {
	// 这里用 CreateRequest 而不是 CallWeapi：/api/nuser/account/get
	// 走的是带 os/appver 头的那条路径，触发 8821 的概率明显更低。
	code, body, _ := util.CreateRequest("POST", apiAccount,
		map[string]string{}, &util.Options{Crypto: "weapi"})

	if code == 301 || code == 302 {
		return Account{}, errs.ErrNotLogin
	}
	if int(code) != 200 {
		return Account{}, fmt.Errorf("%w: account 接口返回 code=%v", errs.ErrUpstream, code)
	}

	var envelope struct {
		Code     float64     `json:"code"`
		Account  AccountInfo `json:"account"`
		Profile  Profile     `json:"profile"`
		Anonymous *bool      `json:"anonymous"`
	}
	if err := json.Unmarshal(body, &envelope); err != nil {
		return Account{}, fmt.Errorf("%w: account 响应解析失败: %v", errs.ErrBadResponse, err)
	}

	if envelope.Account.UserID == 0 && envelope.Profile.UserID == 0 {
		// 两个 id 都为 0 说明是游客态。code 仍然是 200，靠 body 判断。
		return Account{Anonymous: true}, errs.ErrNotLogin
	}

	return Account{
		Profile: envelope.Profile,
		Account: envelope.Account,
	}, nil
}

// Logout 注销登录。
func (s *Service) Logout() error {
	csrf := csrfToken()
	data := map[string]any{"csrf_token": csrf}

	_, body, err := util.CallWeapi(apiLogout, data)
	if err != nil {
		return fmt.Errorf("注销失败: %w", err)
	}

	var envelope struct {
		Code float64 `json:"code"`
	}
	_ = json.Unmarshal(body, &envelope)
	if int(envelope.Code) != 200 {
		return fmt.Errorf("%w: logout 返回 code=%v", errs.ErrUpstream, envelope.Code)
	}

	// 主动清掉登录态。服务端注销成功但本地仍保留 MUSIC_U 的话，
	// 下次启动会表现成"已登录但所有请求都 301"。
	clearMusicU()
	return nil
}

// --- cookie 辅助 ---

// csrfToken 自己从 jar 里读 __csrf，不用 util.GetCsrfToken（内有 log.Fatalf）。
func csrfToken() string {
	u, err := url.Parse(musicHost)
	if err != nil {
		return ""
	}
	for _, c := range jar().Cookies(u) {
		if c.Name == "__csrf" {
			return c.Value
		}
	}
	return ""
}

// clearMusicU 让 MUSIC_U 立即过期。
func clearMusicU() {
	u, err := url.Parse(musicHost)
	if err != nil {
		return
	}
	jar().SetCookies(u, []*http.Cookie{{
		Name:    "MUSIC_U",
		Value:   "",
		MaxAge:  -1,
		Expires: time.Unix(0, 0),
		Path:    "/",
	}})
}

func expireCookies(u *url.URL, cookies []*http.Cookie) {
	for _, c := range cookies {
		if c.Name != "MUSIC_U" {
			continue
		}
		jar().SetCookies(u, []*http.Cookie{{
			Name:    "MUSIC_U",
			Value:   "",
			MaxAge:  -1,
			Expires: time.Unix(0, 0),
			Path:    "/",
		}})
	}
}

// parseCookieHeader 解析 "MUSIC_U=xx; __csrf=yy" 形式的 Cookie 串。
//
// 复用 http.ReadSetCookies 是做不到的（那是 Set-Cookie 头格式）。
// 这里手写解析，但对空值、缺 "="、多余空白都做了防御 ——
// 用户粘贴的 cookie 字符串经常是脏的。
func parseCookieHeader(raw string) ([]*http.Cookie, error) {
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return nil, errs.ErrInvalidArg
	}

	var out []*http.Cookie
	for _, part := range strings.Split(raw, ";") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		eq := strings.IndexByte(part, '=')
		if eq <= 0 {
			// 没有 '='，或者以 '=' 开头（空名字）——跳过而不是报错，
			// 因为一个脏片段不该让整串 cookie 作废。
			continue
		}
		name := strings.TrimSpace(part[:eq])
		value := strings.TrimSpace(part[eq+1:])
		if name == "" {
			continue
		}
		out = append(out, &http.Cookie{Name: name, Value: value, Path: "/"})
	}

	if len(out) == 0 {
		return nil, fmt.Errorf("%w: cookie 串解析后为空", errs.ErrInvalidArg)
	}
	return out, nil
}

func hasMusicU(cookies []*http.Cookie) bool {
	for _, c := range cookies {
		if c.Name == "MUSIC_U" && c.Value != "" {
			return true
		}
	}
	return false
}

// newJar 替换全局 CookieJar，仅供测试隔离用例。
// 不导出：生产代码没有任何调用方，而 test 同行包本就能访问小写符号。
func newJar() error {
	j, err := cookiejar.New(nil)
	if err != nil {
		return err
	}
	util.SetGlobalCookieJar(j)
	return nil
}

// truncate 截断响应体，避免把整个 HTML 错误页塞进错误信息。
func truncate(b []byte) string {
	const max = 512
	s := strings.TrimSpace(string(b))
	if len(s) <= max {
		return s
	}
	return s[:max] + "...(truncated)"
}