/*
 * ncm.hpp - libncm 的 C++17 RAII 封装
 *
 * 设计原则：
 *  - 不抛异常。用 Status 返回值表达失败，避免异常穿越 C ABI。
 *    需要抛异常语义时用 ncm::throw_if_error()。
 *  - 所有 char* 出参在函数返回前已释放，不泄漏、不 double free。
 *  - Client 可移动、不可拷贝。
 *  - 事件回调保持 C 函数指针形式（extern "C"）。带捕获的 lambda 无法
 *    转成该类型，请把自己的状态放进 user_data。
 *
 * 所有返回 std::string 的接口携带的是原始 JSON，字段名见各处注释。
 * 本头文件不做 JSON 解析，交由调用方按需选择解析库。
 */
#ifndef NCM_HPP
#define NCM_HPP

#include "ncm.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(_MSVC_LANG)
#  define NCM_CXX_LANG _MSVC_LANG
#else
#  define NCM_CXX_LANG __cplusplus
#endif

#if NCM_CXX_LANG < 201703L
#  error "ncm.hpp requires C++17 or later"
#endif

static_assert(sizeof(int64_t) == 8, "ncm requires 64-bit int64_t");
static_assert(sizeof(int32_t) == 4, "ncm requires 32-bit int32_t");

namespace ncm {

enum class Status : int32_t {
  Ok             = NCM_OK,
  InvalidArg     = NCM_ERR_INVALID_ARG,
  Network        = NCM_ERR_NETWORK,
  Unauthorized   = NCM_ERR_UNAUTHORIZED,
  Captcha        = NCM_ERR_CAPTCHA,
  GeoBlock       = NCM_ERR_GEO_BLOCK,
  NoResource     = NCM_ERR_NO_RESOURCE,
  TrialOnly      = NCM_ERR_TRIAL_ONLY,
  NotFound       = NCM_ERR_NOT_FOUND,
  NotImplemented = NCM_ERR_NOT_IMPLEMENTED,
  Upstream       = NCM_ERR_UPSTREAM,
  Internal       = NCM_ERR_INTERNAL,
};

enum class Quality : int32_t {
  Standard = NCM_QUALITY_STANDARD,
  Higher   = NCM_QUALITY_HIGHER,
  Exhigh   = NCM_QUALITY_EXHIGH,
  Lossless = NCM_QUALITY_LOSSLESS,
  Hires    = NCM_QUALITY_HIRES,
  Jyeffect = NCM_QUALITY_JYEFFECT,
  Sky      = NCM_QUALITY_SKY,
  Jymaster = NCM_QUALITY_JYMASTER,
};

enum class SourceType : int32_t {
  Remote     = NCM_SOURCE_REMOTE,
  Cached     = NCM_SOURCE_CACHED,
  Downloaded = NCM_SOURCE_DOWNLOADED,
};

enum class QrStatus : int32_t {
  Expired     = NCM_QS_EXPIRED,
  WaitScan    = NCM_QS_WAIT_SCAN,
  WaitConfirm = NCM_QS_WAIT_CONFIRM,
  Ok          = NCM_QS_OK,
  Captcha     = NCM_QS_CAPTCHA,
  Unknown     = -1,
};

enum class Event : int32_t {
  Progress = NCM_EVT_PROGRESS,
  Login    = NCM_EVT_LOGIN,
  PlayFail = NCM_EVT_PLAYFAIL,
};

[[nodiscard]] inline std::string_view to_string(Status s) noexcept {
  switch (s) {
    case Status::Ok:             return "Ok";
    case Status::InvalidArg:     return "InvalidArg";
    case Status::Network:        return "Network";
    case Status::Unauthorized:   return "Unauthorized";
    case Status::Captcha:        return "Captcha";
    case Status::GeoBlock:       return "GeoBlock";
    case Status::NoResource:     return "NoResource";
    case Status::TrialOnly:      return "TrialOnly";
    case Status::NotFound:       return "NotFound";
    case Status::NotImplemented: return "NotImplemented";
    case Status::Upstream:       return "Upstream";
    case Status::Internal:       return "Internal";
  }
  return "Unknown";
}

[[nodiscard]] inline std::string to_string(QrStatus s) {
  switch (s) {
    case QrStatus::Expired:     return "Expired";
    case QrStatus::WaitScan:    return "WaitScan";
    case QrStatus::WaitConfirm: return "WaitConfirm";
    case QrStatus::Ok:          return "Ok";
    case QrStatus::Captcha:     return "Captcha";
    case QrStatus::Unknown:     return "Unknown";
  }
  return "Unknown";
}

/* 把 ncm_status 映射为 Status；未知值归为 Internal。 */
[[nodiscard]] inline Status from_c(int32_t code) noexcept {
  switch (code) {
    case NCM_OK:                  return Status::Ok;
    case NCM_ERR_INVALID_ARG:     return Status::InvalidArg;
    case NCM_ERR_NETWORK:         return Status::Network;
    case NCM_ERR_UNAUTHORIZED:    return Status::Unauthorized;
    case NCM_ERR_CAPTCHA:         return Status::Captcha;
    case NCM_ERR_GEO_BLOCK:       return Status::GeoBlock;
    case NCM_ERR_NO_RESOURCE:     return Status::NoResource;
    case NCM_ERR_TRIAL_ONLY:      return Status::TrialOnly;
    case NCM_ERR_NOT_FOUND:       return Status::NotFound;
    case NCM_ERR_NOT_IMPLEMENTED: return Status::NotImplemented;
    case NCM_ERR_UPSTREAM:        return Status::Upstream;
    default:                      return Status::Internal;
  }
}

namespace detail {

/* 入参缓冲：用 malloc 分配，析构时 free。
 * 刻意不使用 operator new，避免与出参的 ncm_free_string 释放路径混用。 */
class InStr {
public:
  explicit InStr(std::string_view s) : p_(cstr(s)) {}
  ~InStr() { std::free(p_); }

