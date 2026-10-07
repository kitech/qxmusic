// Package version 记录库版本与 ABI 版本。
package version

const (
	// Version 是本库的语义化版本。
	Version = "0.1.0"

	// AbiVersion 是 C ABI 版本。include/ncm.h 中任何破坏性变更
	// （删改已有导出符号、修改已有函数签名或结构体布局）都必须递增，
	// 并在 ncm_version() 的输出中体现，方便 UI 侧做兼容判断。
	AbiVersion = 1
)

// String 形如 "0.1.0 (abi 1)"。
func String() string {
	return Version + " (abi " + itoa(AbiVersion) + ")"
}

func itoa(v int) string {
	if v == 0 {
		return "0"
	}
	var buf [20]byte
	i := len(buf)
	for v > 0 {
		i--
		buf[i] = byte('0' + v%10)
		v /= 10
	}
	return string(buf[i:])
}