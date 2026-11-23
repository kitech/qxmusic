package catalog

import (
	"testing"

	"qx163/ncm/internal/errs"
)

// TestOpStatus520 锁定 520 的三类来源归属，这是离线判断准确性的基础：
//   传输层错误 / "Request Error:" 重发失败 → Network（可进离线）
//   "Request Blocked:" UNM 拦截         → Blocked（网络可达，不进离线）
// 任何一类归错都会让"断网判定"失真，必须防回归。
func TestOpStatus520(t *testing.T) {
	cases := []struct {
		name string
		body string
		want errs.Status
	}{
		{"transport-timeout", "dial tcp: i/o timeout", errs.Network},
		{"transport-dns", "no such host", errs.Network},
		{"unm-retry-fail", "Request Error:https://music.163.com/weapi/x", errs.Network},
		{"unm-blocked", "Request Blocked:https://music.163.com/weapi/x", errs.Blocked},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			err := opStatus("test", "/weapi/x", 520, []byte(c.body))
			ce, ok := err.(*errs.CodedError)
			if !ok {
				t.Fatalf("期望 *errs.CodedError, 得到 %T", err)
			}
			if ce.Status != c.want {
				t.Errorf("Status = %v, 期望 %v", ce.Status, c.want)
			}
			if ce.Code != 520 {
				t.Errorf("Code = %v, 期望 520", ce.Code)
			}
			// errmsg 必须是上游原文，不翻译/不提取/不 trim
			if ce.ErrMsg != c.body {
				t.Errorf("ErrMsg = %q, 期望原文 %q", ce.ErrMsg, c.body)
			}
			if got := errs.StatusOf(err); got != c.want {
				t.Errorf("StatusOf(err) = %v, 期望 %v", got, c.want)
			}
		})
	}
}
