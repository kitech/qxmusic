// Package errs 定义跨 FFI 边界的业务状态码，与 include/ncm.h 中的
// ncm_status 逐值对应。任何一侧改动都必须同步。
package errs

// Status 是对外暴露的错误分类。数值必须与 ncm.h 保持一致。
type Status int32

const (
	Ok             Status = 0
	InvalidArg     Status = -1
	Network        Status = -2
	Unauthorized   Status = -3 // 301 未登录
	Captcha        Status = -4 // 8821 触发风控，需要行为验证码
	GeoBlock       Status = -5 // 460 / 版权地区限制
	NoResource     Status = -6 // 无可播资源（灰歌）
	TrialOnly      Status = -7 // 仅试听片段，非完整歌曲
	NotFound       Status = -8
	NotImplemented Status = -9
	Upstream       Status = -10
	Internal       Status = -100
)

func (s Status) String() string {
	switch s {
	case Ok:
		return "Ok"
	case InvalidArg:
		return "InvalidArg"
	case Network:
		return "Network"
	case Unauthorized:
		return "Unauthorized"
	case Captcha:
		return "Captcha"
	case GeoBlock:
		return "GeoBlock"
	case NoResource:
		return "NoResource"
	case TrialOnly:
		return "TrialOnly"
	case NotFound:
		return "NotFound"
	case NotImplemented:
		return "NotImplemented"
	case Upstream:
		return "Upstream"
	case Internal:
		return "Internal"
	}
	return "Unknown"
}

// FromNetease 把网易云返回的业务 code 映射为 Status。
//
// 8821 需要行为验证码这一语义极易在转发过程中丢失：netease-music 的
// LoginQRService.CheckQR 只是把 8821 原样返回，若调用方只判断
// code != 200 就一律当失败，UI 将无法区分"风控"与"网络错误"。
func FromNetease(code float64) Status {
	switch {
	case code == 8821:
		return Captcha
	case code == 460:
		return GeoBlock
	case code == 301 || code == 302:
		return Unauthorized
	case code == 404:
		return NotFound
	case code == 200:
		return Ok
	}
	return Network
}