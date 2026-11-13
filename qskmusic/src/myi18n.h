#ifndef QSKM_I18N_H
#define QSKM_I18N_H

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVector>

// 语言单例：安装/切换 QTranslator，广播即时重译。
// qskmusic 为 Qt6 纯净工程（自 anystik/stikcommon myi18n 裁 Qt3/Qt4 分支而来）。
class Lang : public QObject {
    Q_OBJECT
public:
    static Lang& instance();
    QString code() const { return m_code; }
    void setLanguage(const QString& code);   // swap qm + QSettings 保存 + 广播
    void useSystemLanguage();                // 跟随系统（清除保存值 + 重新检测）
    void registerRetranslatable(QObject* page); // 需存在槽 retranslateUi()
    void unregister(QObject* page);

signals:
    void languageChanged();

private:
    Lang();
    static QString detectSystem();   // 系统语言 → 受支持 code
    void applyTranslator();
    QString m_code;
    QVector<QObject*> m_pages;
    QTranslator* m_zhCN   = nullptr; // 收录英文源串→中文
    QTranslator* m_en     = nullptr;
    QTranslator* m_zhTW   = nullptr;
};

#endif
