/*
Package catalog 实现免认证目录/取流的业务层：分类、歌单、曲目、搜索源、
可用性校验。全部返回 error（由 errs.StatusOf 翻译为 C ABI 状态码），
由 FFI 层在持有 apiMu 的前提下串行调用。

端点映射（go-musicfox/netease-music v1.6.0，service 层返回的 float64 是
响应体中的 code，不是 HTTP 状态；传输层失败固定为 520）：

	PlaylistCatlistService   POST /weapi/playlist/catalogue   (weapi)
	PlaylistHotService       POST /weapi/playlist/hottags     (weapi)
	TopPlaylistService       POST /weapi/playlist/list        (weapi)
	PlaylistDetailService    POST /weapi/v3/playlist/detail   (linuxapi)
	PlaylistTrackAllService  于 detail 基础上拼 song/detail（≤500/页并发）
	SongDetailService        POST /weapi/v3/song/detail       (weapi, os=pc)
	SongUrlV1Service         POST /weapi/song/enhance/player/url/v1
	SongUrlService           POST /api/song/enhance/player/url (linuxapi, os=pc)
	CheckMusicService        POST /api/song/enhance/player/url (weapi)

认证矩阵（记录于 include/ncm.h，二者必须同步）：

	匿名可用      categories / top_playlists / playlist_detail / playlist_tracks(公开)
	              album_tracks / search(type=1) / song_detail / lyric / check_music
	              resolve_source(免费曲)
	需登录(301)   daily_recommend / user_playlists / 私密歌单曲目 / VIP 专属音质
	无资源(灰歌)  resolve_source url 为空 -> NCM_ERR_NO_RESOURCE
	仅试听        freeTrialInfo 非空 -> NCM_ERR_TRIAL_ONLY

注意：util.CreateRequest 在 code != 200 时会把整个 Options（含 cookie）
打进默认 log，ncmffi 在首次 ncm_new 安装脱敏 writer 拦截 MUSIC_U 等值。
*/
package catalog
