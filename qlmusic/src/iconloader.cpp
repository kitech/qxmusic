#include "iconloader.h"
#include "iconcachecore.h"
#include "iconmem.h"
#include "identicon.h"      // 现有 identicon（qlcomp），保持原视觉

#include <qimage.h>
#include <qthread.h>
#include <qtimer.h>
#include <qmutex.h>
#include <qdatetime.h>
#include <qevent.h>
#include <qpainter.h>
#include <qbitmap.h>
#if QT_VERSION >= 0x040000
#include <qimagereader.h>
#include <qbuffer.h>
#endif
#ifdef QT3_BUILD
#include <qapplication.h>
#include <qglobal.h>        // Qt3: qDebug
#else
#include <qcoreapplication.h>
#include <qdebug.h>
#endif

#include <curl/curl.h>
#include <cstring>

namespace {

/* Qt3 无 QMap::value(key, def)：两端兼容的取值（GUI 线程专用，调用方已持语义） */
inline int qxMapGetInt(const QMap<QString, int>& m, const QString& k, int def)
{
#if QT_VERSION >= 0x040000
    return m.value(k, def);
#else
    QMap<QString, int>::ConstIterator it = m.find(k);
    return it != m.end() ? *it : def;
#endif
}

} // namespace

namespace {

const int kMaxConcurrent = 4;
const int kIconEventType = QEvent::User + 771;

int qxMax(int a, int b) { return a > b ? a : b; }

void iconLog(const QString& msg)
{
#ifdef QT3_BUILD
    qDebug(msg);
#elif QT_VERSION >= 0x050400
    qDebug().noquote() << msg;
#else
    qDebug() << msg;
#endif
}

void appendBytes(QByteArray& dst, const char* data, int size)
{
#ifdef QT3_BUILD
    const int old = (int)dst.size();
    dst.resize(old + size);
    memcpy(dst.data() + old, data, size);
#else
    dst.append(data, size);
#endif
}

#ifdef QT3_BUILD
class IconEvent : public QCustomEvent
{
public:
    explicit IconEvent(const QString& u, bool ok)
        : QCustomEvent(kIconEventType), url(u), success(ok) {}
    QString url;
    bool success;
};
#else
class IconEvent : public QEvent
{
public:
    explicit IconEvent(const QString& u, bool ok)
        : QEvent(QEvent::Type(kIconEventType)), url(u), success(ok) {}
    QString url;
    bool success;
};
#endif

/* 读取 → 等比放大到覆盖 px → 居中裁剪 */
QImage decodeScaled(const QByteArray& bytes, int px)
{
    QImage img;
    if (bytes.isEmpty() || px <= 0)
        return QImage();
    if (!img.loadFromData(bytes) || img.isNull())
        return QImage();

    const int w = img.width();
    const int h = img.height();
    if (w < px && h < px)
        return img;

    int tw, th;
    if (w >= h) {
        th = px;
        tw = qxMax(px, w * px / qxMax(1, h));
    } else {
        tw = px;
        th = qxMax(px, h * px / qxMax(1, w));
    }

#ifdef QT3_BUILD
    QImage scaled = img.smoothScale(tw, th);
#else
    QImage scaled = img.scaled(tw, th, Qt::IgnoreAspectRatio,
                               Qt::SmoothTransformation);
#endif

    if (scaled.width() > px || scaled.height() > px) {
        const int x = (scaled.width() - px) / 2;
        const int y = (scaled.height() - px) / 2;
        scaled = scaled.copy(x, y, px, px);
    }
    return scaled;
}

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    QByteArray* out = static_cast<QByteArray*>(userdata);
    appendBytes(*out, ptr, int(size * nmemb));
    return size * nmemb;
}

QByteArray httpGet(const QString& url)
{
    QByteArray data;
    CURL* curl = curl_easy_init();
    if (!curl)
        return data;

#ifdef QT3_BUILD
    const QByteArray target = url.utf8();
#else
    const QByteArray target = url.toUtf8();
#endif

#ifdef QT3_BUILD
    curl_easy_setopt(curl, CURLOPT_URL, target.data());
#else
    curl_easy_setopt(curl, CURLOPT_URL, target.constData());
#endif
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36 Edg/124.0.0.0");
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://music.163.com");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || status < 200 || status >= 300)
        return QByteArray();
    return data;
}

