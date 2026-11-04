#ifndef CXAUDIO_LOG_H
#define CXAUDIO_LOG_H

/* cxaudio 内建轻量日志（格式对齐主程序 ALOG_INFO）：
 *   [12:24:04] [INFO] - decode flowing: got=2048 chunk frames [pcm_backend.cpp:254]
 * 时间行首；[file:line] 行尾并用 ANSI 90 亮黑灰着色（对齐 ALOG 观感）；
 * isatty 门控——重定向落盘不带色码不污染日志；纯 stderr 零依赖 */
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <unistd.h>

#define CXA_SHORT_FILE() \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define CXA_LOG(...)                                                        \
    do {                                                                    \
        char tbuf_[16];                                                     \
        std::time_t tnow_ = std::time(0);                                   \
        std::strftime(tbuf_, sizeof(tbuf_), "%H:%M:%S",                     \
                      std::localtime(&tnow_));                              \
        fprintf(stderr, "[%s] [INFO] - ", tbuf_);                           \
        fprintf(stderr, __VA_ARGS__);                                       \
        if (isatty(fileno(stderr)))                                         \
            fprintf(stderr, " \033[90m[%s:%d]\033[0m\n",                    \
                    CXA_SHORT_FILE(), __LINE__);                            \
        else                                                                \
            fprintf(stderr, " [%s:%d]\n", CXA_SHORT_FILE(), __LINE__);      \
    } while (0)

#endif /* CXAUDIO_LOG_H */
