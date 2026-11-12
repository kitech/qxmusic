#ifndef MUSICCOM_ICONMEM_H
#define MUSICCOM_ICONMEM_H

#include <qstring.h>
#include <qpixmap.h>
#include <qmap.h>

/*
 * 内存 QPixmap LRU（双版本共享；Qt3/4/6 通用）。
 * key = URL + ":" + px：磁盘缓存原图，内存缓存已缩放结果。
 */
namespace qximage {

class PixmapCache
{
public:
    static PixmapCache& instance();

    QPixmap get(const QString& url, int px);          // 未命中返回空 QPixmap
    void put(const QString& url, int px, const QPixmap& pixmap);
    void clear();

    void setMaxBytes(long long bytes);

private:
    PixmapCache();

    struct Entry {
        QPixmap pixmap;
        long long bytes;
        unsigned long long tick;
    };

    QMap<QString, Entry> m_entries;
    long long m_bytes;
    long long m_maxBytes;
    unsigned long long m_tick;
};

} // namespace qximage

#endif // MUSICCOM_ICONMEM_H
