#ifndef MUSICCOM_RADIOLIST_H
#define MUSICCOM_RADIOLIST_H

/*
 * 网络电台内置清单（P3 直播流）：固定数据，无请求、无认证。
 * - 地区 key 固定四类，导航按地区分类显示；
 * - hls=true 的条目需要 HLS(m3u8) 支持，播放器不支持时按不可播灰态处理；
 * - 清单随流地址漂移可能失效，属预期（电台源特性）。
 * Qt3/4/6 通用：仅用 QString/std::vector，无 UI 依赖。
 */

#include <qstring.h>
#include <vector>

struct RadioStation
{
    const char* region;   /* "china" / "hongkong" / "asia" / "international" */
    const char* name;     /* 显示名（UTF-8） */
    const char* url;      /* 直播流地址 */
    bool        hls;      /* true = .m3u8 (HLS)，false = 直连 mp3/aac */
};

namespace radiolist {

/* 地区 key 集合（固定顺序 = 显示顺序） */
const char* const* regions(int& count);

/* 地区 i18n key，如 "radio.region.cn" */
QString regionTitleKey(const QString& regionKey);

/* 全量清单 */
const std::vector<RadioStation>& stations();

/* 按地区过滤（regionKey 为空 = 全部） */
std::vector<RadioStation> byRegion(const QString& regionKey);

} // namespace radiolist

#endif
