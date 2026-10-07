package auth

import (
	"encoding/json"
	"errors"
	"net/http"
	"net/url"
	"strings"
	"testing"

	"qx163/ncm/internal/errs"
)

func mustJSON(t *testing.T, v any) string {
	t.Helper()
	b, err := json.Marshal(v)
	if err != nil {
		t.Fatalf("序列化失败: %v", err)
	}
	return string(b)
}

// 覆盖的是三类"肉眼保证不了"的性质：
//  1. 脏输入不 panic（cookie 来自配置文件/剪贴板，是不可信输入）
//  2. 错误能穿过 errors.Is/Wrap 保留语义（状态码映射依赖它）
//  3. QR 状态码的语义边界，尤其 800 不能被当成错误

func TestParseCookieHeaderSkipsDirtySegments(t *testing.T) {
	// 一个脏片段不该让整串 cookie 作废：用户粘贴的 cookie 常常带
	// 换行、多余分号、"Set-Cookie:" 前缀等噪音。
	raw := "os=pc; 垃圾片段; appver=8.9; =空名; __csrf=xyz; MUSIC_U=abc"
	cookies, err := parseCookieHeader(raw)
	if err != nil {
		t.Fatalf("parseCookieHeader: %v", err)
	}

	want := map[string]string{"os": "pc", "appver": "8.9", "__csrf": "xyz", "MUSIC_U": "abc"}
	for _, c := range cookies {
		delete(want, c.Name)
	}
	if len(want) != 0 {
		t.Errorf("以下 cookie 未被解析出来: %v", want)
	}
	if len(cookies) != 4 {
		t.Errorf("得到 %d 个 cookie, 期望 4 个（脏片段应被跳过）: %v", len(cookies), cookies)
	}
}

func TestParseCookieHeaderRejectsUnusable(t *testing.T) {
	// 解析后为空 => 无法构成任何登录信息，必须报错
	for _, raw := range []string{"", "   ", "垃圾", "=", "=值", "; ;"} {
		if _, err := parseCookieHeader(raw); !errors.Is(err, errs.ErrInvalidArg) {
			t.Errorf("parseCookieHeader(%q) 应返回 ErrInvalidArg, 得到 %v", raw, err)
		}
	}
}

func TestParseCookieHeaderEmptyValueAllowed(t *testing.T) {
	// "name=" 合法：清 cookie 正是靠空值表达的，不该报错
	cookies, err := parseCookieHeader("name=")
	if err != nil {
		t.Fatalf("空值 cookie 应被接受: %v", err)
	}
	if len(cookies) != 1 || cookies[0].Value != "" {
		t.Errorf("得到 %v, 期望 1 个值为空的 cookie", cookies)
	}
}

func TestHasMusicU(t *testing.T) {
	cases := []struct {
		name  string
		input []*http.Cookie
		want  bool
	}{
		{"有 MUSIC_U", []*http.Cookie{{Name: "MUSIC_U", Value: "v"}}, true},
		{"只有空 MUSIC_U", []*http.Cookie{{Name: "MUSIC_U", Value: ""}}, false},
		{"大小写不同", []*http.Cookie{{Name: "music_u", Value: "v"}}, false},
		{"只有 csrf", []*http.Cookie{{Name: "__csrf", Value: "v"}}, false},
		{"空列表", nil, false},
	}
	for _, c := range cases {
		if got := hasMusicU(c.input); got != c.want {
			t.Errorf("hasMusicU(%s) = %v, 期望 %v", c.name, got, c.want)
		}
	}
}

func TestParseUniKey(t *testing.T) {
	cases := []struct {
		name string
		body string
		want string
	}{
		{"顶层", `{"code":200,"unikey":"uk-1"}`, "uk-1"},
		{"data 内", `{"code":200,"data":{"unikey":"uk-2"}}`, "uk-2"},
		// 直接 Unmarshal 到 struct 会漏掉 data.unikey，这正是要兼容的原因
		{"两个都有时优先顶层", `{"unikey":"flat","data":{"unikey":"nested"}}`, "flat"},
		{"空 unikey 视为无", `{"code":200,"unikey":""}`, ""},
	}
	for _, c := range cases {
		got, err := parseUniKey([]byte(c.body))
		if c.want == "" {
			if !errors.Is(err, errs.ErrBadResponse) {
				t.Errorf("%s: 应返回 ErrBadResponse, 得到 %v", c.name, err)
			}
			continue
		}
		if err != nil {
			t.Errorf("%s: %v", c.name, err)
			continue
		}
		if got != c.want {
			t.Errorf("%s: unikey = %q, 期望 %q", c.name, got, c.want)
		}
	}
}

