// Package bili 提供 B 站音乐内容（视频音轨路线）。
//
// 路线核对（2026-11 实测）：
//   - 音频区（audio.bilibili.com）官方歌单已清空（menu snum=0、menu/song 返回 null），
//     仅存量 auid 可取流 → 不作为内容源；
//   - 视频音轨路线可用：音乐区排行（ranking/v2 rid=3，免签名）、WBI 搜索、
//     view 取 cid、playurl(fnval=4048) 取 dash.audio（64K/132K/192K/Hi-Res）。
//
// 本包只做查询与"取直链"：不下载、不落盘、不设代理。
// 音轨 CDN 必须带 Referer https://www.bilibili.com/（实测无 Referer 403），
// 因此 Stream 把 referer/user_agent 一并返回，由 UI 侧下载时带上。
package bili

import (
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	/* UA 实测口径：B站对 "X11; Linux x86_64" 的完整 Chrome UA 返回 -352 风控，
	 * Windows 常规 UA 正常（2026-11 实测）。CDN 下载同用此 UA。 */
	userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
		"(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
	referer = "https://www.bilibili.com/"
	apiBase = "https://api.bilibili.com"
)

var (
	mu    sync.Mutex
	sess  string // SESSDATA（可选；大会员才出 Hi-Res）
	httpc = &http.Client{Timeout: 15 * time.Second}
	emTag = regexp.MustCompile(`<[^>]+>`)
)

// SetCookie 设置 SESSDATA（空串清除）。仅影响能否取到 Hi-Res FLAC。
func SetCookie(s string) {
	mu.Lock()
	defer mu.Unlock()
	sess = strings.TrimSpace(s)
}

func cookie() string {
	mu.Lock()
	defer mu.Unlock()
	if sess == "" {
		return ""
	}
	return "SESSDATA=" + sess
}

// Song 与 internal/catalog.Song 同形（UI 复用解析），Bvid 为取流标记。
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
	PublishMs  int64    `json:"publish_ms"`
	Bvid       string   `json:"bvid"`
}

// ListResult 与 catalog.SongDetailResult 同形（{"songs":[...]}）。
type ListResult struct {
	Songs []Song `json:"songs"`
	Page  int    `json:"page"`
	Total int    `json:"total"`
}

/* ---- 与 internal/catalog 完全同形的结果类型（UI 零改动） ---- */

type PlaylistSummary struct {
	ID          int64  `json:"id"`
	Name        string `json:"name"`
	Cover       string `json:"cover"`
	Creator     string `json:"creator"`
	TrackCount  int64  `json:"track_count"`
	PlayCount   int64  `json:"play_count"`
	UpdatedAtMs int64  `json:"updated_at_ms"`
}

type TopResult struct {
	Cat       string            `json:"cat"`
	Order     string            `json:"order"`
	Total     int64             `json:"total"`
	More      bool              `json:"more"`
	Playlists []PlaylistSummary `json:"playlists"`
}

type PlaylistInfo struct {
	ID          int64  `json:"id"`
	Name        string `json:"name"`
	Cover       string `json:"cover"`
	Creator     string `json:"creator"`
	TrackCount  int64  `json:"track_count"`
	UpdatedAtMs int64  `json:"updated_at_ms"`
}

type TracksResult struct {
	Playlist PlaylistInfo `json:"playlist"`
	Tracks   []Song       `json:"tracks"`
}

// StreamResult 与 catalog.SourceResult 同构（含 headers）。
type StreamResult struct {
	SongID    int64             `json:"song_id"`
	URL       string            `json:"url"`
	Format    string            `json:"format"`
	Size      int64             `json:"size"`
	ExpiresMS int64             `json:"expires_ms"`
	Level     string            `json:"level"`
	Title     string            `json:"title"`
	Cover     string            `json:"cover"`
	Headers   map[string]string `json:"headers"`
}

