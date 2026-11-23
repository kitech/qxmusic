package catalog

import (
	"log"
	"strings"

	"qx163/ncm/internal/auth"
	"qx163/ncm/internal/errs"
)

// ncmHost 是各 service 统一请求的主机名，用于成功日志的 url 展示。
const ncmHost = "https://music.163.com"

// opStatus 把 service 层返回的业务 code 统一转换成 error。
//
// 关键点：301/302（未登录）必须在这里打点 —— UI 通过
// [ncm][auth-required] 前缀定位"操作需要登录"并弹出三处提示；
// logged_in 附带本端是否有 MUSIC_U，便于区分"未登录"与"登录态失效"。
func opStatus(op, endpoint string, code float64, body []byte) error {
	switch {
	case code == 520:
		// util.CreateRequest 的 520 有三种来源：传输层错误 / UNM 拦截 / UNM 重发失败。
		// "Request Blocked:" 发生在前置 DryRun 请求已成功之后（网络可达），
		// 是 UNM 主动拦截，不能算离线；其余（含 "Request Error:" 重发失败）
		// 归 Network，偏安全侧：宁可误进离线，也不漏判断网。
		msg := string(body)
		if strings.HasPrefix(msg, "Request Blocked:") {
			return &errs.CodedError{Status: errs.Blocked, Code: 520, Op: op,
				ErrMsg: msg, Sentinel: errs.ErrBlocked}
		}
		return &errs.CodedError{Status: errs.Network, Code: 520, Op: op,
			ErrMsg: msg, Sentinel: errs.ErrNetwork}
	case code == 301 || code == 302:
		log.Printf("[ncm][auth-required] op=%s endpoint=%s code=%v logged_in=%v",
			op, endpoint, code, auth.LoggedIn())
		return &errs.CodedError{Status: errs.Unauthorized, Code: code, Op: op,
			ErrMsg: string(body), Sentinel: errs.ErrNotLogin}
	case code == 200:
		log.Printf("[ncm][ok] op=%s url=%s%s code=200 bodyLen=%d",
			op, ncmHost, endpoint, len(body))
		return nil
	case code == 404:
		return &errs.CodedError{Status: errs.NotFound, Code: 404, Op: op,
			ErrMsg: string(body), Sentinel: errs.ErrNotFound}
	case code == 460:
		return &errs.CodedError{Status: errs.GeoBlock, Code: 460, Op: op,
			ErrMsg: string(body), Sentinel: errs.ErrGeoBlock}
	case code == 8821:
		return &errs.CodedError{Status: errs.Captcha, Code: 8821, Op: op,
			ErrMsg: string(body), Sentinel: errs.ErrCaptcha}
	}
	return &errs.CodedError{Status: errs.Upstream, Code: code, Op: op,
		ErrMsg: string(body), Sentinel: errs.ErrUpstream}
}
