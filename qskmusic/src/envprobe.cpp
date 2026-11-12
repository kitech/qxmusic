#include "envprobe.h"
#include "logmodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QStringList>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace {

QHash<QString, QString> parseSnapshot(const QString& text)
{
    QHash<QString, QString> map;
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq > 0)
            map.insert(part.left(eq), part.mid(eq + 1));
    }
    return map;
}

int intOf(const QHash<QString, QString>& map, const char* key, int fallback)
{
    bool ok = false;
    const int value = map.value(QLatin1String(key)).toInt(&ok);
    return ok ? value : fallback;
}

bool boolOf(const QHash<QString, QString>& map, const char* key, bool fallback)
{
    const QString value = map.value(QLatin1String(key));
    if (value.isEmpty())
        return fallback;
    return value == QLatin1String("1");
}

QString strOf(const QHash<QString, QString>& map, const char* key)
{
    return map.value(QLatin1String(key), QStringLiteral("?"));
}

} // namespace

EnvProbe& EnvProbe::instance()
{
    static EnvProbe s_instance;
    return s_instance;
}

void EnvProbe::detect()
{
    m_env = Env();

#ifdef Q_OS_ANDROID
    m_android = true;

    auto context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        log(QStringLiteral("warn"),
            QStringLiteral("probe skipped: android context invalid"));
        return;
    }

    QJniObject snapshot = QJniObject::callStaticObjectMethod(
        "io/fedlet/mobutil/EnvInfo",
        "snapshot",
        "(Landroid/content/Context;)Ljava/lang/String;",
        context.object());

    if (!snapshot.isValid()) {
        log(QStringLiteral("warn"),
            QStringLiteral("probe skipped: EnvInfo.snapshot() failed"));
        return;
    }

    const QString text = snapshot.toString();
    const QHash<QString, QString> map = parseSnapshot(text);

    Env& e = m_env;
    e.valid = true;
    e.sdk = intOf(map, "sdk", 0);
    e.release = strOf(map, "release");
    e.brand = strOf(map, "brand");
    e.manufacturer = strOf(map, "manufacturer");
    e.model = strOf(map, "model");
    e.rom = strOf(map, "rom");
    e.abi = strOf(map, "abi");
    e.notifPermitted = boolOf(map, "notifPerm", true);
    e.notifEnabled = boolOf(map, "notifEnabled", true);
    e.batteryOptIgnored = boolOf(map, "batteryOptIgnored", true);
    e.fgsPermitted = boolOf(map, "fgsPerm", true);
    e.interactive = boolOf(map, "interactive", true);
    e.powerSave = boolOf(map, "powerSave", false);
    e.lowRam = boolOf(map, "lowRam", false);
    e.emulator = boolOf(map, "emulator", false);
    e.focus26 = boolOf(map, "focus26", e.sdk >= 26);
    e.mediaSession = boolOf(map, "mediaSession", e.sdk >= 21);

    // 完整快照（logcat 便于排查，内容与 EnvInfo.snapshot() 完全一致）
    qDebug().noquote() << "[ENV] snapshot" << text;
    log(QStringLiteral("info"),
        QStringLiteral("profile=%1 sdk=%2 release=%3 brand=%4 model=%5 rom=%6 abi=%7")
            .arg(profile(), QString::number(e.sdk), e.release, e.brand,
                 e.model, e.rom, e.abi));

    // 关键风险项：只记录，不打扰用户
    if (!e.notifPermitted)
        log(QStringLiteral("warn"),
            QStringLiteral("notification permission DENIED "
                           "(media-session notifications stay exempt)"));
    if (!e.notifEnabled)
        log(QStringLiteral("warn"),
            QStringLiteral("notifications disabled by user "
                           "(lockscreen controls may be hidden)"));
    if (!e.batteryOptIgnored)
        log(QStringLiteral("warn"),
            QStringLiteral("battery optimization NOT ignored "
                           "(OEM may kill background playback)"));
    if (!e.fgsPermitted)
        log(QStringLiteral("error"),
            QStringLiteral("FOREGROUND_SERVICE_MEDIA_PLAYBACK not granted "
                           "(foreground service will degrade)"));
    if (e.powerSave)
        log(QStringLiteral("info"), QStringLiteral("system power-save mode is ON"));
    if (e.lowRam)
        log(QStringLiteral("info"), QStringLiteral("low-RAM device"));
    if (e.emulator)
        log(QStringLiteral("info"), QStringLiteral("emulator/virtual device"));
    if (isMiuiLike())
        log(QStringLiteral("info"),
            QStringLiteral("MIUI/HyperOS-like ROM detected "
                           "(extra background-kill diagnostics)"));

    return;
#else
    m_android = false;
    m_env.valid = true;
    const QString qtVersion = QString::fromLatin1(qVersion());
    qDebug().noquote() << "[ENV] snapshot platform=desktop qt=" + qtVersion;
    log(QStringLiteral("info"),
        QStringLiteral("profile=desktop platform=desktop qt=%1").arg(qtVersion));
#endif
}

QString EnvProbe::profile() const
{
    if (!m_android)
        return QStringLiteral("desktop");
    if (m_env.sdk >= 34)
        return QStringLiteral("strict");
    if (m_env.sdk >= 33)
        return QStringLiteral("notif-runtime");
    if (m_env.sdk >= 26)
        return QStringLiteral("modern");
    return QStringLiteral("legacy");
}

bool EnvProbe::isLegacyFocus() const
{
    return m_android && m_env.sdk > 0 && m_env.sdk < 26;
}

bool EnvProbe::needsNotifPermission() const
{
    return m_android && m_env.sdk >= 33;
}

bool EnvProbe::needsFgsType() const
{
    return m_android && m_env.sdk >= 34;
}

bool EnvProbe::isMiuiLike() const
{
    const QString rom = m_env.rom.toLower();
    const QString brand = m_env.brand.toLower();
    return rom.contains(QStringLiteral("miui"))
        || rom.contains(QStringLiteral("hyperos"))
        || brand.contains(QStringLiteral("xiaomi"))
        || brand.contains(QStringLiteral("redmi"))
        || brand.contains(QStringLiteral("poco"));
}

void EnvProbe::log(const QString& level, const QString& message) const
{
    qDebug().noquote() << "[ENV]" << message;

    LogModel::Level modelLevel = LogModel::Info;
    if (level == QLatin1String("warn"))
        modelLevel = LogModel::Warn;
    else if (level == QLatin1String("error"))
        modelLevel = LogModel::Error;

    LogModel::instance().append(modelLevel, QStringLiteral("env"), message);
}
