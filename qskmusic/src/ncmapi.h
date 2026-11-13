#ifndef NCM_API_H
#define NCM_API_H

#include <QObject>
#include <QString>
#include <QList>

#include "ncm.hpp"

// 播放音质（NCM_QUALITY_EXHIGH）
constexpr int NCM_Q_EXHIGH = 320000;

struct Song
{
    qint64 id = 0;
    QString name;
    QString artists;
    QList<qint64> artistIds;   // 与 artists 同序（ar[].id）
    QString album;
    QString cover;      // 专辑封面 URL（libncm 歌曲 JSON 的 cover）
    qint64 albumId = 0; // 专辑 id（兜底图标 seed / 去重预留）
    int durationMs = 0;
    bool playable = true;
};

/*
 * libncm 的 Qt 薄封装（单例）。
 * - 网络调用阻塞（Go 内部 HTTP），一律 std::thread 工作线程执行后直接
 *   Q_EMIT（Auto 连接跨线程自动排队，Qt6 自动注册参数元类型）。
 * - ABI：单客户端内部串行化；事件回调在 Go goroutine 触发，
 *   经 QMetaObject::invokeMethod 队列回主线程再发信号。
 */
class NcmApi : public QObject
{
    Q_OBJECT
public:
    static NcmApi& instance();

    QString version() const;

    void searchAsync(const QString& keywords, int limit);
    void resolveAsync(qint64 songId, int quality);
    void resolveForDownloadAsync(qint64 songId);   // 仅取 URL（不触发播放）
    void lyricAsync(qint64 songId);
    void artistAsync(qint64 artistId);   // 艺人详情（头像 URL）
    void likeListAsync();                // 账号"喜欢的音乐" id 列表（需登录）
    void likeAsync(qint64 songId, bool on);   // 收藏/取消收藏（需登录）
    void songDetailAsync(const QString& ids); // 批量歌曲详情（逗号分隔 id）

Q_SIGNALS:
    void searchDone(const QList<Song>& songs, const QString& err);
    void resolveDone(qint64 songId, const QString& url, const QString& err);
    void downloadUrlDone(qint64 songId, const QString& url, const QString& err);
    void lyricDone(qint64 songId, const QString& lrc, const QString& err);
    void artistDone(qint64 artistId, const QString& picUrl, const QString& err);
    void likeListDone(const QList<qint64>& ids, const QString& err);
    void likeDone(qint64 songId, bool on, const QString& err);
    void songDetailDone(const QList<Song>& songs, const QString& err);
    void playFailReceived(qint64 songId, const QString& reason);

private:
    explicit NcmApi(QObject* parent = nullptr);

    ncm::Client m_client;
};

#endif