/* tidBucket：TopPlaylists 的 tname 聚合桶 */
type tidBucket struct {
	tid   int64
	name  string
	cover string
	count int64
}

/* aid→bvid、tid→tname 会话映射（进程内有效；tracks/search 时填充） */
var (
	mapMu    sync.Mutex
	aid2bvid = map[int64]string{}
	tid2name = map[int64]string{}
)

const allTid = 3 /* 音乐区总榜（rid=3）对应的"全部"伪歌单 id */

func rememberAid(aid int64, bvid string) {
	if aid <= 0 || bvid == "" {
		return
	}
	mapMu.Lock()
	aid2bvid[aid] = bvid
	mapMu.Unlock()
}

func bvidOf(aid int64) string {
	mapMu.Lock()
	defer mapMu.Unlock()
	return aid2bvid[aid]
}

func nameOfTid(tid int64) string {
	mapMu.Lock()
	defer mapMu.Unlock()
	return tid2name[tid]
}

func get(rawURL string) ([]byte, error) {
	req, err := http.NewRequest(http.MethodGet, rawURL, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", userAgent)
	req.Header.Set("Referer", referer)
	if c := cookie(); c != "" {
		req.Header.Set("Cookie", c)
	}
	resp, err := httpc.Do(req)
	if err != nil {
		return nil, fmt.Errorf("bili: 请求失败: %v", err)
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(io.LimitReader(resp.Body, 8<<20))
	if err != nil {
		return nil, fmt.Errorf("bili: 读取失败: %v", err)
	}
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("bili: HTTP %d", resp.StatusCode)
	}
	return body, nil
}

/* ---------------- 排行 / 搜索 ---------------- */

type rankEnvelope struct {
	Code int `json:"code"`
	Data struct {
		List []struct {
			Aid      int64  `json:"aid"`
			Bvid     string `json:"bvid"`
			Title    string `json:"title"`
			Pic      string `json:"pic"`
			Duration int64  `json:"duration"`
			Pubdate  int64  `json:"pubdate"`
			Tid      int64  `json:"tid"`
			Tname    string `json:"tname"`
			Owner    struct {
				Name string `json:"name"`
			} `json:"owner"`
		} `json:"list"`
	} `json:"data"`
}

/* 排行的页级缓存：B站对连续 ranking 请求有风控（-352），
 * TopPlaylists + PlaylistTracks 会连发多页，必须复用缓存。 */
type rankItem struct {
	aid, tid, dur, pub          int64
	bvid, title, pic, tname, up string
}

var (
	rankMu    sync.Mutex
	rankCache = map[int]rankPage{}
)

type rankPage struct {
	at    time.Time
	items []rankItem
}

const rankTTL = 5 * time.Minute

// fetchRankPage 取音乐区排行第 pn 页（带缓存 + -352 重试一次）。
func fetchRankPage(pn int) ([]rankItem, error) {
	rankMu.Lock()
	if p, ok := rankCache[pn]; ok && time.Since(p.at) < rankTTL && len(p.items) > 0 {
		items := p.items
		rankMu.Unlock()
		return items, nil
	}
	rankMu.Unlock()

	u := fmt.Sprintf("%s/x/web-interface/ranking/v2?rid=3&type=all&pn=%d", apiBase, pn)
	var lastErr error
	for attempt := 0; attempt < 2; attempt++ {
		if attempt > 0 {
			time.Sleep(600 * time.Millisecond) // -352 风控：退避后再试一次
		}
		body, err := get(u)
		if err != nil {
			lastErr = err
			continue
		}
		var env rankEnvelope
		if err := json.Unmarshal(body, &env); err != nil {
			return nil, fmt.Errorf("bili: ranking 解析失败: %v", err)
		}
		if env.Code != 0 {
			lastErr = fmt.Errorf("bili: ranking code=%d", env.Code)
			continue
		}
		items := make([]rankItem, 0, len(env.Data.List))
		for _, v := range env.Data.List {
			items = append(items, rankItem{
				aid: v.Aid, bvid: v.Bvid, title: v.Title, pic: v.Pic,
				dur: v.Duration, pub: v.Pubdate, tid: v.Tid,
				tname: v.Tname, up: v.Owner.Name,
			})
			rememberMeta(v.Aid, v.Bvid, v.Tid, v.Tname)
		}
		rankMu.Lock()
		rankCache[pn] = rankPage{at: time.Now(), items: items}
		rankMu.Unlock()
		return items, nil
	}
	return nil, lastErr
}

func songOfRank(v rankItem) Song {
	return songOf(v.aid, v.bvid, v.title, v.pic, v.dur, v.pub, v.up)
}

// Ranking 音乐区排行（rid=3）。page 从 1 开始。
func Ranking(page int) (ListResult, error) {
	if page <= 0 {
		page = 1
	}
	items, err := fetchRankPage(page)
	if err != nil {
		return ListResult{}, err
	}
	res := ListResult{Page: page, Songs: make([]Song, 0, len(items))}
	for _, v := range items {
		res.Songs = append(res.Songs, songOfRank(v))
	}
	res.Total = len(res.Songs)
	return res, nil
}

// TopPlaylists 把音乐区排行按 tname 聚合成"歌单"（id=tid；另含总榜 id=3）。
// 与 catalog.TopPlaylists 返回同形 JSON，UI 的歌单列表页零改动。
func TopPlaylists(page int) (TopResult, error) {
	if page <= 0 {
		page = 1
	}
	var order []int64
	buckets := map[int64]*tidBucket{}
	var total int64

	// 聚合前两页（约 200 条）够生成分类卡片；fetchRankPage 带缓存与 -352 重试
	pages := page + 1
	if pages < 2 {
		pages = 2
	}
	for pn := 1; pn <= pages; pn++ {
		items, err := fetchRankPage(pn)
		if err != nil {
			return TopResult{}, err
		}
		if len(items) == 0 {
			break
		}
		for _, v := range items {
			total++
			if v.tid <= 0 || v.tname == "" {
				continue
			}
			b, ok := buckets[v.tid]
			if !ok {
				b = &tidBucket{tid: v.tid, name: v.tname, cover: httpsIf(v.pic)}
				buckets[v.tid] = b
				order = append(order, v.tid)
			}
			b.count++
		}
	}

	res := TopResult{Cat: "音乐区", Order: "hot", Total: total,
		Playlists: make([]PlaylistSummary, 0, len(order)+1)}
	// 总榜放第一张卡（id=3 = rid）
	res.Playlists = append(res.Playlists, PlaylistSummary{
		ID: allTid, Name: "音乐区排行", Creator: "bilibili",
		Cover: coverOfBucket(buckets, order), TrackCount: total,
	})
	for _, tid := range order {
		b := buckets[tid]
		res.Playlists = append(res.Playlists, PlaylistSummary{
			ID: b.tid, Name: b.name, Creator: "bilibili",
			Cover: b.cover, TrackCount: b.count,
		})
	}
	return res, nil
}

func coverOfBucket(m map[int64]*tidBucket, order []int64) string {
	for _, tid := range order {
		if b := m[tid]; b != nil {
			return b.cover
		}
	}
	return ""
}

// PlaylistTracks id==3 → 全榜；否则按 tname 过滤（跨多页聚合后分页）。
// 与 catalog.PlaylistTracks 返回同形 JSON。
func PlaylistTracks(id int64, limit, offset int) (TracksResult, error) {
	if limit <= 0 {
		limit = 50
	}
	if offset < 0 {
		offset = 0
	}
	want := nameOfTid(id)
	if id != allTid && want == "" {
		// 映射缺失（如冷启动直接点分类）：先刷新一次聚合表
		if _, err := TopPlaylists(1); err != nil {
			return TracksResult{}, err
		}
		want = nameOfTid(id)
	}

	var all []Song
	for pn := 1; pn <= 3; pn++ {
		items, err := fetchRankPage(pn)
		if err != nil {
			return TracksResult{}, err
		}
		if len(items) == 0 {
			break
		}
		for _, v := range items {
			if id != allTid && v.tname != want {
				continue
			}
			all = append(all, songOfRank(v))
		}
	}

	name := want
	if id == allTid {
		name = "音乐区排行"
	}
	if name == "" {
		name = "Bilibili Music"
	}
	res := TracksResult{Playlist: PlaylistInfo{
		ID: id, Name: name, Creator: "bilibili", TrackCount: int64(len(all)),
	}}
	if offset < len(all) {
		end := offset + limit
		if end > len(all) {
			end = len(all)
		}
		res.Tracks = all[offset:end]
	} else {
		res.Tracks = []Song{}
	}
	return res, nil
}

// rememberMeta 记录 aid→bvid 与 tid→tname（取流/分类过滤用）。
func rememberMeta(aid int64, bvid string, tid int64, tname string) {
	if aid <= 0 || bvid == "" {
		return
	}
	mapMu.Lock()
	aid2bvid[aid] = bvid
	if tid > 0 && tname != "" {
		tid2name[tid] = tname
	}
	mapMu.Unlock()
}

type searchEnvelope struct {
	Code int `json:"code"`
	Data struct {
		Page       int `json:"page"`
		NumResults int `json:"numResults"`
		Result     []struct {
			Aid      int64  `json:"aid"`
			Bvid     string `json:"bvid"`
			Title    string `json:"title"`
			Pic      string `json:"pic"`
			Duration string `json:"duration"`
			Pubdate  int64  `json:"pubdate"`
			Author   string `json:"author"`
		} `json:"result"`
	} `json:"data"`
}

// Search B 站视频搜索（需 WBI 签名），限定音乐分区 tids=3。
func Search(keyword string, page int) (ListResult, error) {
	if page <= 0 {
		page = 1
	}
	params := url.Values{}
	params.Set("search_type", "video")
	params.Set("keyword", keyword)
	params.Set("page", strconv.Itoa(page))
	params.Set("tids", "3")
	q, err := signQuery(params)
	if err != nil {
		return ListResult{}, err
	}
	body, err := get(fmt.Sprintf("%s/x/web-interface/wbi/search/type?%s", apiBase, q.Encode()))
	if err != nil {
		return ListResult{}, err
	}
	var env searchEnvelope
	if err := json.Unmarshal(body, &env); err != nil {
		return ListResult{}, fmt.Errorf("bili: search 解析失败: %v", err)
	}
	if env.Code != 0 {
		return ListResult{}, fmt.Errorf("bili: search code=%d", env.Code)
	}
	res := ListResult{Page: env.Data.Page, Total: env.Data.NumResults,
		Songs: make([]Song, 0, len(env.Data.Result))}
	for _, v := range env.Data.Result {
		rememberAid(v.Aid, v.Bvid)
		res.Songs = append(res.Songs, songOf(v.Aid, v.Bvid, emTag.ReplaceAllString(v.Title, ""),
			httpsIf(v.Pic), parseDuration(v.Duration), v.Pubdate, v.Author))
	}
	return res, nil
}

func songOf(aid int64, bvid, title, pic string, durSec, pubdate int64, up string) Song {
	s := Song{
		ID:         aid,
		Name:       strings.TrimSpace(title),
		Album:      "",
		AlbumID:    0,
		DurationMS: durSec * 1000,
		Cover:      httpsIf(pic),
		Playable:   bvid != "",
		PublishMs:  pubdate * 1000,
		Bvid:       bvid,
	}
	if up != "" {
		s.Artists = []string{up}
	}
	return s
}

func httpsIf(u string) string {
	if strings.HasPrefix(u, "//") {
		return "https:" + u
	}
	return u
}

// parseDuration 支持 "mm:ss" / "hh:mm:ss"（搜索接口）。
func parseDuration(s string) int64 {
	parts := strings.Split(strings.TrimSpace(s), ":")
	var total int64
	for _, p := range parts {
		n, err := strconv.ParseInt(strings.TrimSpace(p), 10, 64)
		if err != nil {
			return 0
		}
		total = total*60 + n
	}
	return total
}

/* ---------------- 取流 ---------------- */

type viewEnvelope struct {
	Code int `json:"code"`
	Data struct {
		Bvid     string `json:"bvid"`
		Aid      int64  `json:"aid"`
		Title    string `json:"title"`
		Pic      string `json:"pic"`
		Pubdate  int64  `json:"pubdate"`
		Duration int64  `json:"duration"`
		Cid      int64  `json:"cid"`
		Owner    struct {
			Name string `json:"name"`
		} `json:"owner"`
	} `json:"data"`
}

type playEnvelope struct {
	Code int `json:"code"`
	Data struct {
		Dash struct {
			Audio []struct {
				ID        int64    `json:"id"`
				BaseURL   string   `json:"baseUrl"`
				BackupURL []string `json:"backupUrl"`
				Bandwidth int64    `json:"bandwidth"`
				Size      int64    `json:"size"`
			} `json:"audio"`
		} `json:"dash"`
	} `json:"data"`
}

// Stream 返回音频直链与必需请求头。
func Stream(bvid string) (StreamResult, error) {
	if bvid == "" {
		return StreamResult{}, fmt.Errorf("bili: bvid 为空")
	}
	body, err := get(fmt.Sprintf("%s/x/web-interface/view?bvid=%s", apiBase, url.QueryEscape(bvid)))
	if err != nil {
		return StreamResult{}, err
	}
	var ven viewEnvelope
	if err := json.Unmarshal(body, &ven); err != nil {
		return StreamResult{}, fmt.Errorf("bili: view 解析失败: %v", err)
	}
	if ven.Code != 0 || ven.Data.Cid <= 0 {
		return StreamResult{}, fmt.Errorf("bili: view code=%d", ven.Code)
	}

	pu := fmt.Sprintf("%s/x/player/playurl?bvid=%s&cid=%d&fnval=4048&fourk=1",
		apiBase, url.QueryEscape(bvid), ven.Data.Cid)
	body, err = get(pu)
	if err != nil {
		return StreamResult{}, err
	}
	var pen playEnvelope
	if err := json.Unmarshal(body, &pen); err != nil {
		return StreamResult{}, fmt.Errorf("bili: playurl 解析失败: %v", err)
	}
	if pen.Code != 0 || len(pen.Data.Dash.Audio) == 0 {
		return StreamResult{}, fmt.Errorf("bili: playurl code=%d（无音频轨，可能需登录）", pen.Code)
	}

	// 码率优先；同码率优先 FLAC(30251)
	sort.SliceStable(pen.Data.Dash.Audio, func(i, j int) bool {
		a, b := pen.Data.Dash.Audio[i], pen.Data.Dash.Audio[j]
		if a.Bandwidth != b.Bandwidth {
			return a.Bandwidth > b.Bandwidth
		}
		return audioRank(a.ID) > audioRank(b.ID)
	})
	best := pen.Data.Dash.Audio[0]
	res := StreamResult{
		SongID:    ven.Data.Aid,
		URL:       best.BaseURL,
		Format:    audioExt(best.ID),
		Size:      best.Size,
		Level:     audioLevel(best.ID),
		ExpiresMS: 10800000,
		Title:     ven.Data.Title,
		Cover:     httpsIf(ven.Data.Pic),
		Headers: map[string]string{
			"Referer":    referer,
			"User-Agent": userAgent,
		},
	}
	if res.URL == "" {
		return StreamResult{}, fmt.Errorf("bili: 空直链")
	}
	return res, nil
}

func audioRank(id int64) int {
	switch id {
	case 30251:
		return 4
	case 30280:
		return 3
	case 30232:
		return 2
	case 30216:
		return 1
	}
	return 0
}

func audioLevel(id int64) string {
	switch id {
	case 30251:
		return "Hi-Res"
	case 30280:
		return "192K"
	case 30232:
		return "132K"
	case 30216:
		return "64K"
	}
	return "audio"
}

func audioExt(id int64) string {
	if id == 30251 {
		return "flac"
	}
	return "m4a"
}

/* ---------------- WBI 签名 ---------------- */

var mixinKeyEncTab = []int{
	46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
	27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13,
	37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60, 51, 30, 4,
	22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52,
}

type navEnvelope struct {
	Data struct {
		WbiImg struct {
			ImgURL string `json:"img_url"`
			SubURL string `json:"sub_url"`
		} `json:"wbi_img"`
	} `json:"data"`
}

func wbiKeys() (string, string, error) {
	body, err := get(apiBase + "/x/web-interface/nav")
	if err != nil {
		return "", "", err
	}
	var env navEnvelope
	if err := json.Unmarshal(body, &env); err != nil {
		return "", "", fmt.Errorf("bili: nav 解析失败: %v", err)
	}
	img := basename(env.Data.WbiImg.ImgURL)
	sub := basename(env.Data.WbiImg.SubURL)
	if len(img) < 32 || len(sub) < 32 {
		return "", "", fmt.Errorf("bili: wbi 键缺失")
	}
	return img, sub, nil
}

func basename(u string) string {
	i := strings.LastIndex(u, "/")
	s := u[i+1:]
	if j := strings.Index(s, "."); j >= 0 {
		s = s[:j]
	}
	return s
}

func mixinKey(orig string) string {
	out := make([]byte, 0, 32)
	for _, i := range mixinKeyEncTab {
		if i < len(orig) {
			out = append(out, orig[i])
		}
		if len(out) == 32 {
			break
		}
	}
	return string(out)
}

func signQuery(params url.Values) (url.Values, error) {
	img, sub, err := wbiKeys()
	if err != nil {
		return params, err
	}
	mk := mixinKey(img + sub)
	params.Set("wts", strconv.FormatInt(time.Now().Unix(), 10))
	// 按 key 排序后拼接
	keys := make([]string, 0, len(params))
	for k := range params {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	var sb strings.Builder
	for i, k := range keys {
		if i > 0 {
			sb.WriteByte('&')
		}
		v := params.Get(k)
		// 过滤 !'()* 字符（官方 web 端行为）
		v = strings.NewReplacer("!", "", "'", "", "(", "", ")", "", "*", "").Replace(v)
		params.Set(k, v)
		sb.WriteString(url.QueryEscape(k))
		sb.WriteByte('=')
		sb.WriteString(url.QueryEscape(v))
	}
	sum := md5.Sum([]byte(sb.String() + mk))
	params.Set("w_rid", hex.EncodeToString(sum[:]))
	return params, nil
}

// StreamByAid 按 aid 取直链（映射缺失时用 WBI 搜索回填一次）。
func StreamByAid(aid int64) (StreamResult, error) {
	if aid <= 0 {
		return StreamResult{}, fmt.Errorf("bili: aid 无效")
	}
	bv := bvidOf(aid)
	if bv == "" {
		/* 冷启动（未经曲目列表）：直接 view?aid= 拿 bvid 回填映射 */
		body, err := get(fmt.Sprintf("%s/x/web-interface/view?aid=%d", apiBase, aid))
		if err != nil {
			return StreamResult{}, err
		}
		var ven viewEnvelope
		if err := json.Unmarshal(body, &ven); err == nil && ven.Data.Bvid != "" {
			bv = ven.Data.Bvid
			rememberAid(aid, bv)
		}
	}
	if bv == "" {
		return StreamResult{}, fmt.Errorf("bili: 找不到 aid=%d 的 bvid", aid)
	}
	return Stream(bv)
}