func TestParseUniKeyRejectsNonJSON(t *testing.T) {
	// 上游返回 HTML 错误页时不能 panic，也不能返回空串当成功
	for _, body := range []string{"<html>502</html>", "", "{", "null"} {
		if _, err := parseUniKey([]byte(body)); !errors.Is(err, errs.ErrBadResponse) {
			t.Errorf("parseUniKey(%q) 应返回 ErrBadResponse, 得到 %v", body, err)
		}
	}
}

func TestStatusFromQR(t *testing.T) {
	tests := []struct {
		code      float64
		wantState int32
		wantErr   error
	}{
		{800, QrExpired, nil},
		{801, QrWaitScan, nil},
		{802, QrWaitConfirm, nil},
		{803, QrOK, nil},
		{8821, QrCaptcha, errs.ErrCaptcha},
		{999, 999, errs.ErrUnknownCode},
		{0, 0, errs.ErrUnknownCode},
	}
	for _, tt := range tests {
		got, err := StatusFromQR(tt.code)
		if got != tt.wantState {
			t.Errorf("StatusFromQR(%v) state = %d, 期望 %d", tt.code, got, tt.wantState)
		}
		if tt.wantErr == nil && err != nil {
			t.Errorf("StatusFromQR(%v) 不应返回 error, 得到 %v", tt.code, err)
		}
		if tt.wantErr != nil && !errors.Is(err, tt.wantErr) {
			t.Errorf("StatusFromQR(%v) error = %v, 期望 %v", tt.code, err, tt.wantErr)
		}
	}
}

// 单独强调 800：把它当错误会让 UI 无法区分"该重新扫码"和"网络挂了"。
func TestQRExpiredIsNotAnError(t *testing.T) {
	if _, err := StatusFromQR(800); err != nil {
		t.Fatalf("800（二维码失效）必须是 nil error, 得到 %v", err)
	}
}

// 8821 必须与其它状态区分开：它是唯一需要用户去网页端处理的。
func TestQRCaptchaIsDistinctError(t *testing.T) {
	_, err := StatusFromQR(8821)
	if err == nil {
		t.Fatal("8821 必须返回 error")
	}
	if got := errs.StatusOf(err); got != errs.Captcha {
		t.Errorf("8821 应映射为 Captcha, 得到 %v", got)
	}
}

func TestBuildQREscapesValues(t *testing.T) {
	// unikey 来自网络响应，未转义就有注入额外查询参数的机会
	got := buildQRURL("uk 1&x=2", "cid/3")
	u, err := url.Parse(got)
	if err != nil {
		t.Fatalf("生成的 URL 无法解析: %v (%q)", err, got)
	}
	q := u.Query()
	if q.Get("codekey") != "uk 1&x=2" {
		t.Errorf("codekey 往返后 = %q, 期望 %q", q.Get("codekey"), "uk 1&x=2")
	}
	if q.Get("chainId") != "cid/3" {
		t.Errorf("chainId 往返后 = %q, 期望 %q", q.Get("chainId"), "cid/3")
	}
	// 注入的 x=2 必须仍在 codekey 内部，不能变成独立参数
	if _, leaked := q["x"]; leaked {
		t.Error("注入的参数泄漏成独立查询参数")
	}
}

func TestBuildQRURLShape(t *testing.T) {
	got := buildQRURL("uk", "cid")
	if !strings.HasPrefix(got, "http://music.163.com/login?") {
		t.Errorf("URL 前缀不对: %q", got)
	}
	if !strings.Contains(got, "codekey=uk") || !strings.Contains(got, "chainId=cid") {
		t.Errorf("URL 缺少必要参数: %q", got)
	}
}

