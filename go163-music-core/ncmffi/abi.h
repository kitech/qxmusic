/*
 * abi.h - 仅供 cgo 内部使用，不随产物分发给 UI 侧。
 *
 * include/ncm.h 把 ncm_client 声明为不透明类型。这里补全它的布局，
 * 这样 Go 侧可以用 C.malloc 分配、用 uintptr_t 承载 cgo.Handle。
 *
 * 为什么句柄是 malloc 出来的 C 内存、里面装 uintptr_t：
 *   cgo 规则禁止 C 持有 Go 指针。若直接把 *client.Client 伪装成
 *   ncm_client* 返回给 C，C 一旦保存就构成非法引用。cgo.Handle 本身
 *   是 uintptr_t，不属于 Go 指针，放进 C 内存是合法的。
 */
#ifndef NCM_ABI_H
#define NCM_ABI_H

#include <stdlib.h>

#include "ncm.h"

/* "NCM1" —— 用于识别已释放或野指针的句柄，便于让 ncm_free 幂等。 */
#define NCM_CLIENT_MAGIC 0x4E434D31u

struct ncm_client {
  uintptr_t handle;
  uint32_t  magic;
};

/* 事件回调的统一入口，实现在 ffi.c。Go 侧只允许调用 C 头文件中声明过的
 * 函数，因此必须在此处显式声明。 */
void ncm_invoke_event(ncm_event_fn cb, void *user_data, int32_t event, const char *json);

#endif /* NCM_ABI_H */