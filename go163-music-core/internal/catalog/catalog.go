// Package catalog: 免认证目录与取流。JSON 契约见 doc.go。
package catalog

import (
	"encoding/json"
	"fmt"
	"log"
	"sort"
	"strconv"
	"strings"

	nmsvc "github.com/go-musicfox/netease-music/service"

	"qx163/ncm/internal/auth"
	"qx163/ncm/internal/errs"
)

/* ---------------- 出参结构（与 include/ncm.h 注释一一对应） ---------------- */

type CategoryGroup struct {
	Name       string   `json:"name"`
	Categories []string `json:"categories"`
}

type CategoriesResult struct {
	Hot    []string        `json:"hot"`
	Groups []CategoryGroup `json:"groups"`
}

type PlaylistSummary struct {
	ID         int64  `json:"id"`
	Name       string `json:"name"`
	Cover      string `json:"cover"`
	Creator    string `json:"creator"`
	TrackCount int64  `json:"track_count"`
	PlayCount  int64  `json:"play_count"`
}

type TopPlaylistsResult struct {
	Cat       string            `json:"cat"`
	Order     string            `json:"order"`
	Total     int64             `json:"total"`
	More      bool              `json:"more"`
	Playlists []PlaylistSummary `json:"playlists"`
}

type Song struct {
	ID         int64    `json:"id"`
	Name       string   `json:"name"`
	Artists    []string `json:"artists"`
	ArtistIDs  []int64  `json:"artist_ids"`
	Album      string   `json:"album"`
	AlbumID    int64    `json:"album_id"`
	DurationMS int64    `json:"duration_ms"`
	Cover      string   `json:"cover"`
	Playable   bool     `json:"playable"`
}

type SongDetailResult struct {
	Songs []Song `json:"songs"`
}

type PlaylistInfo struct {
	ID          int64  `json:"id"`
	Name        string `json:"name"`
	Cover       string `json:"cover"`
	Creator     string `json:"creator"`
	TrackCount  int64  `json:"track_count"`
	Description string `json:"description"`
}

type PlaylistTracksResult struct {
	Playlist PlaylistInfo `json:"playlist"`
	Tracks   []Song       `json:"tracks"`
}

type SourceResult struct {
	SongID    int64  `json:"song_id"`
	URL       string `json:"url"`
	Source    string `json:"source"`
	Quality   int64  `json:"quality"`
	Format    string `json:"format"`
	Size      int64  `json:"size"`
	ExpiresMS int64  `json:"expires_ms"`
	Level     string `json:"level,omitempty"`
}

type CheckMusicResult struct {
	SongID   int64  `json:"song_id"`
	Playable bool   `json:"playable"`
	Code     string `json:"reason"`
}

// LyricResult 歌词结果。形状对齐 qlmusic C++ 侧 onNcmLyric 的解析
// （cJSON root→"lrc"→"lyric"），仅保留原文 LRC；tlyric/yrc 等暂不透传。
type LyricResult struct {
	Lrc struct {
		Lyric string `json:"lyric"`
	} `json:"lrc"`
}

/* ---------------- 网易云原始响应的解析视图 ---------------- */

type rawSong struct {
	ID   float64 `json:"id"`
	Name string  `json:"name"`
	Ar   []struct {
		ID   float64 `json:"id"`
		Name string  `json:"name"`
	} `json:"ar"`
	Al struct {
		ID     float64 `json:"id"`
		Name   string  `json:"name"`
		PicURL string  `json:"picUrl"`
	} `json:"al"`
	DT        float64 `json:"dt"`
	St        float64 `json:"st"`
	Privilege *struct {
		St float64 `json:"st"`
	} `json:"privilege"`
}

// convSong 归一化歌曲对象。privilege.st < 0 表示不可播（灰歌），
// 顶层 st 仅在 privilege 缺失时作为后备。
func convSong(r rawSong) Song {
	st := r.St
	if r.Privilege != nil {
		st = r.Privilege.St
	}
	s := Song{
		ID:         int64(r.ID),
		Name:       r.Name,
		Album:      r.Al.Name,
		AlbumID:    int64(r.Al.ID),
		DurationMS: int64(r.DT),
		Cover:      r.Al.PicURL,
		Playable:   st >= 0,
	}
	for _, a := range r.Ar {
		if a.Name != "" {
			s.Artists = append(s.Artists, a.Name)
		}
		if a.ID > 0 {
			s.ArtistIDs = append(s.ArtistIDs, int64(a.ID))
		}
	}
	return s
}

