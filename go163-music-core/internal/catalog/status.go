package catalog

import (
	"fmt"
	"log"

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
func opStatus(op, endpoint string, code float64, bodyLen int) error {
	switch {
	case code == 520:
		// util.CreateRequest 的传输层失败固定返回 520（body 为错误文本）
		return errs.ErrNetwork
	case code == 301 || code == 302:
		log.Printf("[ncm][auth-required] op=%s endpoint=%s code=%v logged_in=%v",
			op, endpoint, code, auth.LoggedIn())
		return errs.ErrNotLogin
	case code == 200:
		log.Printf("[ncm][ok] op=%s url=%s%s code=200 bodyLen=%d",
			op, ncmHost, endpoint, bodyLen)
		return nil
	case code == 404:
		return errs.ErrNotFound
	case code == 460:
		return errs.ErrGeoBlock
	case code == 8821:
		return errs.ErrCaptcha
	}
	return fmt.Errorf("%w: %s code=%v", errs.ErrUpstream, endpoint, code)
}
