#ifndef KEEP_ALIVE_H
#define KEEP_ALIVE_H

#include <QObject>
#include <QString>

class KeepAlive : public QObject
{
    Q_OBJECT
public:
    // 与 android.media.session.PlaybackState 常量对齐
    enum MediaState {
        StateStopped   = 1,
        StatePaused    = 2,
        StatePlaying   = 3,
        StateBuffering = 6
    };

    static void start();
    static void stop();
    // 播放中持 PARTIAL_WAKE_LOCK（熄屏不断播）；暂停释放（Android 专属，桌面空操作）
    static void setPlaybackActive(bool playing);

    // ── 音频焦点（桌面恒返回 true / 空操作）──
    static bool requestAudioFocus();
    static void abandonAudioFocus();

    // ── 锁屏/通知元数据与状态（桌面空操作）──
    static void setMetadata(const QString& title, const QString& artist, qint64 durationMs);
    static void setPlaybackState(int state, qint64 positionMs, float speed);

    // Java → C++ 回调入口（native 注册于 installNativeCallbacks）
    static void dispatchMediaCommand(int cmd);   // 1 play 2 pause 3 next 4 prev 5 toggle
    static void dispatchFocusChange(int change); // AudioManager 常量

private:
    static void installNativeCallbacks();        // 幂等：每次 start 前调用
};

#endif // KEEP_ALIVE_H
