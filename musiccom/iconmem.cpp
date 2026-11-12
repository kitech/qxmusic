#include "iconmem.h"

#include <qmutex.h>

#if QT_VERSION < 0x040000
#define QX_ENTRY(it) (it).data()     // Qt3 QMapIterator
#else
#define QX_ENTRY(it) (it).value()
#endif

namespace qximage {
namespace {

QMutex& cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QString pxKey(const QString& url, int px)
{
    return url + QString(QChar(':')) + QString::number(px);
}

} // namespace

PixmapCache::PixmapCache()
    : m_bytes(0), m_maxBytes(16 * 1024 * 1024), m_tick(1)
{
}

PixmapCache& PixmapCache::instance()
{
    static PixmapCache s_instance;
    return s_instance;
}

QPixmap PixmapCache::get(const QString& url, int px)
{
    QMutexLocker locker(&cacheMutex());
    QMap<QString, Entry>::iterator it = m_entries.find(pxKey(url, px));
    if (it == m_entries.end())
        return QPixmap();
    QX_ENTRY(it).tick = ++m_tick;
    return QX_ENTRY(it).pixmap;
}

void PixmapCache::put(const QString& url, int px, const QPixmap& pixmap)
{
    if (pixmap.isNull())
        return;

    QMutexLocker locker(&cacheMutex());
    const QString key = pxKey(url, px);

    QMap<QString, Entry>::iterator old = m_entries.find(key);
    if (old != m_entries.end()) {
        m_bytes -= QX_ENTRY(old).bytes;
#if QT_VERSION >= 0x060000
        m_entries.erase(old);
#else
        m_entries.remove(old);
#endif
    }

    Entry entry;
    entry.pixmap = pixmap;
    entry.bytes = (long long)pixmap.width() * pixmap.height() * 4;
    entry.tick = ++m_tick;
    m_entries.insert(key, entry);
    m_bytes += entry.bytes;

    while (m_bytes > m_maxBytes && !m_entries.isEmpty()) {
        QMap<QString, Entry>::iterator victim = m_entries.begin();
        QMap<QString, Entry>::iterator it = m_entries.begin();
        for (; it != m_entries.end(); ++it) {
            if (QX_ENTRY(it).tick < QX_ENTRY(victim).tick)
                victim = it;
        }
        m_bytes -= QX_ENTRY(victim).bytes;
#if QT_VERSION >= 0x060000
        m_entries.erase(victim);
#else
        m_entries.remove(victim);
#endif
    }
}

void PixmapCache::clear()
{
    QMutexLocker locker(&cacheMutex());
    m_entries.clear();
    m_bytes = 0;
}

void PixmapCache::setMaxBytes(long long bytes)
{
    QMutexLocker locker(&cacheMutex());
    m_maxBytes = bytes;
}

} // namespace qximage