type rawPlaylist struct {
	ID          float64 `json:"id"`
	Name        string  `json:"name"`
	CoverImgURL string  `json:"coverImgUrl"`
	Description string  `json:"description"`
	TrackCount  float64 `json:"trackCount"`
	Creator     struct {
		Nickname string `json:"nickname"`
	} `json:"creator"`
}

func (r rawPlaylist) info() PlaylistInfo {
	return PlaylistInfo{
		ID:          int64(r.ID),
		Name:        r.Name,
		Cover:       r.CoverImgURL,
		Creator:     r.Creator.Nickname,
		TrackCount:  int64(r.TrackCount),
		Description: r.Description,
	}
}

// playlistEnvelope 是 detail 接口通用外壳；playlist_tracks 复用
// （AllTracks 只是把 songs 数组填进 playlist.tracks）。
type playlistEnvelope struct {
	Code     float64     `json:"code"`
	Playlist rawPlaylist `json:"playlist"`
}

type rawURLData struct {
	Code          float64          `json:"code"`
	URL           string           `json:"url"`
	Br            float64          `json:"br"`
	Size          float64          `json:"size"`
	Type          string           `json:"type"`
	Expi          float64          `json:"expi"`
	Level         string           `json:"level"`
	FreeTrialInfo *json.RawMessage `json:"freeTrialInfo"`
}

// detailEnvelope 解析 detail 响应并转义失败为 errs.ErrBadResponse。
func playlistDetailEnvelope(body []byte) (rawPlaylist, error) {
	var env playlistEnvelope
	if err := json.Unmarshal(body, &env); err != nil {
		return rawPlaylist{}, fmt.Errorf("%w: playlist/detail 解析失败: %v", errs.ErrBadResponse, err)
	}
	if env.Playlist.ID <= 0 {
		return rawPlaylist{}, fmt.Errorf("%w: playlist/detail 缺少 playlist 对象", errs.ErrBadResponse)
	}
	return env.Playlist, nil
}

// urlData 解析 player/url 类响应的 data 数组。
func urlData(body []byte) ([]rawURLData, error) {
	var env struct {
		Data []rawURLData `json:"data"`
	}
	if err := json.Unmarshal(body, &env); err != nil || len(env.Data) == 0 {
		return nil, fmt.Errorf("%w: player/url 响应缺少 data", errs.ErrBadResponse)
	}
	return env.Data, nil
}

/* ---------------- 免认证目录接口 ---------------- */

// Categories 合并 hottags + 完整分类目录。
// catalogue 失败不拖垮整体（UI 还能用 hot 标签）。
func Categories() (CategoriesResult, error) {
	res := CategoriesResult{}

	code, body := (&nmsvc.PlaylistHotService{}).PlaylistHot()
	if err := opStatus("playlist_categories", "/weapi/playlist/hottags", code, len(body)); err != nil {
		return res, err
	}
	var hot struct {
		Tags []struct {
			Name string `json:"name"`
		} `json:"tags"`
	}
	if err := json.Unmarshal(body, &hot); err != nil {
		return res, fmt.Errorf("%w: hottags 解析失败: %v", errs.ErrBadResponse, err)
	}
	for _, t := range hot.Tags {
		if t.Name != "" {
			res.Hot = append(res.Hot, t.Name)
		}
	}

	code, body = (&nmsvc.PlaylistCatlistService{}).PlaylistCatlist()
	if st := opStatus("playlist_categories", "/weapi/playlist/catalogue", code, len(body)); st != nil {
		return res, nil // 降级：仅返回 hot
	}
	var catlist struct {
		Categories map[string]string `json:"categories"`
		Sub        []struct {
			Name     string  `json:"name"`
			Category float64 `json:"category"`
		} `json:"sub"`
	}
	if err := json.Unmarshal(body, &catlist); err != nil {
		return res, nil
	}
	byIdx := map[int][]string{}
	for _, s := range catlist.Sub {
		byIdx[int(s.Category)] = append(byIdx[int(s.Category)], s.Name)
	}
	// categories 的 key 是分类索引；按索引排序输出保证顺序稳定
	idxs := make([]int, 0, len(catlist.Categories))
	for k := range catlist.Categories {
		if v, e := strconv.Atoi(k); e == nil {
			idxs = append(idxs, v)
		}
	}
	sort.Ints(idxs)
	for _, i := range idxs {
		name := catlist.Categories[strconv.Itoa(i)]
		if name == "" {
			continue
		}
		res.Groups = append(res.Groups, CategoryGroup{Name: name, Categories: byIdx[i]})
	}
	return res, nil
}

