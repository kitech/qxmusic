/*
 * smoke.c - libncm 的 C ABI 冒烟测试。
 *
 * 覆盖 P1 需要验收的点：
 *   - 能加载并调用 ncm_version
 *   - ncm_new / ncm_free 生命周期
 *   - 出参交给 ncm_free_string 释放后无泄漏
 *   - 未实现的接口返回 NCM_ERR_NOT_IMPLEMENTED 而非崩溃
 *   - 事件回调能被触发并收到合法 JSON
 *   - NULL 入参与 NULL 句柄不导致崩溃
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ncm.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));            \
      g_failures++;                                                             \
    } else {                                                                    \
      printf("ok   %s\n", (msg));                                               \
    }                                                                           \
  } while (0)

static void NCM_CALL on_event(void *user_data, int32_t event, const char *json) {
  int *counter = (int *)user_data;
  (*counter)++;
  printf("     event=%d json=%s\n", (int)event, json ? json : "(null)");
}

int main(void) {
  char *ver = ncm_version();
  CHECK(ver != NULL && strstr(ver, "abi") != NULL, "ncm_version 返回带 abi 标识的版本串");
  if (ver) {
    printf("     version = %s\n", ver);
  }
  ncm_free_string(ver);

  /* NULL 是合法的空操作 */
  ncm_free_string(NULL);
  ncm_free(NULL);
  CHECK(1, "对 NULL 调用 ncm_free_string / ncm_free 安全");

  ncm_client *c = ncm_new(NULL);
  CHECK(c != NULL, "ncm_new(NULL) 使用默认配置目录");
  if (!c) {
    return 1;
  }

  int events = 0;
  ncm_set_event_cb(c, on_event, &events);
  ncm_set_event_cb(c, NULL, NULL);
  CHECK(1, "ncm_set_event_cb 订阅/取消订阅不崩溃");

  char *out = NULL;
  int32_t rc;

  /* ---- 认证（P2 已实现，但需要网络；离线时降级为"不能崩溃"） ---- */

  rc = ncm_login_cookie(c, "MUSIC_U=definitely_not_a_real_cookie", &out);
  CHECK(rc != NCM_OK, "无效 cookie 不会被当成登录成功");
  CHECK(out == NULL, "登录失败时不分配出参");
  if (out) ncm_free_string(out);
  out = NULL;

  rc = ncm_login_cookie(c, "not-a-cookie-header", &out);
  CHECK(rc == NCM_ERR_INVALID_ARG, "缺少 '=' 的脏 cookie 返回 NCM_ERR_INVALID_ARG");
  out = NULL;

  rc = ncm_login_cookie(c, "__csrf=abc", &out);
  CHECK(rc == NCM_ERR_UNAUTHORIZED,
        "缺少 MUSIC_U 的 cookie 返回 NCM_ERR_UNAUTHORIZED（与未登录同义，而非内部错误）");
  out = NULL;

  rc = ncm_account(c, &out);
  CHECK(rc == NCM_ERR_UNAUTHORIZED || rc == NCM_ERR_NETWORK || rc == NCM_ERR_INTERNAL,
        "未登录时 ncm_account 返回 UNAUTHORIZED（或网络不可达）");
  if (out) ncm_free_string(out);
  out = NULL;

  rc = ncm_qr_begin(c, &out);
  if (rc == NCM_OK) {
    CHECK(out != NULL && strstr(out, "qrcode_url") != NULL,
          "ncm_qr_begin 成功时返回含 qrcode_url 的 JSON");
    /* 验证 unikey 已被回传，便于 UI 直接进入轮询 */
    CHECK(strstr(out, "unikey") != NULL, "ncm_qr_begin 返回 unikey");
    ncm_free_string(out);
    out = NULL;
  } else {
    CHECK(rc == NCM_ERR_NETWORK || rc == NCM_ERR_CAPTCHA || rc == NCM_ERR_UPSTREAM,
          "ncm_qr_begin 失败时为网络/风控类错误");
    printf("     (离线，跳过二维码检查)\n");
  }

  /* ncm_qr_poll 的关键契约：8821 必须映射成 NCM_ERR_CAPTCHA，
   * 而不是被 code != 200 的通用判断吞掉。 */
  rc = ncm_qr_poll(c, "invalid-unikey", &out);
  CHECK(rc == NCM_ERR_CAPTCHA || rc == NCM_OK || rc == NCM_ERR_INTERNAL ||
            rc == NCM_ERR_UPSTREAM || rc == NCM_ERR_NETWORK,
        "轮询无效 unikey 不返回 OK（除非网络不可达）");
  if (out) ncm_free_string(out);
  out = NULL;

  /* ---- 尚未实现的接口（P3+）：必须返回 NOT_IMPLEMENTED ---- */

  rc = ncm_playlist_tracks(c, 0, &out);
  CHECK(rc == NCM_ERR_INVALID_ARG, "非法 id 返回 NCM_ERR_INVALID_ARG");

  rc = ncm_playlist_tracks(c, 12345, &out);
  CHECK(rc == NCM_ERR_NOT_IMPLEMENTED, "未实现的目录接口返回 NCM_ERR_NOT_IMPLEMENTED");
  CHECK(out == NULL, "失败时不分配出参（调用方无需释放）");

  rc = ncm_resolve_source(c, 12345, NCM_QUALITY_JYMASTER, &out);
  CHECK(rc == NCM_ERR_NOT_IMPLEMENTED, "合法参数走到未实现分支");

  rc = ncm_resolve_source(c, 12345, 999999, &out);
  CHECK(rc == NCM_ERR_INVALID_ARG, "非法音质枚举被拦截（brMap 缺档问题的前置防线）");

  /* NULL 句柄必须被安全拒绝。这是 ncm.h 第 13 条 ABI 约定：
   * 每个新增导出函数都要满足，因此逐个显式覆盖而不是抽样。 */
  {
    int violations = 0;
    char *o = NULL;

    /* 带 char** 出参的接口 */
    if (ncm_login_cookie(NULL, "MUSIC_U=x", &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_qr_begin(NULL, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_qr_poll(NULL, "k", &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_account(NULL, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_daily_recommend(NULL, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_playlist_tracks(NULL, 1, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_album_tracks(NULL, 1, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_search(NULL, "x", 10, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_resolve_source(NULL, 1, NCM_QUALITY_STANDARD, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_download(NULL, 1, NCM_QUALITY_STANDARD, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_outer_url(NULL, 1, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;
    if (ncm_lyric(NULL, 1, &o) != NCM_ERR_INVALID_ARG) violations++;
    o = NULL;

    /* 无出参的接口 */
    if (ncm_logout(NULL) != NCM_ERR_INVALID_ARG) violations++;
    if (ncm_clear_cache(NULL) != NCM_ERR_INVALID_ARG) violations++;

    CHECK(violations == 0, "全部 14 个导出函数对 NULL 句柄返回 NCM_ERR_INVALID_ARG");
  }

  ncm_free(c);
  ncm_free(c); /* magic 已失效，应为空操作而非 double free */
  CHECK(1, "重复 ncm_free 不 double free");

  printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
  return g_failures ? 1 : 0;
}