  InStr(const InStr &) = delete;
  InStr &operator=(const InStr &) = delete;

  [[nodiscard]] char *get() const noexcept { return p_; }
  operator char *() const noexcept { return p_; }

private:
  static char *cstr(std::string_view s) {
    const std::size_t n = s.size() + 1;
    auto *p = static_cast<char *>(std::malloc(n));
    if (!p) throw std::bad_alloc{};
    if (!s.empty()) std::memcpy(p, s.data(), s.size());
    p[s.size()] = '\0';
    return p;
  }

  char *p_;
};

} // namespace detail

class Client {
public:
  explicit Client(std::string_view config_dir = {}) {
    detail::InStr dir(config_dir);
    h_ = ncm_new(dir.get());
  }

  ~Client() {
    if (h_) {
      ncm_free(h_);
      h_ = nullptr;
    }
  }

  Client(Client &&other) noexcept : h_(std::exchange(other.h_, nullptr)) {}
  Client &operator=(Client &&other) noexcept {
    if (this != &other) {
      if (h_) ncm_free(h_);
      h_ = std::exchange(other.h_, nullptr);
    }
    return *this;
  }

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  [[nodiscard]] bool valid() const noexcept { return h_ != nullptr; }
  explicit operator bool() const noexcept { return valid(); }

  /* ---- 认证 ---- */

  /* out_json: 账户信息（昵称、UID、头像等） */
  [[nodiscard]] Status login_cookie(std::string_view cookie, std::string &out_json) {
    detail::InStr c(cookie);
    return fetch([&](char **o) { return ncm_login_cookie(h_, c.get(), o); }, out_json);
  }

  /* out_json: {"qrcode_url":"...","unikey":"..."} —— 把 qrcode_url 渲染成二维码即可 */
  [[nodiscard]] Status qr_begin(std::string &out_json) {
    return fetch([&](char **o) { return ncm_qr_begin(h_, o); }, out_json);
  }

