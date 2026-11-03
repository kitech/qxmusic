module qx163/ncm

go 1.22

require github.com/go-musicfox/netease-music v1.6.0

require (
	github.com/buger/jsonparser v1.1.1 // indirect
	github.com/cnsilvan/UnblockNeteaseMusic v0.0.0-20230310083816-92b59c95a366 // indirect
	github.com/forgoer/openssl v1.6.0 // indirect
	github.com/go-musicfox/requests v0.2.3 // indirect
	github.com/mitchellh/mapstructure v1.5.0 // indirect
	github.com/tidwall/gjson v1.17.1 // indirect
	github.com/tidwall/match v1.1.1 // indirect
	github.com/tidwall/pretty v1.2.1 // indirect
	golang.org/x/text v0.13.0 // indirect
)

// 依赖方 go.mod 里的 replace 在主模块中会被忽略，必须在此处声明，
// 否则会解析到 2023-03 的旧版 UNM（v0.1.5/v0.0.0-2023… 伪版本）。
replace github.com/cnsilvan/UnblockNeteaseMusic => github.com/go-musicfox/UnblockNeteaseMusic v0.1.6
