#include "keepalive.h"
#include "envprobe.h"
#include "playback.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMetaObject>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <jni.h>

namespace {
/* Java → C++ 回调（运行在 Android 主线程 = Qt 主线程；仍按队列转投，避免重入） */
void jniMediaCommand(JNIEnv*, jclass, jint cmd)
{
    KeepAlive::dispatchMediaCommand(int(cmd));
}

void jniFocusChange(JNIEnv*, jclass, jint change)
{
    KeepAlive::dispatchFocusChange(int(change));
}
}
#endif

void KeepAlive::start()
{
#ifdef Q_OS_ANDROID
    const EnvProbe& probe = EnvProbe::instance();
    const EnvProbe::Env& env = probe.env();
    qDebug() << "[ENV] keepalive start: profile=" << probe.profile()
             << "notifPermitted=" << env.notifPermitted
             << "notifEnabled=" << env.notifEnabled
             << "fgsPermitted=" << env.fgsPermitted
             << "batteryOptIgnored=" << env.batteryOptIgnored;

    installNativeCallbacks();

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/PermissionHelper",
            "requestNotificationPermission",
            "(Landroid/app/Activity;)V",
            ctx.object());

        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/KeepAliveService",
            "startService",
            "(Landroid/content/Context;)V",
            ctx.object());

        QJniEnvironment jni;
        if (jni.checkAndClearExceptions(QJniEnvironment::OutputMode::Silent))
            qWarning() << "[ENV] keepalive start threw a Java exception (cleared)";
    });
#endif
}

void KeepAlive::installNativeCallbacks()
{
#ifdef Q_OS_ANDROID
    static bool installed = false;
    if (installed)
        return;

    JNINativeMethod methods[] = {
        { const_cast<char*>("nativePlaybackCommand"), const_cast<char*>("(I)V"),
          reinterpret_cast<void*>(jniMediaCommand) },
        { const_cast<char*>("nativeFocusChange"), const_cast<char*>("(I)V"),
          reinterpret_cast<void*>(jniFocusChange) }
    };

    QJniEnvironment jni;
    installed = jni.registerNativeMethods(
        "io/fedlet/mobutil/KeepAliveService", methods, 2);
    if (installed)
        qDebug() << "[ENV] media natives registered=2";
    else
        qWarning() << "[ENV] media natives registration FAILED "
                      "(lockscreen controls degraded)";
#endif
}

void KeepAlive::stop()
{
#ifdef Q_OS_ANDROID
    qDebug() << "[ENV] keepalive stop";
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/KeepAliveService",
            "stopService",
            "(Landroid/content/Context;)V",
            ctx.object());
    });
#endif
}

void KeepAlive::setPlaybackActive(bool playing)
{
#ifdef Q_OS_ANDROID
    qDebug() << "[ENV] playback active ->" << playing
             << "(wake lock" << (playing ? "acquire" : "release") << ")";
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([playing]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/KeepAliveService",
            "setPlaybackActive",
            "(Landroid/content/Context;Z)V",
            ctx.object(), jboolean(playing));
    });
#else
    Q_UNUSED(playing)
#endif
}

bool KeepAlive::requestAudioFocus()
{
#ifdef Q_OS_ANDROID
    bool granted = false;
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([&granted]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        granted = QJniObject::callStaticMethod<jboolean>(
            "io/fedlet/mobutil/KeepAliveService",
            "requestAudioFocus",
            "(Landroid/content/Context;)Z",
            ctx.object()) == JNI_TRUE;
    }).waitForFinished();
    qDebug() << "[ENV] focus request granted=" << granted;
    return granted;
#else
    return true;
#endif
}

void KeepAlive::abandonAudioFocus()
{
#ifdef Q_OS_ANDROID
    qDebug() << "[ENV] focus abandon";
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/KeepAliveService",
            "abandonAudioFocus",
            "(Landroid/content/Context;)V",
            ctx.object());
    });
#endif
}

void KeepAlive::setMetadata(const QString& title, const QString& artist, qint64 durationMs)
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
        [title, artist, durationMs]() {
            QJniObject jtitle = QJniObject::fromString(title);
            QJniObject jartist = QJniObject::fromString(artist);
            QJniObject::callStaticMethod<void>(
                "io/fedlet/mobutil/KeepAliveService",
                "setMetadata",
                "(Ljava/lang/String;Ljava/lang/String;J)V",
                jtitle.object(), jartist.object(), jlong(durationMs));
        });
#else
    Q_UNUSED(title)
    Q_UNUSED(artist)
    Q_UNUSED(durationMs)
#endif
}

void KeepAlive::setPlaybackState(int state, qint64 positionMs, float speed)
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
        [state, positionMs, speed]() {
            QJniObject::callStaticMethod<void>(
                "io/fedlet/mobutil/KeepAliveService",
                "setPlaybackState",
                "(IJF)V",
                jint(state), jlong(positionMs), jfloat(speed));
        });
#else
    Q_UNUSED(state)
    Q_UNUSED(positionMs)
    Q_UNUSED(speed)
#endif
}

void KeepAlive::dispatchMediaCommand(int cmd)
{
    Playback& p = Playback::instance();
    QMetaObject::invokeMethod(&p, [cmd]() {
        Playback::instance().handleMediaCommand(cmd);
    }, Qt::QueuedConnection);
}

void KeepAlive::dispatchFocusChange(int change)
{
    Playback& p = Playback::instance();
    QMetaObject::invokeMethod(&p, [change]() {
        Playback::instance().handleFocusChange(change);
    }, Qt::QueuedConnection);
}
