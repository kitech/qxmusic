module qx163/ncm

go 1.22

require github.com/go-musicfox/netease-music v1.6.0

require github.com/cnsilvan/UnblockNeteaseMusic v0.0.0-20230310083816-92b59c95a366 // indirect

// 依赖方 go.mod 里的 replace 在主模块中会被忽略，必须在此处声明，
// 否则会解析到 2023-03 的旧版 UNM（v0.1.5/v0.0.0-2023… 伪版本）。
replace github.com/cnsilvan/UnblockNeteaseMusic => github.com/go-musicfox/UnblockNeteaseMusic v0.1.6