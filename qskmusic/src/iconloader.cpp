#include "iconloader.h"
#include "iconcachecore.h"
#include "iconmem.h"
#include "logmodel.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QImageReader>
#include <QBuffer>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <jni.h>
#endif

namespace {

const int kMaxConcurrent = 4;

QString keyOf(const QString& url, int px)
{
    return url + QLatin1Char(':') + QString::number(px);
}

/* 读取 → 按 px 等比放大到覆盖 → 居中裁剪为正方形 */
QImage readScaledCrop(const QByteArray& bytes, int px)
{
    if (bytes.isEmpty() || px <= 0)
        return QImage();

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly))
        return QImage();

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);

    const QSize orig = reader.size();
    if (orig.isValid() && !orig.isEmpty())
        reader.setScaledSize(orig.scaled(px, px, Qt::KeepAspectRatioByExpanding));

    QImage image = reader.read();
    if (image.isNull())
        return QImage();

    image = image.scaled(px, px, Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);
    if (image.width() > px || image.height() > px) {
        const int x = (image.width() - px) / 2;
        const int y = (image.height() - px) / 2;
        image = image.copy(x, y, px, px);
    }
    return image;
}

class DecodeTask : public QRunnable
{
public:
    DecodeTask(IconLoader* loader, const QString& url, int px,
               const QByteArray& bytes)
        : m_loader(loader), m_url(url), m_px(px), m_bytes(bytes) {}

    void run() override
    {
        const QImage image = readScaledCrop(m_bytes, m_px);
        IconLoader* loader = m_loader;
        const QString url = m_url;
        const int px = m_px;
        QMetaObject::invokeMethod(loader, [loader, url, px, image]() {
            loader->deliver(url, px, image);
        }, Qt::QueuedConnection);
    }

private:
    IconLoader* m_loader;
    QString m_url;
    int m_px;
    QByteArray m_bytes;
};

#ifdef Q_OS_ANDROID
/* Android：Qt 未捆绑 OpenSSL，走系统 HttpURLConnection（Java） */
QByteArray fetchViaJava(const QString& url)
{
    QJniObject jurl = QJniObject::fromString(url);
    QJniObject result = QJniObject::callStaticObjectMethod(
        "io/fedlet/mobutil/ImageFetch",
        "fetch",
        "(Ljava/lang/String;)[B",
        jurl.object());

    if (!result.isValid())
        return QByteArray();

    QJniEnvironment env;
    jbyteArray array = static_cast<jbyteArray>(result.object());
    if (!array)
        return QByteArray();

    QByteArray bytes;
    const jsize len = env->GetArrayLength(array);
    jbyte* data = env->GetByteArrayElements(array, nullptr);
    if (data && len > 0)
        bytes = QByteArray(reinterpret_cast<char*>(data), int(len));
    if (data)
        env->ReleaseByteArrayElements(array, data, JNI_ABORT);
    return bytes;
}

class AndroidFetchTask : public QRunnable
{
public:
    AndroidFetchTask(IconLoader* loader, const QString& url, int px)
        : m_loader(loader), m_url(url), m_px(px) {}

    void run() override
    {
        const QByteArray bytes = fetchViaJava(m_url);
        IconLoader* loader = m_loader;
        const QString url = m_url;
        const int px = m_px;
        QMetaObject::invokeMethod(loader, [loader, url, px, bytes]() {
            loader->finishFetch(url, px, bytes);
        }, Qt::QueuedConnection);
    }

private:
    IconLoader* m_loader;
    QString m_url;
    int m_px;
};
#endif

} // namespace

IconLoader& IconLoader::instance()
{
    static IconLoader s_instance;
    return s_instance;
}

IconLoader::IconLoader(QObject* parent)
    : QObject(parent)
{
#ifndef Q_OS_ANDROID
    m_nam = new QNetworkAccessManager(this);
#endif
    if (QThreadPool::globalInstance()->maxThreadCount() < 4)
        QThreadPool::globalInstance()->setMaxThreadCount(4);
}

void IconLoader::log(const QString& level, const QString& message)
{
    qDebug().noquote() << "[ICON]" << message;
    LogModel::Level lv = LogModel::Info;
    if (level == QLatin1String("warn"))
        lv = LogModel::Warn;
    LogModel::instance().append(lv, QStringLiteral("icon"), message);
}

QPixmap IconLoader::cached(const QString& url, int px)
{
    const QString raw = qximage::normalizedUrl(url);
    if (raw.isEmpty() || px <= 0)
        return QPixmap();

    QPixmap pixmap = qximage::PixmapCache::instance().get(raw, px);
    if (!pixmap.isNull())
        return pixmap;

    const QByteArray bytes = qximage::read(raw);
    if (bytes.isEmpty())
        return QPixmap();

    const QImage image = readScaledCrop(bytes, px);
    if (image.isNull()) {
        // 损坏/非图片文件：删掉并短期负缓存，避免永久兜底 + 每次重试解码
        qximage::remove(raw);
        qximage::negativePut(raw);
        log(QStringLiteral("warn"),
            QStringLiteral("corrupt-on-disk url=%1 px=%2").arg(raw).arg(px));
        return QPixmap();
    }

    pixmap = QPixmap::fromImage(image);
    qximage::PixmapCache::instance().put(raw, px, pixmap);
    log(QStringLiteral("info"),
        QStringLiteral("hit=disk url=%1 px=%2").arg(raw).arg(px));
    return pixmap;
}

