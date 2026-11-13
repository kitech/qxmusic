#include "downloadservice.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
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

void DownloadService::enqueue(qint64 id, const QString& url, const QString& suffix)
{
    if (id <= 0 || url.isEmpty() || isCached(id, suffix))
        return;
    for (const auto& t : m_queue)
        if (t.first == id) return;
    if (m_activeId == id) return;

    m_queue.enqueue({ id, { url, suffix } });
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
    m_activeId = task.first;
    const QString url = task.second.first;
    const QString dst = filePath(task.first, task.second.second);

    QNetworkRequest req{ QUrl(url) };
    req.setTransferTimeout(30000);   /* 30s 无数据流动即中止 → finished 走错误路径 */
    auto* reply = m_nam->get(req);
    auto* f = new QFile(dst + QStringLiteral(".part"), this);
    f->open(QIODevice::WriteOnly | QIODevice::Truncate);
    Q_EMIT progress(m_activeId, -1);   // 未定态：播放条转圈

    connect(reply, &QNetworkReply::readyRead, this, [reply, f]() {
        if (f->isOpen()) f->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this](qint64 got, qint64 total) {
            if (total > 0)
                Q_EMIT progress(m_activeId, int(got * 100 / total));
        });
    connect(reply, &QNetworkReply::finished, this, [this, reply, f, dst]() {
        reply->deleteLater();
        if (f->isOpen()) f->write(reply->readAll());
        f->close();
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            QFile::remove(dst);
            QFile::rename(dst + QStringLiteral(".part"), dst);
        } else {
            QFile::remove(dst + QStringLiteral(".part"));
        }
        Q_EMIT progress(m_activeId, ok ? 100 : -2);
        f->deleteLater();
        m_activeId = 0;
        startNext();
    });
}
