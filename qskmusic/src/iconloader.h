#ifndef ICON_LOADER_H
#define ICON_LOADER_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QSet>
#include <QHash>
#include <QString>
#include <QByteArray>
#include <QImage>

class QNetworkAccessManager;

/*
 * 封面加载器（qskmusic）：
 *   命中顺序 内存 QPixmap LRU → 磁盘（musiccom/qximage）→ 网络（QNetworkAccessManager）
 *   只缓存原图；按目标 px 在工作线程缩放裁剪后回 GUI 线程。
 *   失败：负缓存（musiccom，内存 5 分钟）+ 重试 1 次；调用方保留兜底图标。
 */
class IconLoader : public QObject
{
    Q_OBJECT
public:
    static IconLoader& instance();

    QPixmap cached(const QString& url, int px);   // 同步命中（内存/磁盘）
    void request(const QString& url, int px);     // 异步拉取；命中立即 iconReady
    void trimCache();                             // 启动后调用（单目录 LRU）

    /* 解码任务回投（QThreadPool 工作线程 → GUI 线程） */
    void deliver(const QString& url, int px, const QImage& image);

    /* 下载完成回投（Android Java 线程 / QNetworkReply → GUI 线程） */
    void finishFetch(const QString& url, int px, const QByteArray& bytes,
                     const QString& contentType = QString());

Q_SIGNALS:
    void iconReady(const QString& url, int px);

private:
    explicit IconLoader(QObject* parent = nullptr);

    void startDownload(const QString& url, int px);
    void pump();
    void log(const QString& level, const QString& message,
             bool toLogModel = true);   // false = 仅 qDebug（高频命中降噪）

    struct Pending {
        QString url;
        int px;
    };

    QNetworkAccessManager* m_nam = nullptr;
    QList<Pending> m_queue;
    QSet<QString> m_inFlight;
    QHash<QString, int> m_retry;
    int m_active = 0;
    int m_netWrites = 0;      // 成功落盘计数 → 每 50 张触发一次 trim
};

#endif // ICON_LOADER_H