func TestStatusOf(t *testing.T) {
	cases := []struct {
		err  error
		want errs.Status
	}{
		{nil, errs.Ok},
		{errs.ErrInvalidArg, errs.InvalidArg},
		{errs.ErrNotLogin, errs.Unauthorized},
		// 缺 MUSIC_U 必须与未登录同义：用户粘了个不含登录态的 cookie，
		// 报"内部错误"会让人以为程序坏了
		{errs.ErrMissingMusicU, errs.Unauthorized},
		{errs.ErrCaptcha, errs.Captcha},
		{errs.ErrGeoBlock, errs.GeoBlock},
		{errs.ErrNoResource, errs.NoResource},
		{errs.ErrTrialOnly, errs.TrialOnly},
		{errs.ErrNotFound, errs.NotFound},
		{errs.ErrNotImplemented, errs.NotImplemented},
		{errs.ErrUpstream, errs.Upstream},
		{errs.ErrBadResponse, errs.Upstream},
		{errs.ErrUnknownCode, errs.Upstream},
	}
	for _, c := range cases {
		if got := errs.StatusOf(c.err); got != c.want {
			t.Errorf("StatusOf(%v) = %v, 期望 %v", c.err, got, c.want)
		}
	}
}

func TestStatusOfWrapped(t *testing.T) {
	// 生产代码全程用 fmt.Errorf("%w") 包装；若 errors.Is 链断了，
	// 所有真实错误都会退化成 Internal，UI 提示全部变成"内部错误"。
	wrapped := errCaptchaWrapped()
	if got := errs.StatusOf(wrapped); got != errs.Captcha {
		t.Errorf("包装后的 ErrCaptcha 应映射为 Captcha, 得到 %v", got)
	}

	unknown := errors.New("网络超时")
	if got := errs.StatusOf(unknown); got != errs.Internal {
		t.Errorf("未知错误应归 Internal, 得到 %v", got)
	}
	if got := errs.StatusOf(nil); got != errs.Ok {
		t.Errorf("nil 应映射为 Ok, 得到 %v", got)
	}
}

func errCaptchaWrapped() error {
	return wrappedErr{}
}

// wrappedErr 模拟 fmt.Errorf("%w") 的效果：实现 Unwrap 让 errors.Is 可用。
type wrappedErr struct{}

func (wrappedErr) Error() string { return "触发风控: " + errs.ErrCaptcha.Error() }
func (wrappedErr) Unwrap() error { return errs.ErrCaptcha }

func TestTruncate(t *testing.T) {
	if got := truncate([]byte("  abc  ")); got != "abc" {
		t.Errorf("truncate 应去掉首尾空白, 得到 %q", got)
	}
	long := strings.Repeat("x", 600)
	got := truncate([]byte(long))
	if len(got) > 600 || !strings.HasSuffix(got, "...(truncated)") {
		t.Errorf("超长响应应被截断并加后缀, 长度 %d", len(got))
	}
}

func TestAccountJSONShape(t *testing.T) {
	// 出参 JSON 的字段名是对外契约：UI 直接解析，改名即破坏兼容。
	// 这里锁住关键字段，防止重构时不小心改掉 json tag。
	acc := Account{Profile: Profile{UserID: 42, Nickname: "n"}, Account: AccountInfo{UserID: 42}}
	s := mustJSON(t, acc)
	for _, want := range []string{
		`"anonymous":false`, `"profile":{`, `"account":{`,
		`"userId":42`, `"nickname":"n"`, `"vipType":0`,
	} {
		if !strings.Contains(s, want) {
			t.Errorf("Account JSON 缺少 %s: %s", want, s)
		}
	}
}

func TestQrResultJSONShape(t *testing.T) {
	// ncm_qr_poll 的出参由 UI 按 "status" 分支处理，字段名不能变
	s := mustJSON(t, QrPollResult{Status: QrWaitScan, Message: "等待扫码"})
	if !strings.Contains(s, `"status":801`) || !strings.Contains(s, `"message":"等待扫码"`) {
		t.Errorf("QrPollResult JSON 不符契约: %s", s)
	}

	b := mustJSON(t, QrBeginResult{UniKey: "uk", QrcodeURL: "http://x/y"})
	if !strings.Contains(b, `"unikey":"uk"`) || !strings.Contains(b, `"qrcode_url":"http://x/y"`) {
		t.Errorf("QrBeginResult JSON 不符契约: %s", b)
	}
}