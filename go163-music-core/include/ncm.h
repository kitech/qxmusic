/*
 * ncm.h - libncm 公共 C ABI
 *
 * ---- 认证要求矩阵（与 internal/catalog/doc.go 必须同步） ----
 *
 *   匿名可用：
 *     ncm_playlist_categories   分类目录（catalogue+hottags）
 *     ncm_top_playlists         分类歌单列表
 *     ncm_playlist_detail       公开歌单元信息
 *     ncm_playlist_tracks       公开歌单曲目
 *     ncm_album_tracks          专辑曲目
 *     ncm_search                搜索（type=1）
 *     ncm_song_detail           批量歌曲详情
 *     ncm_artist_detail         艺人详情（头像 URL）
 *     ncm_lyric                 歌词
 *     ncm_check_music           可用性探测
 *     ncm_resolve_source        免费曲直链
 *   需登录（服务端 301/302 -> NCM_ERR_UNAUTHORIZED）：
 *     ncm_daily_recommend           每日推荐
 *     ncm_playlist_tracks/detail    私密歌单（"我喜欢的音乐"等）
 *     ncm_account                   真实账号（匿名是游客态）
 *     ncm_resolve_source            VIP 专属音质 / 无版权高码率
 *   资源类：
 *     灰歌/无版权 -> NCM_ERR_NO_RESOURCE（url 为空）
 *     仅试听     -> NCM_ERR_TRIAL_ONLY（freeTrialInfo 非空）
 *     风控       -> NCM_ERR_CAPTCHA（8821）
 *     传输失败   -> NCM_ERR_NETWORK（依赖库固定 code 520）
 *
 * 已知副作用：依赖库在 code != 200 时把含 cookie 的请求上下文打进默认
 * log；libncm 已在首次 ncm_new 安装脱敏 writer（MUSIC_U 等值变 ***）。
 *
 * ABI 约定（在所有平台上都必须遵守，跨平台兼容性的基础）：
 *
 *  1. 不跨边界传递结构体。仅使用定宽整数（int32_t/int64_t/uint32_t）。
 *     禁止使用 long / size_t / bool —— long 在 Windows 是 32 位、
 *     在 Linux/macOS 是 64 位，size_t 随位宽变化，bool 大小不保证。
 *  2. 所有字符串为 UTF-8 且以 NUL 结尾。调用方传入的字符串由调用方保证
 *     在调用期间有效（不需要长期存活）。
 *  3. 所有 char* 出参由库内分配，必须用 ncm_free_string() 释放。
 *     函数返回非 NCM_OK 时，出参不会被分配，调用方不应释放。
 *  4. 除 ncm_new() 外，所有函数第一个参数 ncm_client* 可以为 NULL，
 *     此时返回 NCM_ERR_INVALID_ARG 而不是崩溃。
 *  5. 回调（ncm_event_fn）为单向 Go -> C 调用。回调内部不得同步重入
 *     本库的任何函数，否则会与库内互斥量死锁。需要在回调里做事请
 *     投递到自己的事件循环。
 *  6. 单个客户端内部串行化。所有导出的非回调函数在返回前已释放内部锁，
 *     但仍不建议从多个线程同时调用同一个 ncm_client*。
 *  7. 本库不会调用 exit()，也不会把 panic 传播到宿主进程。
 */
#ifndef NCM_H
#define NCM_H

#include <stdint.h>
#include <stddef.h>

/* cgo 导出的 Go 侧无法表达 const；导出实现（char*）与声明（const char*）
 * 在 C 链接下兼容。cgo 构建传 -DNCM_GO_CGO 时 NCM_CPARAM 去掉 const，
 * 避免导出头与公共头原型冲突。宿主（C/C++）侧保持 const。 */
#ifdef NCM_GO_CGO
#define NCM_CPARAM /*const 摘除*/
#else
#define NCM_CPARAM const
#endif

#if defined(_WIN32)
#  if defined(NCM_BUILDING_SHARED)
#    define NCM_API __declspec(dllexport)
#  else
#    define NCM_API __declspec(dllimport)
#  endif
#  define NCM_CALL __cdecl
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define NCM_API __attribute__((visibility("default")))
#  else
#    define NCM_API
#  endif
#  define NCM_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明客户端句柄。禁止对其 sizeof，禁止解引用。
 * 只能由 ncm_new() 创建，且必须恰好调用一次 ncm_free() 释放。 */
