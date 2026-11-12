#ifndef ICON_LOADER_H
#define ICON_LOADER_H

#include <qobject.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qmutex.h>

class QThread;
class QEvent;
class QCustomEvent;

/*
 * qlmusic 封面加载器（Qt3/4/6 通用）：
 *   GUI 线程：cover() 先查内存 → 磁盘；命中直接返回；
 *             未命中返回 identicon 兜底并排队下载。
 *   工作线程：QThread + libcurl 下载原图 → musiccom 磁盘缓存（文件名=URL 原名）。
 *   完成 → 自定义事件回 GUI 线程（Qt3 无跨线程信号）→ iconsReady() 触发重绘。
 */
class IconLoader : public QObject
{
    Q_OBJECT
public:
    static IconLoader& instance();

    QPixmap cover(const QString& url, const QString& seed, int px);
    QPixmap generated(const QString& seed, int px);   // identicon（带内存缓存）
    void trimCache();

    // 工作线程调用：投递完成事件到 GUI 线程
    void postDone(const QString& url);

signals:
    void iconsReady();

private slots:
    void trimLater();

protected:
#ifdef QT3_BUILD
    void customEvent(QCustomEvent* event);
#else
    void customEvent(QEvent* event);
#endif

private:
    explicit IconLoader(QObject* parent = 0);

    void requestDownload(const QString& url);
    void startFetch(const QString& url);   // 调用方需持有 m_mutex
    void handleDone(const QString& url);

    QMap<QString, int> m_inFlight;
    QMap<QString, QThread*> m_threads;
    QStringList m_queue;
    int m_active;
    int m_done;        // 完成计数 → 每 50 次触发 trim
    QMutex m_mutex;
};

#endif // ICON_LOADER_H
