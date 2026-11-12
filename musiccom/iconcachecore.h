#ifndef MUSICCOM_ICONCACHECORE_H
#define MUSICCOM_ICONCACHECORE_H

#include <qstring.h>
#ifdef QT3_BUILD
#include <qcstring.h>      // Qt3：QByteArray 定义在 qcstring.h
#else
#include <qbytearray.h>
#endif

/*
 * 双版本（qskmusic/qlmusic）共享的封面磁盘缓存核心（Qt3/4/6 通用，纯 QtCore）。
 * 协议见 docs/图片缓存协议.md：
 *   目录 <cache_root>/qxmusic/covers，扁平存储；
 *   文件名 = 封面 URL 最后一段（原始文件名），如 109951163265698885.jpg；
 *   只缓存原图（剥离 ?param=），显示时按目标尺寸缩放。
 */
namespace qximage {

QString cacheDir();                         // 解析目录并确保存在
QString normalizedUrl(const QString& url);  // 去掉 query/fragment 与尾斜杠
QString fileNameForUrl(const QString& url); // 原始文件名（非法字符过滤）；空=不可缓存
QString pathForUrl(const QString& url);

bool contains(const QString& url);
QByteArray read(const QString& url);                    // 未命中返回空
bool write(const QString& url, const QByteArray& data); // 原子写，不覆盖已有文件

QString extensionForData(const QByteArray& data);       // 魔数判扩展名（诊断用）
void trim(long long maxBytes);                          // 单目录容量上限（mtime LRU）

bool negativeHit(const QString& url);                   // 失败负缓存（仅内存 5 分钟）
void negativePut(const QString& url);

int fileCount();
long long totalBytes();

} // namespace qximage

#endif // MUSICCOM_ICONCACHECORE_H
