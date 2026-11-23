#ifndef DOWNLOAD_SERVICE_H
#define DOWNLOAD_SERVICE_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMap>

class QNetworkAccessManager;

/*
 * 下载队列（qlmusic NcmTaskFetch 的 qsk 简化版）：
 *   串行下载到 AppLocalData/downloads/<id>.<suffix>，.part 落盘后 rename；
 *   同一 id 在途/已缓存不重复入队；进度节流由 QNetworkReply 自带。
 * 触发：① 播放 resolve 成功后自动缓存 ② 曲目行长按菜单「下载」。
 */
class DownloadService : public QObject
{
    Q_OBJECT
public:
    static DownloadService& instance();

    /* headers：逐条 set（Referer/UA/Cookie…）；NCM 与 B站同一条路径 */
    void enqueue(qint64 songId, const QString& url, const QString& suffix,
                 const QMap<QString, QString>& headers = {});
    bool isCached(qint64 songId, const QString& suffix) const;
    QString filePath(qint64 songId, const QString& suffix) const;
    /* 离线播放：任音质命中即算已下载 */
    bool hasAny(qint64 songId) const;
    QString anyCachedPath(qint64 songId) const;

Q_SIGNALS:
    void progress(qint64 songId, int pct);   // -1 未定；0-100；100 完成；-2 失败

private:
    explicit DownloadService(QObject* parent = nullptr);

    void startNext();
    static QString dirPath();

    QNetworkAccessManager* m_nam = nullptr;
    struct Task { qint64 id; QString url; QString suffix; QMap<QString, QString> headers; };
    QQueue<Task> m_queue;
    qint64 m_activeId = 0;
};

#endif
