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
    QString album;
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
    void lyricAsync(qint64 songId);

Q_SIGNALS:
    void searchDone(const QList<Song>& songs, const QString& err);
    void resolveDone(qint64 songId, const QString& url, const QString& err);
    void lyricDone(qint64 songId, const QString& lrc, const QString& err);
    void playFailReceived(qint64 songId, const QString& reason);

private:
    explicit NcmApi(QObject* parent = nullptr);

    ncm::Client m_client;
};

#endif
