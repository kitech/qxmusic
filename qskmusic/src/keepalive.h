#ifndef KEEP_ALIVE_H
#define KEEP_ALIVE_H

#include <QObject>

class KeepAlive : public QObject
{
    Q_OBJECT
public:
    static void start();
    static void stop();
    // 播放中持 PARTIAL_WAKE_LOCK（熄屏不断播）；暂停释放（Android 专属，桌面空操作）
    static void setPlaybackActive(bool playing);
};

#endif