// TopPlaylists 按分类/Page 拉取歌单列表。order 仅 hot/new。
func TopPlaylists(cat, order string, limit, offset int) (TopPlaylistsResult, error) {
	if cat == "" {
		cat = "全部"
	}
	if order != "hot" {
		order = "new"
	}
	if limit <= 0 {
		limit = 50
	}
	if offset < 0 {
		offset = 0
	}

	svc := &nmsvc.TopPlaylistService{
		Cat:    cat,
		Order:  order,
		Limit:  strconv.Itoa(limit),
		Offset: strconv.Itoa(offset),
	}
	code, body := svc.TopPlaylist()
	if err := opStatus("top_playlists", "/weapi/playlist/list", code, len(body)); err != nil {
		return TopPlaylistsResult{}, err
	}

	var env struct {
		Code      float64 `json:"code"`
		Total     float64 `json:"total"`
		More      bool    `json:"more"`
		Cat       string  `json:"cat"`
		Playlists []struct {
			rawPlaylist
			PlayCount float64 `json:"playCount"`
		} `json:"playlists"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return TopPlaylistsResult{}, fmt.Errorf("%w: playlist/list 解析失败: %v", errs.ErrBadResponse, err)
	}

	res := TopPlaylistsResult{
		Cat:   env.Cat,
		Order: order,
		Total: int64(env.Total),
		More:  env.More,
	}
	for _, p := range env.Playlists {
		res.Playlists = append(res.Playlists, PlaylistSummary{
			ID:         int64(p.ID),
			Name:       p.Name,
			Cover:      p.CoverImgURL,
			Creator:    p.Creator.Nickname,
			TrackCount: int64(p.TrackCount),
			PlayCount:  int64(p.PlayCount),
		})
	}
	return res, nil
}

// PlaylistDetail 歌单元信息（不含曲目）。
func PlaylistDetail(id int64) (PlaylistInfo, error) {
	if id <= 0 {
		return PlaylistInfo{}, errs.ErrInvalidArg
	}
	svc := &nmsvc.PlaylistDetailService{Id: strconv.FormatInt(id, 10)}
	code, body := svc.PlaylistDetail()
	if err := opStatus("playlist_detail", "/weapi/v3/playlist/detail", code, len(body)); err != nil {
		return PlaylistInfo{}, err
	}
	pl, err := playlistDetailEnvelope(body)
	if err != nil {
		return PlaylistInfo{}, err
	}
	return pl.info(), nil
}

// PlaylistTracks 歌单曲目（含歌单元信息）。
// 未登录访问私密歌单会 301 -> ErrNotLogin。
func PlaylistTracks(id int64) (PlaylistTracksResult, error) {
	if id <= 0 {
		return PlaylistTracksResult{}, errs.ErrInvalidArg
	}
	svc := &nmsvc.PlaylistTrackAllService{Id: strconv.FormatInt(id, 10)}
	code, body := svc.AllTracks()
	if err := opStatus("playlist_tracks", "/weapi/v3/playlist/detail", code, len(body)); err != nil {
		return PlaylistTracksResult{}, err
	}
	// AllTracks 在 detail 的 playlist.tracks 里填好了全量曲目
	var env struct {
		Playlist struct {
			rawPlaylist
			Tracks []rawSong `json:"tracks"`
		} `json:"playlist"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return PlaylistTracksResult{}, fmt.Errorf("%w: playlist/detail 解析失败: %v", errs.ErrBadResponse, err)
	}
	res := PlaylistTracksResult{Playlist: env.Playlist.info()}
	res.Tracks = make([]Song, 0, len(env.Playlist.Tracks))
	for _, raw := range env.Playlist.Tracks {
		res.Tracks = append(res.Tracks, convSong(raw))
	}
	return res, nil
}

