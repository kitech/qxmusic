#include "myi18n.h"

#include <QLocale>
#include <QMetaObject>
#include <QSettings>

Lang& Lang::instance() {
    static Lang g;
    return g;
}

Lang::Lang() {
    const QString saved = QSettings().value("language", QString()).toString();
    m_code = saved.isEmpty() ? detectSystem() : saved;
    applyTranslator();
}

/* 系统语言检测（构造与「跟随系统」共用）：
 *  - 中文按 script/territory 判简繁（zh-Hant / TW / HK / MO → 繁）
 *  - 英文 → en
 *  - 其它语言：再看 uiLanguages 里是否有中/英
 *  - 都没有 → en（国际默认；与 qsktox 一致）
 * Android/iOS/桌面 同一套：Qt 各平台 QLocale::system() 均读系统区域设置，
 * Android 上为 Java Locale → Qt 映射，无需 JNI 特判。 */
QString Lang::detectSystem()
{
    const QLocale sys = QLocale::system();
    if (sys.language() == QLocale::Chinese) {
        const bool trad = sys.script() == QLocale::TraditionalChineseScript
            || sys.territory() == QLocale::Taiwan
            || sys.territory() == QLocale::HongKong
            || sys.territory() == QLocale::Macao;
        return trad ? "zh-TW" : "zh-CN";
    }
    if (sys.language() == QLocale::English)
        return "en";
    const QStringList ui = sys.uiLanguages();
    for (const QString& tag : ui) {
        const QLocale l(tag);
        if (l.language() == QLocale::Chinese) {
            return (l.script() == QLocale::TraditionalChineseScript
                    || l.territory() == QLocale::Taiwan
                    || l.territory() == QLocale::HongKong
                    || l.territory() == QLocale::Macao)
                       ? "zh-TW" : "zh-CN";
        }
        if (l.language() == QLocale::English)
            return "en";
    }
    return "en";
}

void Lang::applyTranslator() {
    if (m_zhCN) QCoreApplication::removeTranslator(m_zhCN);
    if (m_en)   QCoreApplication::removeTranslator(m_en);
    if (m_zhTW) QCoreApplication::removeTranslator(m_zhTW);

    QTranslator** slot;
    QString qmName;
    if (m_code == "en") {
        slot   = &m_en;
        qmName = "qskmusic_en";
    } else if (m_code == "zh-TW") {
        slot   = &m_zhTW;
        qmName = "qskmusic_zh_TW";
    } else {
        slot   = &m_zhCN;
        qmName = "qskmusic_zh_CN";
    }

    if (*slot == nullptr)
        *slot = new QTranslator(this);
    if (!(*slot)->load(QString(":/i18n/%1.qm").arg(qmName)))
        qWarning("[i18n] failed to load %s.qm", qUtf8Printable(qmName));
    QCoreApplication::installTranslator(*slot);
}

void Lang::setLanguage(const QString& code) {
    if (code == m_code)
        return;
    m_code = code;
    applyTranslator();
    QSettings().setValue("language", code);
    for (QObject* p : std::as_const(m_pages)) {
        p->metaObject()->invokeMethod(p, "retranslateUi", Qt::DirectConnection);
    }
    emit languageChanged();
}

void Lang::useSystemLanguage()
{
    m_code = detectSystem();
    applyTranslator();
    QSettings().remove("language");   // 清除保存值 → 下次启动仍跟随系统
    for (QObject* p : std::as_const(m_pages))
        p->metaObject()->invokeMethod(p, "retranslateUi", Qt::DirectConnection);
    emit languageChanged();
}

void Lang::registerRetranslatable(QObject* page) {
    if (page && !m_pages.contains(page))
        m_pages.append(page);
}

void Lang::unregister(QObject* page) {
    m_pages.removeAll(page);
}
