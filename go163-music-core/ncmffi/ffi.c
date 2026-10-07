/*
 * ffi.c - 与 include/ncm.h 分离的 C 实现。
 *
 * 为什么单独成文件：Go 侧含 //export 的文件，其 cgo preamble 会被复制进
 * 两份不同的 C 输出文件，因此 preamble 内不得出现任何函数或变量定义，
 * 只有声明。把实现放到本文件即可绕开该限制。
 */
#include "abi.h"

/* ABI 宽度自检。ncm.h 依赖 int32_t/int64_t 精确宽度，
 * 任何一处不符都应在编译期而不是运行期炸掉。 */
_Static_assert(sizeof(int32_t) == 4, "ncm requires 32-bit int32_t");
_Static_assert(sizeof(int64_t) == 8, "ncm requires 64-bit int64_t");
_Static_assert(sizeof(void *) == sizeof(uintptr_t),
               "pointer size and uintptr_t size must match");

/* 事件回调的统一入口。
 * Go 侧不直接调用 C 函数指针，改为经由本函数，好处：
 *   1. 这里能做 NULL 检查，避免向已取消订阅的回调发起调用（UB）；
 *   2. 签名在这里固定一次，避免 cgo 对函数指针类型的处理差异。
 * 注意：本函数运行在 Go 持有的 goroutine 上，回调内部不得同步重入本库。 */
void ncm_invoke_event(ncm_event_fn cb, void *user_data, int32_t event, const char *json) {
  if (cb == NULL) {
    return;
  }
  cb(user_data, event, json);
}