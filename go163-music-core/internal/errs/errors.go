package errs

import "errors"

// 哨兵错误。给上层 errors.Is/As 与日志用，与 Status 是两个维度：
// Status 面向 C ABI（数值契约），这些 error 面向 Go 内部（语义契约）。
var (
	ErrInvalidArg     = errors.New("参数不合法")
	ErrNotLogin       = errors.New("未登录")
	ErrCaptcha        = errors.New("触发风控，需要行为验证码")
	ErrMissingMusicU  = errors.New("cookie 中缺少 MUSIC_U")
	ErrUpstream       = errors.New("上游接口返回异常")
	ErrBadResponse    = errors.New("上游响应无法解析")
	ErrUnknownCode    = errors.New("无法识别的状态码")
	ErrGeoBlock       = errors.New("受版权地区限制")
	ErrNoResource     = errors.New("无可播资源")
	ErrTrialOnly      = errors.New("仅试听片段")
	ErrNotFound       = errors.New("资源不存在")
	ErrNotImplemented = errors.New("尚未实现")
	ErrInternal       = errors.New("内部错误")
)

// 没有 ErrQRExpired。二维码失效（状态码 800）是流程的正常终态，
// 经 QrPollResult.Status 传给 UI，不是错误 —— 定义一个用不上的哨兵
// 只会诱导人返回它，然后被 StatusOf 归成 Internal。

// StatusOf 把哨兵错误映射为对外状态码。
//
// 这是 error -> Status 的唯一入口：所有导出函数都经它转换，
// 避免同一个错误在不同接口上返回不同的状态码（UI 会因此出现不一致的提示）。
func StatusOf(err error) Status {
	switch {
	case err == nil:
		return Ok
	case errors.Is(err, ErrInvalidArg):
		return InvalidArg
	case errors.Is(err, ErrNotLogin), errors.Is(err, ErrMissingMusicU):
		// 缺 MUSIC_U 是"你给的 cookie 不构成登录态"，从 UI 角度与未登录同义，
		// 都该引导用户去登录，而不是报成内部错误。
		return Unauthorized
	case errors.Is(err, ErrCaptcha):
		return Captcha
	case errors.Is(err, ErrGeoBlock):
		return GeoBlock
	case errors.Is(err, ErrNoResource):
		return NoResource
	case errors.Is(err, ErrTrialOnly):
		return TrialOnly
	case errors.Is(err, ErrNotFound):
		return NotFound
	case errors.Is(err, ErrNotImplemented):
		return NotImplemented
	case errors.Is(err, ErrUpstream), errors.Is(err, ErrBadResponse),
		errors.Is(err, ErrUnknownCode):
		// 未知状态码也算上游异常：是我们没预料到的响应，不是本地 bug。
		return Upstream
	}
	// 剩下的只可能是 ErrInternal 或真正未知的 error，归为 Internal。
	return Internal
}