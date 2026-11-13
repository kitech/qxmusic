#include "iconcachecore.h"

#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qdatetime.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qmutex.h>
#ifdef QT3_BUILD
#include <qglobal.h>       // Qt3：qDebug 声明在 qglobal.h
#else
#include <qdebug.h>
#endif

#include <cstdlib>
#include <cstring>

#if defined(Q_OS_ANDROID) && QT_VERSION >= 0x050000
#include <QStandardPaths>
#endif

namespace qximage {
namespace {

/* ── Qt3/4+ 兼容小工具 ── */
QString qxTrimmed(const QString& s)
{
#if QT_VERSION < 0x040000
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

int qxIndexOf(const QString& s, char c)
{
#if QT_VERSION < 0x040000
    return s.find(QChar(c));
#else
    return s.indexOf(QLatin1Char(c));
#endif
}

int qxLastIndexOf(const QString& s, char c)
{
#if QT_VERSION < 0x040000
    return s.findRev(QChar(c));
#else
    return s.lastIndexOf(QLatin1Char(c));
#endif
}

QString qxEnv(const char* name)
{
    const char* value = getenv(name);
    return value ? QString::fromLocal8Bit(value) : QString();
}

QString qxHomeDir()
{
#if QT_VERSION < 0x040000
    return QDir::homeDirPath();
#else
    return QDir::homePath();
#endif
}

/* 逐级创建目录（Qt3 无 mkpath；Qt4+ 行为一致） */
void qxMakePath(const QString& path)
{
    if (path.isEmpty())
        return;
    QString current;
    int pos = 0;
    const int len = path.length();
    if (path.startsWith(QString::fromLatin1("/"))) {
        current = QString::fromLatin1("/");
        pos = 1;
    }
    for (; pos <= len; ++pos) {
        if (pos == len || path.at(pos) == QChar('/')) {
            if (!current.isEmpty() && current != QString::fromLatin1("/"))
                QDir().mkdir(current);
            if (pos < len)
                current += QString::fromLatin1("/");
            continue;
        }
        current += path.at(pos);
    }
}

bool qxRename(const QString& from, const QString& to)
{
    return QDir().rename(from, to);   // Qt3/4/6 均存在
}

int qxFileCount(const QFileInfoList* list)
{
#ifdef QT3_BUILD
    return list ? list->count() : 0;
#else
    return list ? list->size() : 0;
#endif
}

const QFileInfo& qxFileAt(const QFileInfoList* list, int i)
{
#ifdef QT3_BUILD
    // Qt3 QPtrList::at() 非 const（历史 API），此处 const_cast 安全：只读访问
    return *const_cast<QFileInfoList*>(list)->at(i);
#else
    return list->at(i);
#endif
}

void qxLog(const QString& msg)
{
#ifdef QT3_BUILD
    qDebug(msg);                 // Qt3：qDebug(const QString&)
#elif QT_VERSION >= 0x050400
    qDebug().noquote() << msg;
#else
    qDebug() << msg;             // Qt4：无 noquote
#endif
}

unsigned int qxNowSecs()
{
#if QT_VERSION >= 0x050800
    return (unsigned int)QDateTime::currentSecsSinceEpoch();
#else
    return (unsigned int)QDateTime::currentDateTime().toTime_t();
#endif
}

QMutex& negativeMutex()
{
    static QMutex mutex;
    return mutex;
}

QMap<QString, unsigned int>& negativeMap()
{
    static QMap<QString, unsigned int> map;
    return map;
}

/* 失败负缓存：60s（原 300s）。Android cleartext/网络抖动修复后，
 * 过长的负缓存会让已恢复的封面继续兜底图标 */
const unsigned int kNegativeTtlSec = 60;

} // namespace

QString cacheDir()
{
    QString dir;
    const QString override = qxEnv("QXIMAGE_CACHE_DIR");
    if (!override.isEmpty()) {
        dir = override;
    } else {
#if defined(Q_OS_ANDROID) && QT_VERSION >= 0x050000
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        dir += QString::fromLatin1("/qxmusic/covers");
#else
        const QString xdg = qxEnv("XDG_CACHE_HOME");
        if (!xdg.isEmpty())
            dir = xdg + QString::fromLatin1("/qxmusic/covers");
        else
            dir = qxHomeDir() + QString::fromLatin1("/.cache/qxmusic/covers");
#endif
    }

    qxMakePath(dir);
    return dir;
}

QString normalizedUrl(const QString& url)
{
    QString s = qxTrimmed(url);
    const int q = qxIndexOf(s, '?');
    if (q >= 0)
        s = s.left(q);
    const int h = qxIndexOf(s, '#');
    if (h >= 0)
        s = s.left(h);
    while (s.endsWith(QString::fromLatin1("/")))
        s = s.left(s.length() - 1);
    return s;
}

QString fileNameForUrl(const QString& url)
{
    const QString path = normalizedUrl(url);
    const int slash =
#if QT_VERSION < 0x040000
        path.findRev('/');
#else
        path.lastIndexOf(QLatin1Char('/'));
#endif
    QString name = (slash >= 0) ? path.mid(slash + 1) : path;

    // 过滤路径分隔符与文件系统非法字符（保留中文/点号）
    QString clean;
    for (int i = 0; i < name.length(); ++i) {
        const QChar c = name.at(i);
        const ushort u = c.unicode();
        if (u < 0x20)
            continue;
        if (c == QChar('/') || c == QChar('\\') || c == QChar(':')
            || c == QChar('*') || c == QChar('?') || c == QChar('"')
            || c == QChar('<') || c == QChar('>') || c == QChar('|'))
            continue;
        clean += c;
    }
    clean = qxTrimmed(clean);

    if (clean.isEmpty())
        return QString();

    if (qxIndexOf(clean, '.') < 0)
        clean += QString::fromLatin1(".img");

    // 文件名长度保护：保留扩展名，主体截到 90 字符
    if (clean.length() > 100) {
        const int dot = qxLastIndexOf(clean, '.');
        QString base = (dot > 0) ? clean.left(dot) : clean;
        const QString ext = (dot > 0) ? clean.mid(dot) : QString();
        if (base.length() > 90)
            base = base.left(90);
        clean = base + ext;
    }
    return clean;
}

QString pathForUrl(const QString& url)
{
    const QString name = fileNameForUrl(url);
    if (name.isEmpty())
        return QString();
    return cacheDir() + QString::fromLatin1("/") + name;
}

bool contains(const QString& url)
{
    const QString path = pathForUrl(url);
    if (path.isEmpty())
        return false;
    QFileInfo fi(path);
    return fi.exists() && fi.size() > 0;
}

QByteArray read(const QString& url)
{
    const QString path = pathForUrl(url);
    if (path.isEmpty())
        return QByteArray();

    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly))
        return QByteArray();
#else
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
#endif
    return f.readAll();
}