typedef struct ncm_client ncm_client;

typedef enum {
  NCM_OK                = 0,
  NCM_ERR_INVALID_ARG   = -1,
  NCM_ERR_NETWORK       = -2,
  NCM_ERR_UNAUTHORIZED  = -3, /* 301 未登录 */
  NCM_ERR_CAPTCHA       = -4, /* 8821 触发风控，需要行为验证码 */
  NCM_ERR_GEO_BLOCK     = -5, /* 460 / 版权地区限制 */
  NCM_ERR_NO_RESOURCE   = -6, /* 无可播资源（灰歌） */
  NCM_ERR_TRIAL_ONLY    = -7, /* 仅试听片段，非完整歌曲 */
NCM_ERR_NOT_FOUND     = -8,
  NCM_ERR_NOT_IMPLEMENTED = -9,
  NCM_ERR_UPSTREAM     = -10, /* 上游返回非 200 或响应无法解析 */
  NCM_ERR_INTERNAL     = -100
} ncm_status;

typedef enum {
  NCM_EVT_PROGRESS = 1, /* 下载 / 后台缓存进度 */
  NCM_EVT_LOGIN    = 2, /* 登录态变化 */
  NCM_EVT_PLAYFAIL = 3  /* 播放失败，建议跳下一首 */
} ncm_event;

typedef enum {
  NCM_QS_EXPIRED      = 800,
  NCM_QS_WAIT_SCAN    = 801,
  NCM_QS_WAIT_CONFIRM = 802,
  NCM_QS_OK           = 803,
  NCM_QS_CAPTCHA      = 8821
} ncm_qr_status;

typedef enum {
  NCM_QUALITY_STANDARD = 128000,
  NCM_QUALITY_HIGHER   = 192000,
  NCM_QUALITY_EXHIGH   = 320000,
  NCM_QUALITY_LOSSLESS = 999000,
  NCM_QUALITY_HIRES    = 1000000,
  NCM_QUALITY_JYEFFECT = 100001,
  NCM_QUALITY_SKY      = 100002,
  NCM_QUALITY_JYMASTER = 100003
} ncm_quality;

typedef enum {
  NCM_SOURCE_REMOTE    = 0, /* 网络 URL */
  NCM_SOURCE_CACHED    = 1, /* 本地缓存 */
  NCM_SOURCE_DOWNLOADED = 2  /* 已下载文件 */
} ncm_source_type;

/* user_data 必须能安全地存活到 ncm_set_event_cb() 被再次调用或
 * ncm_free() 之后 —— 库不会持有它的所有权，也不会释放它。 */
typedef void (NCM_CALL *ncm_event_fn)(void *user_data, int32_t event, NCM_CPARAM char *json);

/* ---- 生命周期 ---- */

/* 创建客户端。config_dir 为配置与缓存根目录，传 NULL 使用默认目录。
 * 返回 NULL 表示创建失败。 */
NCM_API ncm_client *NCM_CALL ncm_new(NCM_CPARAM char *config_dir);

/* 释放客户端。传入 NULL 是合法的空操作。
 * 对同一句柄重复调用同样是安全的空操作（实现靠句柄内的 magic 做失效判断），
 * 但把 ncm_free(NULL) 与重复调用混用属于设计不清晰，应避免。 */
NCM_API void NCM_CALL ncm_free(ncm_client *client);

/* 返回库版本，形如 "0.1.0 (abi 2)"。由库分配，需 ncm_free_string() 释放。 */
NCM_API char *NCM_CALL ncm_version(void);

/* 释放任何 char* 出参。对 NULL 是空操作。 */
NCM_API void NCM_CALL ncm_free_string(char *s);

/* 设置事件回调。cb 为 NULL 表示取消订阅。回调在库内部 goroutine 上触发。 */
NCM_API void NCM_CALL ncm_set_event_cb(ncm_client *client, ncm_event_fn cb, void *user_data);

/* ---- 认证 ---- */

