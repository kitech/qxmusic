// musiccom/storeutil：QueueStore/FavoriteStore 共用的小工具（Qt 无关）
//  - save() 的 .part 临时名按进程区分：多实例并发保存互不踩
//    （固定 ".part" 名下两实例交替 rename 会 ENOENT，表现为 save FAILED）
//  - 保存失败三步（fopen/fwrite/rename）带 errno 明细 stderr 日志
#ifndef MUSICCOM_STOREUTIL_H
#define MUSICCOM_STOREUTIL_H

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace musiccom {
namespace storeutil {

inline int thisPid()
{
#ifdef _WIN32
    return _getpid();
#else
    return (int)getpid();
#endif
}

/* 临时文件名：<path>.part.<pid>（进程独占，多实例无竞争） */
inline std::string uniquePartPath(const std::string& path)
{
    return path + ".part." + std::to_string((long long)thisPid());
}

/* 保存失败明细日志（stderr 直出；errno 文案一并打） */
inline void logSaveError(const char* store, const char* step,
                         const std::string& path)
{
    const int e = errno;
    std::fprintf(stderr, "[%s] save %s failed: %s errno=%d (%s)\n",
                 store, step, path.c_str(), e, std::strerror(e));
}

} // namespace storeutil
} // namespace musiccom

#endif // MUSICCOM_STOREUTIL_H