bool write(const QString& url, const QByteArray& data)
{
    if (data.isEmpty())
        return false;
    const QString path = pathForUrl(url);
    if (path.isEmpty())
        return false;

    QFileInfo fi(path);
    if (fi.exists() && fi.size() > 0)
        return true;                    // 同名文件已存在：保留不覆盖

    const QString tmp = path + QString::fromLatin1(".part");
    QFile f(tmp);
#ifdef QT3_BUILD
    if (!f.open(IO_WriteOnly)) {
        qxLog(QString::fromLatin1("[ICON] cache-open-failed ") + tmp);
        return false;
    }
    const bool ok = (f.writeBlock(data) == (long)data.size());
#else
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qxLog(QString::fromLatin1("[ICON] cache-open-failed ") + tmp);
        return false;
    }
    const bool ok = (f.write(data) == data.size());
#endif
    f.close();

    if (!ok || !qxRename(tmp, path)) {
        qxLog(QString::fromLatin1("[ICON] cache-write-failed ") + path);
        QFile::remove(tmp);
        return false;
    }
    return true;
}

void remove(const QString& url)
{
    const QString path = pathForUrl(url);
    if (!path.isEmpty())
        QFile::remove(path);
}

QString extensionForData(const QByteArray& d)
{
    const int n = (int)d.size();
    const char* p = d.data();
    if (n >= 3 && (unsigned char)p[0] == 0xFF
        && (unsigned char)p[1] == 0xD8
        && (unsigned char)p[2] == 0xFF)
        return QString::fromLatin1("jpg");
    if (n >= 8 && (unsigned char)p[0] == 0x89 && memcmp(p + 1, "PNG", 3) == 0)
        return QString::fromLatin1("png");
    if (n >= 12 && memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "WEBP", 4) == 0)
        return QString::fromLatin1("webp");
    if (n >= 4 && memcmp(p, "GIF8", 4) == 0)
        return QString::fromLatin1("gif");
    return QString();
}