NCM_API int32_t NCM_CALL ncm_login_cookie(ncm_client *client, NCM_CPARAM char *cookie, char **out_json);
NCM_API int32_t NCM_CALL ncm_qr_begin(ncm_client *client, char **out_json);
NCM_API int32_t NCM_CALL ncm_qr_poll(ncm_client *client, NCM_CPARAM char *unikey, char **out_json);
NCM_API int32_t NCM_CALL ncm_account(ncm_client *client, char **out_json);
NCM_API int32_t NCM_CALL ncm_logout(ncm_client *client);

/* ---- 目录（认证要求见顶部矩阵；出参 JSON 契约见各行注释） ---- */

/* 认证: 需登录。出参: 每日推荐歌曲数组 */
NCM_API int32_t NCM_CALL ncm_daily_recommend(ncm_client *client, char **out_json);

/* 认证: 否（公开歌单）。出参: {"playlist":{id,name,cover,creator,track_count,description},"tracks":[{id,name,artists[],album,album_id,duration_ms,cover,playable}]} */
NCM_API int32_t NCM_CALL ncm_playlist_tracks(ncm_client *client, int64_t playlist_id, char **out_json);
/* 认证: 否。 */
NCM_API int32_t NCM_CALL ncm_album_tracks(ncm_client *client, int64_t album_id, char **out_json);
/* 认证: 否。 */
NCM_API int32_t NCM_CALL ncm_search(ncm_client *client, NCM_CPARAM char *keywords, int32_t limit, char **out_json);

/* 认证: 否。出参: {"hot":[...],"groups":[{"name":"语种","categories":[...]}]} */
NCM_API int32_t NCM_CALL ncm_playlist_categories(ncm_client *client, char **out_json);
/* 认证: 否。cat 为分类名（空=全部），仅 "hot"/"new"。出参: {"cat","order","total","more","playlists":[{id,name,cover,creator,track_count,play_count}]} */
NCM_API int32_t NCM_CALL ncm_top_playlists(ncm_client *client, NCM_CPARAM char *cat, NCM_CPARAM char *order,
                                           int32_t limit, int32_t offset, char **out_json);
/* 认证: 否（公开歌单）。出参: {id,name,cover,creator,track_count,description} */
NCM_API int32_t NCM_CALL ncm_playlist_detail(ncm_client *client, int64_t id, char **out_json);
/* 认证: 否。ids 为逗号分隔歌曲 id。出参: {"songs":[...]}（结构同 tracks 元素） */
NCM_API int32_t NCM_CALL ncm_song_detail(ncm_client *client, NCM_CPARAM char *ids, char **out_json);
/* 认证: 否。出参: {"id","name","pic_url"}（艺人头像，可能为空串） */
NCM_API int32_t NCM_CALL ncm_artist_detail(ncm_client *client, int64_t id, char **out_json);
/* 认证: 否。br 传 0 用默认 999000。出参: {"song_id","playable","reason"} */
NCM_API int32_t NCM_CALL ncm_check_music(ncm_client *client, int64_t song_id, int32_t br, char **out_json);

/* ---- 收藏（与账号"喜欢的音乐"互通） ---- */

/* 认证: 是（未登录返回 301）。出参: {"ids":[123,456,...]}（账号喜欢的全量歌曲 id，顺序=收藏顺序） */
NCM_API int32_t NCM_CALL ncm_like_list(ncm_client *client, char **out_json);
/* 认证: 是。like 非 0 = 收藏，0 = 取消。出参: {"song_id","liked"} */
NCM_API int32_t NCM_CALL ncm_like(ncm_client *client, int64_t song_id, int32_t like, char **out_json);

/* ---- 播放 ---- */

/* 认证: 免费曲否；VIP 音质/无版权 301 或空 url。出参: {"song_id","url","source","quality","format","size","expires_ms","level"} */
NCM_API int32_t NCM_CALL ncm_resolve_source(ncm_client *client, int64_t song_id, int32_t quality, char **out_json);
NCM_API int32_t NCM_CALL ncm_download(ncm_client *client, int64_t song_id, int32_t quality, char **out_json);
NCM_API int32_t NCM_CALL ncm_outer_url(ncm_client *client, int64_t song_id, char **out_json);
NCM_API int32_t NCM_CALL ncm_clear_cache(ncm_client *client);

/* ---- 歌词 ---- */

NCM_API int32_t NCM_CALL ncm_lyric(ncm_client *client, int64_t song_id, char **out_json);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NCM_H */