class FetchThread : public QThread
{
public:
    explicit FetchThread(const QString& url) : m_url(url), m_ok(false) {}

    void run() override
    {
        QByteArray bytes = httpGet(m_url);
        if (bytes.isEmpty()) {
            iconLog(QString::fromLatin1("[ICON] retry url=") + m_url);
            QThread::msleep(1000);        // 弱网重试一次
            bytes = httpGet(m_url);
        }
        m_ok = !bytes.isEmpty();
        if (m_ok) {
            if (!qximage::write(m_url, bytes))
                iconLog(QString::fromLatin1("[ICON] cache-write-failed url=") + m_url);
            iconLog(QString::fromLatin1("[ICON] hit=net url=") + m_url);
        } else {
            qximage::negativePut(m_url);
            iconLog(QString::fromLatin1("[ICON] miss url=") + m_url);
        }
        IconLoader::instance().postDone(m_url, m_ok);
    }

private:
    QString m_url;
    bool m_ok;
};

QPixmap pixmapFromImage(const QImage& img)
{
#ifdef QT3_BUILD
    QPixmap pm;
    pm.convertFromImage(img);
    return pm;
#else
    return QPixmap::fromImage(img);
#endif
}

} // namespace

IconLoader& IconLoader::instance()
{
    static IconLoader s_instance;
    return s_instance;
}

IconLoader::IconLoader(QObject* parent)
    : QObject(parent), m_active(0), m_done(0)
{
    /* 桌面：16MB 内存 LRU（与 qskmusic 一致） */
    qximage::PixmapCache::instance().setMaxBytes(16LL * 1024 * 1024);
#if QT_VERSION >= 0x040000
    /* 启动期格式能力清单：缺 jpeg 插件一眼可见（Qt4 部署经典坑） */
    qDebug() << "[ICON] decodable formats:"
             << QImageReader::supportedImageFormats();
#endif
}

QPixmap IconLoader::cover(const QString& url, const QString& seed, int px)
{
    const QString raw = qximage::normalizedUrl(url);
    if (!raw.isEmpty()) {
        QPixmap pm = qximage::PixmapCache::instance().get(raw, px);
        if (!pm.isNull())
            return pm;

        const QByteArray bytes = qximage::read(raw);
        if (!bytes.isEmpty()) {
            const QImage img = decodeScaled(bytes, px);
            if (!img.isNull()) {
                pm = pixmapFromImage(img);
                qximage::PixmapCache::instance().put(raw, px, pm);
                return pm;
            }
            /* 损坏/非图片：删掉 + 短期负缓存，避免每次重绘重复解码。
             * 分层诊断：size/head=文件层证据；head 合法（FFD8 等）仍失败
             * → Qt 插件层缺格式（如 mac Qt4 未部署 libqjpeg） */
            QString head;
            for (int i = 0; i < (8 < (int)bytes.size() ? 8 : (int)bytes.size()); ++i) {
                const QString h = QString::number((int)(unsigned char)bytes[i], 16);
                head += h.length() < 2 ? QString(QChar('0')) + h : h;
            }
#if QT_VERSION >= 0x040000
            {
                QBuffer buf;
                buf.setData(bytes);
                buf.open(QIODevice::ReadOnly);
                QImageReader r(&buf);
                r.read();
                iconLog(QString::fromLatin1(
                            "[ICON] corrupt-on-disk url=%1 size=%2 head=%3 err=%4")
                            .arg(raw).arg((int)bytes.size()).arg(head)
                            .arg(r.errorString()));
            }
#else
            iconLog(QString::fromLatin1("[ICON] corrupt-on-disk url=%1 size=%2 head=%3")
                        .arg(raw).arg((int)bytes.size()).arg(head));
#endif
            qximage::remove(raw);
            qximage::negativePut(raw);
        }

        /* 负缓存过期后也不再无限"每 5 分钟一轮"：会话内每 URL 最多 2 轮失败 */
        if (!qximage::negativeHit(raw) && qxMapGetInt(m_fails, raw, 0) < 2)
            requestDownload(raw);
    }
    return generated(seed, px);
}

