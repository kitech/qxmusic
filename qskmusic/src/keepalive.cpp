#include "keepalive.h"
#include "envprobe.h"
#include <QDebug>
#include <QCoreApplication>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
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
