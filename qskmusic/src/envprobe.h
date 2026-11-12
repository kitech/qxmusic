#ifndef ENV_PROBE_H
#define ENV_PROBE_H

#include <QString>

/*
 * 运行环境探测：启动时采集一次设备/系统/权限事实，打印 [ENV] 日志
 * （qDebug + LogModel，App 内 Logs 页可见），并提供差异化处理所需的查询。
 * 只做观察与记录，不改变播控/保活逻辑。
 */
class EnvProbe
{
public:
    struct Env
    {
        bool valid = false;
        int sdk = 0;
        QString release;
        QString brand;
        QString manufacturer;
        QString model;
        QString rom;
        QString abi;
        bool notifPermitted = true;      // POST_NOTIFICATIONS（<33 恒 true）
        bool notifEnabled = true;        // 用户总开关
        bool batteryOptIgnored = true;   // 是否已加入电池优化白名单
        bool fgsPermitted = true;        // FOREGROUND_SERVICE_MEDIA_PLAYBACK（<34 恒 true）
        bool interactive = true;         // 屏幕是否点亮
        bool powerSave = false;          // 省电模式
        bool lowRam = false;
        bool emulator = false;
        bool focus26 = false;            // 可用 AudioFocusRequest
        bool mediaSession = false;       // 平台 MediaSession 可用
    };

    static EnvProbe& instance();

    void detect();                       // 采集 + 日志（幂等，仅主线程调用）
    const Env& env() const { return m_env; }

    QString profile() const;             // legacy/modern/notif-runtime/strict/desktop
    bool isAndroid() const { return m_android; }
    bool isLegacyFocus() const;          // sdk < 26
    bool needsNotifPermission() const;   // sdk >= 33
    bool needsFgsType() const;           // sdk >= 34
    bool isMiuiLike() const;             // MIUI/HyperOS 类 ROM 诊断

    // [ENV] 双通道输出：qDebug（logcat）+ LogModel（App 内 Logs 页）
    void log(const QString& level, const QString& message) const;

private:
    EnvProbe() = default;
    Env m_env;
    bool m_android = false;
};

#endif // ENV_PROBE_H