// SongDetail 批量歌曲详情，ids 形如 "123,456"。
func SongDetail(ids string) (SongDetailResult, error) {
	if strings.TrimSpace(ids) == "" {
		return SongDetailResult{}, errs.ErrInvalidArg
	}
	code, body := (&nmsvc.SongDetailService{Ids: ids}).SongDetail()
	if err := opStatus("song_detail", "/weapi/v3/song/detail", code, len(body)); err != nil {
		return SongDetailResult{}, err
	}
	var env struct {
		Songs []rawSong `json:"songs"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return SongDetailResult{}, fmt.Errorf("%w: song/detail 解析失败: %v", errs.ErrBadResponse, err)
	}
	res := SongDetailResult{Songs: make([]Song, 0, len(env.Songs))}
	for _, raw := range env.Songs {
		res.Songs = append(res.Songs, convSong(raw))
	}
	return res, nil
}

/* ---------------- 艺人详情（头像） ---------------- */

// ArtistInfo 艺人基础信息（含头像 URL）。
type ArtistInfo struct {
	ID     int64  `json:"id"`
	Name   string `json:"name"`
	PicURL string `json:"pic_url"`
}

// LikeListResult 账号"喜欢的音乐"全量 id（顺序=服务端收藏顺序）。
type LikeListResult struct {
	IDs []int64 `json:"ids"`
}

// LikeResult 收藏/取消收藏回执。
type LikeResult struct {
	SongID int64 `json:"song_id"`
	Liked  bool  `json:"liked"`
}

// LikeList 拉取当前登录账号喜欢的歌曲 id（weapi/song/like/get，需登录）。
// uid 取自 auth.Account()——与 ncm_account 同一会话，未登录返回 ErrNotLogin。
func LikeList() (LikeListResult, error) {
	acc, err := (&auth.Service{}).Account()
	if err != nil {
		return LikeListResult{}, err
	}
	uid := acc.Profile.UserID
	if uid == 0 {
		uid = acc.Account.UserID
	}
	if uid == 0 {
		return LikeListResult{}, errs.ErrNotLogin
	}
	svc := &nmsvc.LikeListService{UID: strconv.FormatInt(uid, 10)}
	code, body := svc.LikeList()
	if err := opStatus("like_list", "/weapi/song/like/get", code, len(body)); err != nil {
		return LikeListResult{}, err
	}
	var env struct {
		IDs []float64 `json:"ids"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return LikeListResult{}, fmt.Errorf("%w: like_list 解析失败: %v", errs.ErrBadResponse, err)
	}
	res := LikeListResult{IDs: make([]int64, 0, len(env.IDs))}
	for _, v := range env.IDs {
		if v > 0 {
			res.IDs = append(res.IDs, int64(v))
		}
	}
	return res, nil
}

// Like 收藏（on=true）或取消收藏一首歌（api/radio/like，需登录）。
func Like(songID int64, on bool) (LikeResult, error) {
	if songID <= 0 {
		return LikeResult{}, errs.ErrInvalidArg
	}
	l := "false"
	if on {
		l = "true"
	}
	svc := &nmsvc.LikeService{ID: strconv.FormatInt(songID, 10), L: l}
	code, body := svc.Like()
	if err := opStatus("like", "/api/radio/like", code, len(body)); err != nil {
		return LikeResult{}, err
	}
	var env struct {
		Code   float64 `json:"code"`
		SongID float64 `json:"songId"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return LikeResult{}, fmt.Errorf("%w: like 解析失败: %v", errs.ErrBadResponse, err)
	}
	return LikeResult{SongID: songID, Liked: on}, nil
}

// ArtistDetail 取艺人详情（头像）。响应形状：
// {"artist":{"id","name","picUrl","img1v1Url",...}}
func ArtistDetail(id int64) (ArtistInfo, error) {
	if id <= 0 {
		return ArtistInfo{}, errs.ErrInvalidArg
	}
	idStr := strconv.FormatInt(id, 10)
	svc := &nmsvc.ArtistsService{ID: idStr}
	code, body := svc.Artists()
	if err := opStatus("artist_detail", "/weapi/v1/artist/"+idStr, code, len(body)); err != nil {
		return ArtistInfo{}, err
	}
	var env struct {
		Artist struct {
			ID        float64 `json:"id"`
			Name      string  `json:"name"`
			PicURL    string  `json:"picUrl"`
			Img1v1URL string  `json:"img1v1Url"`
		} `json:"artist"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return ArtistInfo{}, fmt.Errorf("%w: artist 解析失败: %v", errs.ErrBadResponse, err)
	}
	pic := env.Artist.PicURL
	if pic == "" {
		pic = env.Artist.Img1v1URL
	}
	return ArtistInfo{
		ID:     int64(env.Artist.ID),
		Name:   env.Artist.Name,
		PicURL: pic,
	}, nil
}