void trim(long long maxBytes)
{
    QDir dir(cacheDir());
#ifdef QT3_BUILD
    const QFileInfoList* infos = dir.entryInfoList(QDir::Files, QDir::Time);
#else
    const QFileInfoList values = dir.entryInfoList(QDir::Files, QDir::Time);
    const QFileInfoList* infos = &values;
#endif
    if (!infos)
        return;

    const QDateTime now = QDateTime::currentDateTime();

    // 先清理超 1 小时的 .part 残渣（崩溃/被杀进程遗留）
    for (int i = 0; i < qxFileCount(infos); ++i) {
        const QFileInfo& fi = qxFileAt(infos, i);
        if (!fi.fileName().endsWith(QString::fromLatin1(".part")))
            continue;
        if (fi.lastModified().secsTo(now) >= 3600)
            QFile::remove(fi.filePath());
    }

    long long total = 0;
    for (int i = 0; i < qxFileCount(infos); ++i) {
        const QFileInfo& fi = qxFileAt(infos, i);
        if (fi.fileName().endsWith(QString::fromLatin1(".part")))
            continue;
        total += (long long)fi.size();
    }

    if (total <= maxBytes)
        return;

    int removed = 0;
    // QDir::Time：最新在前 → 从尾部（最旧）删除
    for (int i = qxFileCount(infos) - 1; i >= 0 && total > maxBytes; --i) {
        const QFileInfo& fi = qxFileAt(infos, i);
        if (fi.fileName().endsWith(QString::fromLatin1(".part")))
            continue;
        if (fi.lastModified().secsTo(now) < 3600)
            continue;                   // 1 小时内新文件跳过（另一端可能正在读）
        const long long size = (long long)fi.size();
        if (QFile::remove(fi.filePath())) {
            total -= size;
            ++removed;
        }
    }
    if (removed > 0)
        qxLog(QString::fromLatin1("[ICON] evict files=") + QString::number(removed)
              + QString::fromLatin1(" remainBytes=") + QString::number((long)total));
}

bool negativeHit(const QString& url)
{
    QMutexLocker locker(&negativeMutex());
    QMap<QString, unsigned int>& map = negativeMap();
    const QString key = normalizedUrl(url);
    QMap<QString, unsigned int>::iterator it = map.find(key);
    if (it == map.end())
        return false;
#if QT_VERSION < 0x040000
    const unsigned int stamp = it.data();
#else
    const unsigned int stamp = it.value();
#endif
    if (qxNowSecs() - stamp > kNegativeTtlSec) {
#if QT_VERSION >= 0x040000
        map.erase(it);
#else
        map.remove(it);
#endif
        return false;
    }
    return true;
}

void negativePut(const QString& url)
{
    QMutexLocker locker(&negativeMutex());
    negativeMap().insert(normalizedUrl(url), qxNowSecs());
}

int fileCount()
{
    QDir dir(cacheDir());
#ifdef QT3_BUILD
    const QFileInfoList* infos = dir.entryInfoList(QDir::Files);
#else
    const QFileInfoList values = dir.entryInfoList(QDir::Files);
    const QFileInfoList* infos = &values;
#endif
    return qxFileCount(infos);
}

long long totalBytes()
{
    QDir dir(cacheDir());
#ifdef QT3_BUILD
    const QFileInfoList* infos = dir.entryInfoList(QDir::Files);
#else
    const QFileInfoList values = dir.entryInfoList(QDir::Files);
    const QFileInfoList* infos = &values;
#endif
    if (!infos)
        return 0;
    long long total = 0;
    for (int i = 0; i < qxFileCount(infos); ++i)
        total += (long long)qxFileAt(infos, i).size();
    return total;
}

} // namespace qximage