void IconLoader::request(const QString& url, int px)
{
    const QString raw = qximage::normalizedUrl(url);
    if (raw.isEmpty() || px <= 0)
        return;

    if (!cached(raw, px).isNull()) {
        QMetaObject::invokeMethod(this, [this, raw, px]() {
            Q_EMIT iconReady(raw, px);
        }, Qt::QueuedConnection);
        return;
    }

    if (qximage::negativeHit(raw))
        return;

    const QString key = keyOf(raw, px);
    if (m_inFlight.contains(key))
        return;

    for (const Pending& p : m_queue) {
        if (keyOf(p.url, p.px) == key)
            return;
    }

    Pending pending;
    pending.url = raw;
    pending.px = px;
    m_queue.append(pending);
    pump();
}

void IconLoader::pump()
{
    while (m_active < kMaxConcurrent && !m_queue.isEmpty()) {
        const Pending p = m_queue.takeFirst();
        startDownload(p.url, p.px);
    }
}

void IconLoader::startDownload(const QString& url, int px)
{
    const QString key = keyOf(url, px);
    m_inFlight.insert(key);
    ++m_active;

#ifdef Q_OS_ANDROID
    QThreadPool::globalInstance()->start(new AndroidFetchTask(this, url, px));
#else
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Referer", "https://music.163.com");
    req.setRawHeader("User-Agent", "qskmusic/0.1");
    req.setTransferTimeout(15000);          // 15s 无响应即中止，避免占满并发槽
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, px]() {
        reply->deleteLater();
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray bytes;
        if (reply->error() == QNetworkReply::NoError && status >= 200
            && status < 300)
            bytes = reply->readAll();
        if (bytes.isEmpty())
            log(QStringLiteral("warn"),
                QStringLiteral("net-fail url=%1 status=%2 err=%3")
                    .arg(url).arg(status).arg(reply->errorString()));
        finishFetch(url, px, bytes);
    });
#endif
}

void IconLoader::finishFetch(const QString& url, int px, const QByteArray& bytes)
{
    m_inFlight.remove(keyOf(url, px));
    if (m_active > 0)
        --m_active;

    if (!bytes.isEmpty()) {
        if (!qximage::write(url, bytes)) {
            log(QStringLiteral("error"),
                QStringLiteral("cache-write-failed url=%1").arg(url));
        }
        ++m_netWrites;
        if (m_netWrites % 50 == 0)
            trimCache();                    // 长会话也不再无限增长
        log(QStringLiteral("info"),
            QStringLiteral("hit=net url=%1 px=%2").arg(url).arg(px));
        QThreadPool::globalInstance()->start(new DecodeTask(this, url, px, bytes));
    } else {
        const int tries = m_retry.value(url, 0);
        if (tries < 1) {
            m_retry.insert(url, tries + 1);
            QTimer::singleShot(1000, this, [this, url, px]() {
                Pending retry;
                retry.url = url;
                retry.px = px;
                m_queue.append(retry);
                pump();
            });
            log(QStringLiteral("warn"),
                QStringLiteral("retry url=%1").arg(url));
        } else {
            qximage::negativePut(url);
            log(QStringLiteral("warn"),
                QStringLiteral("miss url=%1 (negative cached)").arg(url));
        }
    }
    pump();
}

void IconLoader::deliver(const QString& url, int px, const QImage& image)
{
    if (image.isNull()) {
        qximage::remove(url);
        qximage::negativePut(url);
        log(QStringLiteral("error"),
            QStringLiteral("decode-failed url=%1 px=%2").arg(url).arg(px));
        return;
    }
    const QPixmap pixmap = QPixmap::fromImage(image);
    qximage::PixmapCache::instance().put(url, px, pixmap);
    Q_EMIT iconReady(url, px);
    log(QStringLiteral("info"),
        QStringLiteral("hit=mem url=%1 px=%2 (decoded)").arg(url).arg(px));
}

void IconLoader::trimCache()
{
    QTimer::singleShot(3000, this, []() {
#ifdef Q_OS_ANDROID
        const long long limit = 200LL * 1024 * 1024;
#else
        const long long limit = 400LL * 1024 * 1024;
#endif
        qximage::trim(limit);
        qDebug() << "[ICON] cache files=" << qximage::fileCount()
                 << "bytes=" << qximage::totalBytes();
    });
}
