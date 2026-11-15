// Package ytm 提供 YouTube Music 的检索与取流。
//
// 路线定案（2026-11 实测）：
//   - Piped 官方实例 API 502，多实例不可达；Invidious 公共实例全部 403 disabled；
//   - InnerTube player 直调需 PO token（WEB_REMIX 搜索可用但取流全灭）；
//   - yt-dlp（2026.10.26 实测）搜索/直链/取流全通 → 采用 exec yt-dlp。
//
// 代理：默认 http://127.0.0.1:10809（YTM_PROXY 环境变量覆盖，
// ncm_ytm_set_proxy 动态改）。注意 googlevideo 直链回源也必须走代理
// （实测直连超时、走代理 206），播放端须用同一代理。
package ytm

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"
)

var (
	mu    sync.Mutex
	proxy = "http://127.0.0.1:10809"
	bin   = "yt-dlp"
)

func init() {
	if v := os.Getenv("YTM_PROXY"); v != "" {
		proxy = v
	}
}

// SetProxy 动态改代理（空串保持不变）。
func SetProxy(p string) {
	mu.Lock()
	defer mu.Unlock()
	if p != "" {
		proxy = p
	}
}

// SetBin 指定 yt-dlp 可执行文件路径（空串保持不变；默认 PATH 查找）。
func SetBin(b string) {
	mu.Lock()
	defer mu.Unlock()
	if b != "" {
		bin = b
	}
}

// Available 报告 yt-dlp 是否可用（PATH 查找）。不可用时 UI 侧提示降级。
func Available() bool {
	_, err := exec.LookPath(binPath())
	return err == nil
}

func binPath() string {
	mu.Lock()
	defer mu.Unlock()
	return bin
}

func proxyAddr() string {
	mu.Lock()
	defer mu.Unlock()
	return proxy
}

// Song 与 internal/catalog.Song 的 JSON 形状对齐（id 为 videoId 字符串，
// 便于双端复用同一解析路径；publish_ms 置 0）。
type Song struct {
	ID         string   `json:"id"`
	Name       string   `json:"name"`
	Artists    []string `json:"artists"`
	ArtistIDs  []int64  `json:"artist_ids"`
	Album      string   `json:"album"`
	AlbumID    int64    `json:"album_id"`
	DurationMS int64    `json:"duration_ms"`
	Cover      string   `json:"cover"`
	Playable   bool     `json:"playable"`
	PublishMs  int64    `json:"publish_ms"`
}

// SearchResult 与 catalog.SongDetailResult 形状对齐（UI 直接复用解析）。
type SearchResult struct {
	Songs []Song `json:"songs"`
}

// StreamResult 取流结果：url 为 googlevideo 直链（须走代理播放）。
type StreamResult struct {
	VideoID string `json:"video_id"`
	URL     string `json:"url"`
	Title   string `json:"title,omitempty"`
}

// ytdlpEntry 是 --dump-json 扁平输出的字段视图（只取需要的）。
type ytdlpEntry struct {
	ID        string  `json:"id"`
	Title     string  `json:"title"`
	Channel   string  `json:"channel"`
	Duration  float64 `json:"duration"`
	Timestamp float64 `json:"timestamp"`
}

// Search 用 ytsearchN 扁平搜索。limit<=0 时取 30。
func Search(query string, limit int) (SearchResult, error) {
	if limit <= 0 {
		limit = 30
	}
	out, err := run("--flat-playlist", "--dump-json",
		"-I", "1:"+strconv.Itoa(limit),
		"ytsearch"+strconv.Itoa(limit)+":"+query)
	if err != nil {
		return SearchResult{}, err
	}
	res := SearchResult{Songs: make([]Song, 0, limit)}
	for _, line := range strings.Split(strings.TrimSpace(string(out)), "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		var e ytdlpEntry
		if err := json.Unmarshal([]byte(line), &e); err != nil {
			continue // 单行坏数据不致命
		}
		if e.ID == "" {
			continue
		}
		s := Song{
			ID:       e.ID,
			Name:     e.Title,
			Playable: true,
		}
		if e.Channel != "" {
			s.Artists = []string{e.Channel}
		}
		if e.Duration > 0 {
			s.DurationMS = int64(e.Duration * 1000)
		}
		if e.Timestamp > 0 {
			s.PublishMs = time.Unix(int64(e.Timestamp), 0).UnixMilli()
		}
		res.Songs = append(res.Songs, s)
	}
	if len(res.Songs) == 0 {
		return SearchResult{}, fmt.Errorf("ytm: 搜索无结果: %s", query)
	}
	return res, nil
}

// Stream 返回直链 audio URL（googlevideo，播放端须走同一代理）。
func Stream(videoID string) (StreamResult, error) {
	if videoID == "" {
		return StreamResult{}, fmt.Errorf("ytm: video_id 为空")
	}
	out, err := run("-f", "bestaudio[ext=m4a]/bestaudio", "-g", videoID)
	if err != nil {
		return StreamResult{}, err
	}
	url := strings.TrimSpace(strings.SplitN(string(out), "\n", 2)[0])
	if url == "" {
		return StreamResult{}, fmt.Errorf("ytm: 未取得直链: %s", videoID)
	}
	return StreamResult{VideoID: videoID, URL: url}, nil
}

func run(extra ...string) ([]byte, error) {
	argv := append([]string{"--proxy", proxyAddr(), "--no-warnings"}, extra...)
	cmd := exec.Command(binPath(), argv...)
	var stderr strings.Builder
	cmd.Stderr = &stderr
	out, err := cmd.Output()
	if err != nil {
		msg := stderr.String()
		if msg == "" {
			if _, lookErr := exec.LookPath(binPath()); lookErr != nil {
				return nil, fmt.Errorf("ytm: yt-dlp 不可用（未安装或不在 PATH）")
			}
		}
		// 截尾避免刷屏；yt-dlp 失败信息在 stderr 尾部
		if len(msg) > 400 {
			msg = msg[len(msg)-400:]
		}
		return nil, fmt.Errorf("ytm: yt-dlp 失败: %s", strings.TrimSpace(msg))
	}
	return out, nil
}
