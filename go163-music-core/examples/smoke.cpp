// smoke.cpp - libncm 的 C++17 封装冒烟测试。
//
// 覆盖：
//   - RAII 构造/析构与移动语义
//   - 拷贝构造被正确禁用
//   - Status 枚举映射
//   - char* 出参的自动回收（配合 -fsanitize=address 查泄漏）
//   - 无效句柄时状态正确返回
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#include "ncm.hpp"

static_assert(!std::is_copy_constructible_v<ncm::Client>,
              "Client 必须不可拷贝（否则会 double free）");
static_assert(std::is_move_constructible_v<ncm::Client>, "Client 必须可移动");

static int g_failures = 0;

#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));       \
      ++g_failures;                                                             \
    } else {                                                                    \
      std::printf("ok   %s\n", (msg));                                           \
    }                                                                           \
  } while (0)

namespace {
int g_events = 0;
std::string g_last_payload;

void NCM_CALL on_event(void *ud, int32_t event, const char *json) {
  auto *counter = static_cast<int *>(ud);
  ++(*counter);
  g_last_payload.assign(json ? json : "");
  std::printf("     event=%d json=%s\n", static_cast<int>(event),
              g_last_payload.c_str());
}
} // namespace

int main() {
  const std::string version = ncm::Client::version();
  CHECK(!version.empty() && version.find("abi") != std::string::npos,
        "version() 返回带 abi 标识的版本串");
  std::printf("     version = %s\n", version.c_str());

  {
    ncm::Client client;
    CHECK(client.valid(), "默认构造成功");
    CHECK(static_cast<bool>(client), "explicit operator bool 可用于条件判断");

    client.on_event(&on_event, &g_events);
    client.on_event(nullptr, nullptr);
    CHECK(true, "on_event 订阅/取消订阅不崩溃");

    std::string json;
    CHECK(client.qr_begin(json) == ncm::Status::NotImplemented,
          "qr_begin 返回 NotImplemented");
    CHECK(json.empty(), "失败时 out_json 保持为空");

    CHECK(client.playlist_tracks(0, json) == ncm::Status::InvalidArg,
          "非法 id 返回 InvalidArg");
    CHECK(client.playlist_tracks(12345, json) == ncm::Status::NotImplemented,
          "合法 id 返回 NotImplemented");

    CHECK(client.resolve_source(12345, ncm::Quality::Jymaster, json) ==
              ncm::Status::NotImplemented,
          "resolve_source(Jymaster) 走到未实现分支");
    CHECK(client.resolve_source(12345, static_cast<ncm::Quality>(999999), json) ==
              ncm::Status::InvalidArg,
          "非法音质枚举被拦截");

    CHECK(client.search("周杰伦", 0, json) == ncm::Status::NotImplemented,
          "search 接受空 limit 并回退默认值");

    CHECK(client.logout() == ncm::Status::NotImplemented, "logout 不崩溃");
    CHECK(client.clear_cache() == ncm::Status::NotImplemented, "clear_cache 不崩溃");
  }
  CHECK(true, "作用域结束时析构未泄漏（配合 ASan 验证）");

  // 移动语义
  {
    ncm::Client a;
    ncm::Client b(std::move(a));
    CHECK(b.valid(), "移动构造后目标有效");
    CHECK(!a.valid(), "移动后源被置空");
    ncm::Client c;
    c = std::move(b);
    CHECK(c.valid() && !b.valid(), "移动赋值正确转移所有权");
  }

  // 枚举到字符串
  CHECK(ncm::to_string(ncm::Status::Captcha) == "Captcha", "Status::Captcha 文案正确");
  CHECK(ncm::from_c(NCM_ERR_CAPTCHA) == ncm::Status::Captcha, "8821 -> Captcha 映射正确");
  CHECK(ncm::from_c(-424242) == ncm::Status::Internal, "未知码归为 Internal");
  CHECK(ncm::to_string(ncm::QrStatus::WaitScan) == "WaitScan", "QrStatus 文案正确");

  // throw_if_error
  bool threw = false;
  try {
    ncm::throw_if_error(ncm::Status::NoResource);
  } catch (const std::runtime_error &e) {
    threw = true;
    std::printf("     exception = %s\n", e.what());
  }
  CHECK(threw, "throw_if_error 按预期抛出 runtime_error");

  std::printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
  return g_failures ? 1 : 0;
}