  /* out_json: {"status":801,...}
   * 返回 Status::Ok 表示轮询成功（可能仍在等待），
   * 返回 Status::Captcha 表示 8821 触发风控，需要行为验证码。 */
  [[nodiscard]] Status qr_poll(std::string_view unikey, std::string &out_json) {
    detail::InStr k(unikey);
    const Status s = fetch([&](char **o) { return ncm_qr_poll(h_, k.get(), o); }, out_json);
    if (s == Status::Captcha && out_json.empty()) out_json = R"({"status":8821})";
    return s;
  }

  [[nodiscard]] Status account(std::string &out_json) {
    return fetch([&](char **o) { return ncm_account(h_, o); }, out_json);
  }

  Status logout() noexcept { return from_c(ncm_logout(h_)); }

  /* ---- 目录 ---- */

  [[nodiscard]] Status daily_recommend(std::string &out_json) {
    return fetch([&](char **o) { return ncm_daily_recommend(h_, o); }, out_json);
  }

  [[nodiscard]] Status playlist_tracks(int64_t id, std::string &out_json) {
    return fetch([&](char **o) { return ncm_playlist_tracks(h_, id, o); }, out_json);
  }

  [[nodiscard]] Status album_tracks(int64_t id, std::string &out_json) {
    return fetch([&](char **o) { return ncm_album_tracks(h_, id, o); }, out_json);
  }

  [[nodiscard]] Status search(std::string_view keywords, int32_t limit, std::string &out_json) {
    detail::InStr k(keywords);
    return fetch([&](char **o) { return ncm_search(h_, k.get(), limit, o); }, out_json);
  }

  /* ---- 播放 ---- */

  /* out_json:
   * {"type":"downloaded|cached|remote","playable":true,"url":"...","path":"...",
   *  "musicType":"mp3","size":10691439,"quality":"exhigh","reason":""} */
  [[nodiscard]] Status resolve_source(int64_t song_id, Quality q, std::string &out_json) {
    const int32_t qv = static_cast<int32_t>(q);
    return fetch([&](char **o) { return ncm_resolve_source(h_, song_id, qv, o); }, out_json);
  }

  [[nodiscard]] Status download(int64_t song_id, Quality q, std::string &out_json) {
    const int32_t qv = static_cast<int32_t>(q);
    return fetch([&](char **o) { return ncm_download(h_, song_id, qv, o); }, out_json);
  }

  /* 403 防盗链兜底：https://music.163.com/song/media/outer/url?id=<song_id>.mp3
   * 播放器需带 Referer: https://music.163.com */
  [[nodiscard]] Status outer_url(int64_t song_id, std::string &out_json) {
    return fetch([&](char **o) { return ncm_outer_url(h_, song_id, o); }, out_json);
  }

  Status clear_cache() noexcept { return from_c(ncm_clear_cache(h_)); }

  /* ---- 歌词 ---- */

  [[nodiscard]] Status lyric(int64_t song_id, std::string &out_json) {
    return fetch([&](char **o) { return ncm_lyric(h_, song_id, o); }, out_json);
  }

  /* ---- 事件 ---- */

  /* cb 必须是静态函数或无捕获 lambda（可转换为 ncm_event_fn）。
   * 带捕获请把状态放进 user_data，并自行保证其生命周期。 */
  void on_event(ncm_event_fn cb, void *user_data) noexcept {
    ncm_set_event_cb(h_, cb, user_data);
  }

  [[nodiscard]] static std::string version() {
    char *v = ncm_version();
    std::string out = v ? v : "";
    ncm_free_string(v);
    return out;
  }

private:
  /* 统一处理 char* 出参：拷贝进 std::string 后立即归还给库，避免泄漏。 */
  template <typename F>
  static Status fetch(F &&call, std::string &out_json) {
    char *out = nullptr;
    const Status s = from_c(call(&out));
    if (s == Status::Ok) out_json.assign(out ? out : "");
    ncm_free_string(out); /* 对 NULL 是空操作 */
    return s;
  }

  ncm_client *h_ = nullptr;
};

inline void throw_if_error(Status s) {
  if (s != Status::Ok) {
    throw std::runtime_error(std::string(to_string(s)));
  }
}

} // namespace ncm

#endif /* NCM_HPP */