QPixmap IconLoader::generated(const QString& seed, int px)
{
    const QString key = QString::fromLatin1("gen:") + seed;
    QPixmap pm = qximage::PixmapCache::instance().get(key, px);
    if (!pm.isNull())
        return pm;
    pm = generateIdenticon(seed, px);
    qximage::PixmapCache::instance().put(key, px, pm);
    return pm;
}

void IconLoader::requestDownload(const QString& url)
{
    QMutexLocker locker(&m_mutex);
    if (m_inFlight.contains(url) || m_queue.contains(url))
        return;
    if (m_active >= kMaxConcurrent) {
        m_queue.append(url);
        return;
    }
    startFetch(url);
}

void IconLoader::startFetch(const QString& url)
{
    m_inFlight.insert(url, 1);
    ++m_active;

    FetchThread* thread = new FetchThread(url);
    m_threads.insert(url, thread);
    thread->start();
}

void IconLoader::postDone(const QString& url, bool ok)
{
#ifdef QT3_BUILD
    QApplication::postEvent(this, new IconEvent(url, ok));
#else
    QCoreApplication::postEvent(this, new IconEvent(url, ok));
#endif
}

#ifdef QT3_BUILD
void IconLoader::customEvent(QCustomEvent* event)
#else
void IconLoader::customEvent(QEvent* event)
#endif
{
    if (event->type() != kIconEventType) {
#ifdef QT3_BUILD
        QObject::customEvent(event);
#else
        QObject::customEvent(event);
#endif
        return;
    }

    IconEvent* iconEvent = static_cast<IconEvent*>(event);
    handleDone(iconEvent->url, iconEvent->success);
}

void IconLoader::handleDone(const QString& url, bool ok)
{
    QString next;
    {
        QMutexLocker locker(&m_mutex);
        m_inFlight.remove(url);
        if (m_active > 0)
            --m_active;
        if (!m_queue.isEmpty()) {
#if QT_VERSION >= 0x040000
            next = m_queue.takeFirst();
#else
            next = m_queue.first();
            m_queue.remove(m_queue.begin());
#endif
        }
        if (!next.isEmpty())
            startFetch(next);
    }

    // 回收已完成线程（postDone 后 run 立即返回，wait 仅极短阻塞）
    QThread* thread = 0;
    {
        QMap<QString, QThread*>::iterator it = m_threads.find(url);
        if (it != m_threads.end()) {
#if QT_VERSION >= 0x040000
            thread = it.value();
#else
            thread = it.data();
#endif
#if QT_VERSION >= 0x040000
            m_threads.erase(it);
#else
            m_threads.remove(it);
#endif
        }
    }
    if (thread) {
        thread->wait(5000);
        delete thread;
    }

    if (++m_done % 50 == 0)
        qximage::trim(400LL * 1024 * 1024);   // 长会话定期回收

    if (ok)
        m_fails.remove(url);           // 成功清除失败计数
    else
        m_fails.insert(url, qxMapGetInt(m_fails, url, 0) + 1);

    emit iconsReady();
}

QPixmap IconLoader::rounded(const QPixmap& src, int px, int radius)
{
    if (src.isNull() || radius <= 0 || px <= 0)
        return src;
    QPixmap pm = src;
    QPixmap mask(px, px);
    mask.fill(Qt::black);
    QPainter m(&mask);
#if QT_VERSION >= 0x040000
    m.setRenderHint(QPainter::Antialiasing);
#endif
    m.setBrush(Qt::white);
    m.setPen(Qt::NoPen);
#if QT_VERSION >= 0x040000
    m.drawRoundedRect(0, 0, px, px, radius, radius);
#else
    m.drawRoundRect(0, 0, px, px, radius, radius);   // Qt3 同义（整型半径）
#endif
    m.end();
    pm.setMask(mask.createHeuristicMask(false));
    return pm;
}

void IconLoader::trimCache()
{
    QTimer::singleShot(3000, this, SLOT(trimLater()));
}

void IconLoader::trimLater()
{
    qximage::trim(400LL * 1024 * 1024);
    iconLog(QString::fromLatin1("[ICON] cache files=%1 bytes=%2")
                .arg(qximage::fileCount()).arg((int)qximage::totalBytes()));
}