// Search 关键词搜索（type=1 单曲，匿名可用）。
// 响应形状与 cloudsearch 一致：{"result":{"songs":[...]}}。
func Search(keywords string, limit int) (SongDetailResult, error) {
	kw := strings.TrimSpace(keywords)
	if kw == "" {
		return SongDetailResult{}, errs.ErrInvalidArg
	}
	if limit <= 0 {
		limit = 30
	}
	svc := &nmsvc.SearchService{
		S:     kw,
		Type:  "1",
		Limit: strconv.Itoa(limit),
	}
	code, body := svc.Search()
	if err := opStatus("search", "/api/cloudsearch/pc", code, len(body)); err != nil {
		return SongDetailResult{}, err
	}
	var env struct {
		Result struct {
			Songs []rawSong `json:"songs"`
		} `json:"result"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return SongDetailResult{}, fmt.Errorf("%w: cloudsearch 解析失败: %v", errs.ErrBadResponse, err)
	}
	res := SongDetailResult{Songs: make([]Song, 0, len(env.Result.Songs))}
	for _, raw := range env.Result.Songs {
		res.Songs = append(res.Songs, convSong(raw))
	}
	return res, nil
}

// CheckMusic 探测歌曲可用性（player/url 探测）。
func CheckMusic(songID int64, br int32) (CheckMusicResult, error) {
	if songID <= 0 {
		return CheckMusicResult{}, errs.ErrInvalidArg
	}
	if br <= 0 {
		br = 999000
	}
	svc := &nmsvc.CheckMusicService{
		ID: strconv.FormatInt(songID, 10),
		Br: strconv.Itoa(int(br)),
	}
	code, body := svc.CheckMusic()
	if err := opStatus("check_music", "/api/song/enhance/player/url", code, len(body)); err != nil {
		return CheckMusicResult{}, err
	}
	data, err := urlData(body)
	if err != nil {
		return CheckMusicResult{}, err
	}
	res := CheckMusicResult{SongID: songID}
	if len(data) > 0 {
		res.Code = strconv.FormatFloat(data[0].Code, 'f', -1, 64)
		res.Playable = data[0].Code == 200 && data[0].URL != ""
	}
	return res, nil
}

// Lyric 拉取歌曲歌词（LRC 原文，含 [mm:ss.xx] 时间轴；空串 = 无词，UI 落空态）。
// 网搜核对（pkg.go.dev netease-music v1.6 + go-musicfox FetchLyric 参考）：
// LyricService{ID}.Lyric() 返回 (code, body)，body 为 NCM 原始 JSON，
// 歌词原文在 lrc.lyric。
func Lyric(songID int64) (LyricResult, error) {
	if songID <= 0 {
		return LyricResult{}, errs.ErrInvalidArg
	}
	code, body := (&nmsvc.LyricService{ID: strconv.FormatInt(songID, 10)}).Lyric()
	if err := opStatus("song_lyric", "/api/song/lyric", code, len(body)); err != nil {
		return LyricResult{}, err
	}
	var env struct {
		Lrc struct {
			Lyric string `json:"lyric"`
		} `json:"lrc"`
	}
	if err := json.Unmarshal(body, &env); err != nil {
		return LyricResult{}, fmt.Errorf("%w: song/lyric 解析失败: %v", errs.ErrBadResponse, err)
	}
	res := LyricResult{}
	res.Lrc.Lyric = env.Lrc.Lyric
	return res, nil
}

/* ---------------- 取流 ---------------- */

// qualityLevel 把 ncm_quality 枚举映射为 v1 接口的 level 串。
// 调用前 FFI 已用 validQuality 拦截非法值，这里只需兜底。
func qualityLevel(q int32) nmsvc.SongQualityLevel {
	switch q {
	case 128000:
		return nmsvc.Standard
	case 192000:
		return nmsvc.Higher
	case 320000:
		return nmsvc.Exhigh
	case 999000:
		return nmsvc.Lossless
	case 1000000:
		return nmsvc.Hires
	case 100001:
		return nmsvc.JYEffect
	case 100002:
		return nmsvc.Sky
	case 100003:
		return nmsvc.JYMaster
	}
	return nmsvc.Exhigh
}

// isTrial 判定 freeTrialInfo（nil 或字面量 null 都算非试听）。
func isTrial(d rawURLData) bool {
	return d.FreeTrialInfo != nil && string(*d.FreeTrialInfo) != "null"
}

// finishSource 把单条 data 收尾成结果或错误：
//
//	url 非空 + 非试听 -> SourceResult
//	url 非空 + 试听   -> ErrTrialOnly（UI 不会再拿到 URL）
//	url 为空          -> 灰歌/无版权 -> ErrNoResource
func finishSource(songID int64, quality int32, d rawURLData) (SourceResult, error) {
	out := SourceResult{
		SongID:  songID,
		Source:  "remote",
		Level:   d.Level,
		Quality: int64(d.Br),
	}
	if out.Quality <= 0 {
		out.Quality = int64(quality)
	}
	if d.URL == "" {
		return out, errs.ErrNoResource
	}
	out.URL = d.URL
	out.Format = d.Type
	out.Size = int64(d.Size)
	if d.Expi > 0 {
		out.ExpiresMS = int64(d.Expi) * 1000
	}
	if isTrial(d) {
		return out, errs.ErrTrialOnly
	}
	// 点播成功的歌曲播放地址落日志（便于运维核对 URL 有效期/防密令牌）
	log.Printf("[ncm][song] song_id=%d url=%s source=%s quality=%d level=%s",
		out.SongID, out.URL, out.Source, out.Quality, out.Level)
	return out, nil
}

// ResolveSource 取播放地址。优先 v1 接口（level 精确），v1 不可用
// （传输层失败/响应异常）才回落旧接口；v1 已给出明确业务结果时不回落。
func ResolveSource(songID int64, quality int32) (SourceResult, error) {
	if songID <= 0 {
		return SourceResult{}, errs.ErrInvalidArg
	}

	res, err := resolveV1(songID, quality)
	if err == nil {
		return res, nil
	}
	// 非"网络不可达"的业务结果（灰歌/试听/未登录/上游异常）直接透传
	if errs.StatusOf(err) != errs.Network {
		return SourceResult{}, err
	}
	return resolveLegacy(songID, quality)
}

func resolveV1(songID int64, quality int32) (SourceResult, error) {
	svc := &nmsvc.SongUrlV1Service{
		ID:         strconv.FormatInt(songID, 10),
		Level:      qualityLevel(quality),
		EncodeType: "flac",
	}
	code, body, err := svc.SongUrl()
	if err != nil {
		// CallWeapi 传输层错误；罩上 ErrNetwork 便于回落判断
		return SourceResult{}, errs.ErrNetwork
	}
	if st := opStatus("resolve_source", "/weapi/song/enhance/player/url/v1", code, len(body)); st != nil {
		return SourceResult{}, st
	}
	data, perr := urlData(body)
	if perr != nil {
		return SourceResult{}, perr
	}
	if len(data) == 0 {
		return SourceResult{}, errs.ErrNoResource
	}
	return finishSource(songID, quality, data[0])
}

func resolveLegacy(songID int64, quality int32) (SourceResult, error) {
	svc := &nmsvc.SongUrlService{
		ID: strconv.FormatInt(songID, 10),
		Br: strconv.Itoa(int(quality)),
	}
	code, body := svc.SongUrl()
	if err := opStatus("resolve_source", "/api/song/enhance/player/url", code, len(body)); err != nil {
		return SourceResult{}, err
	}
	data, perr := urlData(body)
	if perr != nil {
		return SourceResult{}, perr
	}
	if len(data) == 0 {
		return SourceResult{}, errs.ErrNoResource
	}
	return finishSource(songID, quality, data[0])
}

