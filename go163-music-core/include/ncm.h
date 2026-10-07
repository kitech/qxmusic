/*
 * ncm.h - libncm 公共 C ABI
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
typedef void (NCM_CALL *ncm_event_fn)(void *user_data, int32_t event, const char *json);

/* ---- 生命周期 ---- */

/* 创建客户端。config_dir 为配置与缓存根目录，传 NULL 使用默认目录。
 * 返回 NULL 表示创建失败。 */
NCM_API ncm_client *NCM_CALL ncm_new(const char *config_dir);

/* 释放客户端。传入 NULL 是合法的空操作。
 * 对同一句柄重复调用同样是安全的空操作（实现靠句柄内的 magic 做失效判断），
 * 但把 ncm_free(NULL) 与重复调用混用属于设计不清晰，应避免。 */
NCM_API void NCM_CALL ncm_free(ncm_client *client);

/* 返回库版本，形如 "0.1.0 (abi 1)"。由库分配，需 ncm_free_string() 释放。 */
NCM_API char *NCM_CALL ncm_version(void);

/* 释放任何 char* 出参。对 NULL 是空操作。 */
NCM_API void NCM_CALL ncm_free_string(char *s);

/* 设置事件回调。cb 为 NULL 表示取消订阅。回调在库内部 goroutine 上触发。 */
NCM_API void NCM_CALL ncm_set_event_cb(ncm_client *client, ncm_event_fn cb, void *user_data);

/* ---- 认证 ---- */

NCM_API int32_t NCM_CALL ncm_login_cookie(ncm_client *client, const char *cookie, char **out_json);
NCM_API int32_t NCM_CALL ncm_qr_begin(ncm_client *client, char **out_json);
NCM_API int32_t NCM_CALL ncm_qr_poll(ncm_client *client, const char *unikey, char **out_json);
NCM_API int32_t NCM_CALL ncm_account(ncm_client *client, char **out_json);
NCM_API int32_t NCM_CALL ncm_logout(ncm_client *client);

/* ---- 目录 ---- */

NCM_API int32_t NCM_CALL ncm_daily_recommend(ncm_client *client, char **out_json);
NCM_API int32_t NCM_CALL ncm_playlist_tracks(ncm_client *client, int64_t playlist_id, char **out_json);
NCM_API int32_t NCM_CALL ncm_album_tracks(ncm_client *client, int64_t album_id, char **out_json);
NCM_API int32_t NCM_CALL ncm_search(ncm_client *client, const char *keywords, int32_t limit, char **out_json);

/* ---- 播放 ---- */

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