#include "downloadservice.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QUrl>

DownloadService& DownloadService::instance()
{
    static DownloadService s;
    return s;
}

DownloadService::DownloadService(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QDir().mkpath(dirPath());
}

QString DownloadService::dirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/downloads");
}

QString DownloadService::filePath(qint64 id, const QString& suffix) const
{
    return dirPath() + QStringLiteral("/%1.%2").arg(id).arg(suffix);
}

bool DownloadService::isCached(qint64 id, const QString& suffix) const
{
    return QFile::exists(filePath(id, suffix));
}

static const char* const kCachedExts[] = { "mp3", "flac", "m4a", "aac" };

bool DownloadService::hasAny(qint64 id) const
{
    if (id <= 0) return false;
    for (const char* e : kCachedExts)
        if (isCached(id, QString::fromLatin1(e))) return true;
    return false;
}

QString DownloadService::anyCachedPath(qint64 id) const
{
    if (id <= 0) return QString();
    for (const char* e : kCachedExts) {
        const QString s = QString::fromLatin1(e);
        if (isCached(id, s)) return filePath(id, s);
    }
    return QString();
}

void DownloadService::enqueue(qint64 id, const QString& url, const QString& suffix,
                              const QMap<QString, QString>& headers)
{
    if (id <= 0 || url.isEmpty() || isCached(id, suffix))
        return;
    for (const auto& t : m_queue)
        if (t.id == id) return;
    if (m_activeId == id) return;

    Task task;
    task.id = id;
    task.url = url;
    task.suffix = suffix;
    task.headers = headers;
    m_queue.enqueue(task);
    if (m_activeId == 0)
        startNext();
}

void DownloadService::startNext()
{
    if (m_queue.isEmpty()) {
        m_activeId = 0;
        return;
    }
    const auto task = m_queue.dequeue();
    m_activeId = task.id;
    const QString url = task.url;
    const QString dst = filePath(task.id, task.suffix);

    QNetworkRequest req{ QUrl(url) };
    /* 下载头：NCM 与 B站统一由 resolve 出参给出（Referer/UA/Cookie…） */
    for (auto it = task.headers.constBegin(); it != task.headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    req.setTransferTimeout(60000);   /* 30s 无数据流动即中止 → finished 走错误路径 */
    auto* reply = m_nam->get(req);
    auto* f = new QFile(dst + QStringLiteral(".part"), this);
    const bool opened = f->open(QIODevice::WriteOnly | QIODevice::Truncate);
    Q_EMIT progress(m_activeId, -1);   // 未定态：播放条转圈

    connect(reply, &QNetworkReply::readyRead, this, [reply, f]() {
        if (f->isOpen()) f->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this](qint64 got, qint64 total) {
            if (total > 0)
                Q_EMIT progress(m_activeId, int(got * 100 / total));
        });
    connect(reply, &QNetworkReply::finished, this, [this, reply, f, dst, opened]() {
        reply->deleteLater();
        if (f->isOpen()) f->write(reply->readAll());
        f->close();
        /* 真·成功：文件真的打开、落盘、rename 成功、且非空；
         * 否则报 -2 并记下真实原因（不再"HTTP 无错就当成功"） */
        const bool httpOk = (reply->error() == QNetworkReply::NoError);
        QString fail;
        bool ok = false;
        if (!opened) {
            fail = QStringLiteral("open failed");
        } else if (!httpOk) {
            fail = reply->errorString();
        } else {
            QFile::remove(dst);
            if (!QFile::rename(dst + QStringLiteral(".part"), dst))
                fail = QStringLiteral("rename failed");
            else if (QFileInfo(dst).size() <= 0)
                fail = QStringLiteral("empty file");
            else
                ok = true;
        }
        if (!ok) {
            QFile::remove(dst + QStringLiteral(".part"));
            qWarning() << "[DownloadService] id=" << m_activeId << "fail:" << fail;
        }
        Q_EMIT progress(m_activeId, ok ? 100 : -2);
        f->deleteLater();
        m_activeId = 0;
        startNext();
    });
}
