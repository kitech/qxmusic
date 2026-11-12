#include "playerbar.h"
#include "musicstyle.h"
#include "translator.h"
#include "identicon.h"
#include "iconloader.h"

#include <qglobal.h>
#include <qpainter.h>
#include <qevent.h>
#include <qfontmetrics.h>
#include <qframe.h>
#include <qcheckbox.h>
#include <qapplication.h>
#include <qtimer.h>
#include <algorithm>
#ifdef QT3_BUILD
#include <qpointarray.h>
#include <qtooltip.h>
#else
#include <QToolTip>
#endif

/* ASCII 数字判定（qt3 QChar 无 isDigit，用 unicode 域判零依赖） */
static bool lrcIsDigit(const QChar& c)
{
    return c.unicode() >= '0' && c.unicode() <= '9';
}

/* qt3/qt4 QString 查找/裁剪 compat（qt3: find/findRev/stripWhiteSpace） */
static int qstrFind(const QString& s, QChar c, int from = 0)
{
#ifdef QT3_BUILD
    return s.find(c, from);
#else
    return s.indexOf(c, from);
#endif
}

static int qstrFindRev(const QString& s, QChar c)
{
#ifdef QT3_BUILD
    return s.findRev(c);
#else
    return s.lastIndexOf(c);
#endif
}

static QString qstrTrim(const QString& s)
{
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

struct LrcLine {
    int timeMs;
    QString text;
};

/* 排序比较（函数指针形态，规避老编译器 lambda 兼容风险） */
static bool lrcLineLess(const LrcLine& a, const LrcLine& b)
{
    return a.timeMs < b.timeMs;
}

/* 时间标签 inner（"mm:ss.xx"/"mm:ss"/"mm:ss.xxx"）→ ms；非时间标签返回 -1 */
static int lrcTimeTagMs(const QString& inner)
{
    int colon = qstrFind(inner, ':');
    if (colon < 1 || colon != qstrFindRev(inner, ':')) return -1;
    for (int i = 0; i < colon; ++i)
        if (!lrcIsDigit(inner[i])) return -1;
    int dot = qstrFind(inner, '.', colon);
    int secLen = (dot < 0 ? (int)inner.length() : dot) - colon - 1;
    if (secLen < 0) return -1;
    for (int i = colon + 1; i < colon + 1 + secLen; ++i)
        if (!lrcIsDigit(inner[i])) return -1;
    long mins = 0;
    for (int i = 0; i < colon; ++i) mins = mins * 10 + inner[i].unicode() - '0';
    long secs = 0;
    for (int i = colon + 1; i < colon + 1 + secLen; ++i) secs = secs * 10 + inner[i].unicode() - '0';
    int fracMs = 0;
    if (dot >= 0) {
        /* 1-3 位小数补零右对齐（.4 == .400 == 0.4s） */
        int mul = 100;
        for (int i = dot + 1; i < (int)inner.length(); ++i) {
            if (!lrcIsDigit(inner[i])) return -1;
            if (i - dot > 3) break;                     /* 最多取 3 位 */
            fracMs += (inner[i].unicode() - '0') * mul;
            mul /= 10;
        }
    }
    return (int)((mins * 60 + secs) * 1000 + fracMs);
}

/* LRC 解析（网搜核对集：同行多时间戳/[offset:] 全局偏移/CRLF 兼容/
 * 空文本时间行保留（间奏清屏）/纯文本不同步降级）。
 * offset 正 = 歌词提前（时间戳减 offset） */
static void parseLrc(const QString& raw, std::vector<LrcLine>& out,
                     int& offsetMs, bool& synced, QString& plain)
{
    out.clear();
    offsetMs = 0;
    synced = false;
    plain = QString();
    /* 手动按 \n 切行（规避 QStringList::split 的 qt3/qt4 签名差异） */
    int len = (int)raw.length();
    int pos = 0;
    while (pos <= len) {
        int nl = qstrFind(raw, QChar('\n'), pos);
        if (nl < 0) nl = len;
        QString l = qstrTrim(raw.mid(pos, nl - pos));
        pos = nl + 1;
        if (l.isEmpty()) continue;
        if (l.startsWith(QString::fromUtf8("[offset:")) && l.endsWith(QString::fromUtf8("]"))) {
            offsetMs = qstrTrim(l.mid(8, l.length() - 9)).toInt();
            continue;
        }
        std::vector<int> times;
        while (l.startsWith(QString::fromUtf8("["))) {
            int close = qstrFind(l, ']');
            if (close < 0) break;
            int t = lrcTimeTagMs(l.mid(1, close - 1));
            if (t < 0) break;               /* [ti:] 等元数据标签 → 无时间不参与 */
            times.push_back(t);
            l = l.mid(close + 1);
        }
        if (times.empty()) {
            if (plain.isEmpty()) plain = l;  /* 纯文本降级素材 */
            continue;
        }
        synced = true;
        QString text = qstrTrim(l);
        for (size_t k = 0; k < times.size(); ++k) {
            LrcLine li;
            li.timeMs = times[k] - offsetMs;
            if (li.timeMs < 0) li.timeMs = 0;
            li.text = text;
            out.push_back(li);
        }
    }
    std::sort(out.begin(), out.end(), lrcLineLess);
}

/* 三行滚动歌词窗（网搜核对标准形态：prev 灰 / 当前行 accent+bold / next 灰；
 * tick 由 positionChanged 驱动，行变才 update()；零交互零滚动条） */
class LyricsStrip : public QWidget {
public:
    explicit LyricsStrip(QWidget* parent)
        : QWidget(parent), m_active(-1), m_songTag(0), m_offsetMs(0), m_synced(false) {}

    void setLyrics(qint64 songTag, const QString& lrcText)
    {
        m_songTag = songTag;
        parseLrc(lrcText, m_lines, m_offsetMs, m_synced, m_plain);
        m_active = -1;
        update();
    }

    void clearLyrics()
    {
        m_lines.clear();
        m_plain = QString();
        m_active = -1;
        m_songTag = 0;
        m_synced = false;
        update();
    }

    void setPositionMs(int ms)
    {
        if (!m_synced || m_lines.empty()) return;
        /* 二分：最后一个 start <= ms（seek 后即从 position 重算，防漂移） */
        int lo = 0, hi = (int)m_lines.size() - 1, res = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (m_lines[mid].timeMs <= ms) { res = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        if (res != m_active) {
            m_active = res;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent*)
    {
        const StyleParams::Palette& pal = muiPalette();
        QPainter p(this);
        p.fillRect(rect(), pal.baseBg);
        int rowH = height() / 3;
        if (rowH < 1) return;
        if (m_synced && !m_lines.empty()) {
            QFont f = font();
            QFont bf = font();
            bf.setBold(true);
            int rows[3];
            rows[0] = m_active - 1; rows[1] = m_active; rows[2] = m_active + 1;
            if (m_active < 0) { rows[0] = -1; rows[1] = -1; rows[2] = 0; }
            for (int k = 0; k < 3; ++k) {
                int idx = rows[k];
                if (idx < 0 || idx >= (int)m_lines.size()) continue;
                if (m_lines[idx].text.isEmpty()) continue;   /* 间奏行清屏 */
                bool act = (k == 1 && m_active >= 0);
                p.setFont(act ? bf : f);
                p.setPen(act ? pal.accent : pal.textMuted);
                p.drawText(QRect(8, k * rowH, width() - 16, rowH),
                           Qt::AlignVCenter | Qt::AlignLeft, m_lines[idx].text);
            }
        } else if (!m_plain.isEmpty()) {
            p.setFont(font());
            p.setPen(pal.textMuted);
            p.drawText(rect(), Qt::AlignCenter, m_plain);   /* 纯文本降级（不可同步） */
        } else {
            p.setFont(font());
            p.setPen(pal.textMuted);
            p.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("暂无歌词"));
        }
    }

private:
    std::vector<LrcLine> m_lines;
    QString m_plain;
    int m_active;
    qint64 m_songTag;
    int m_offsetMs;
    bool m_synced;
};

#ifdef QT3_BUILD
/* Qt3 区段级动态 tooltip：maybeTip 虚口，每次命中现读 PlayerBar 公开
 * tipRectFor/tipForPos —— 三态/语言切换即时生效，无需 remove/re-add */
class PlayerBarToolTip : public QToolTip {
public:
    explicit PlayerBarToolTip(QWidget* w) : QToolTip(w), m_w(w) {}
    void maybeTip(const QPoint& p)
    {
        PlayerBar* bar = static_cast<PlayerBar*>(m_w);
        QRect r = bar->tipRectFor(p);
        if (!r.isValid()) return;
        QString t = bar->tipForPos(p);
        if (!t.isEmpty()) tip(r, t);
    }
    /* 点击即时反馈（B2/B3）：tip() 在 QToolTip 内为 protected，经子类公开转发 */
    void showTip(const QRect& r, const QString& t) { tip(r, t); }
private:
    QWidget* m_w;
};
#endif

#ifdef QT3_BUILD
typedef QPointArray MuiPolygon;
#else
typedef QPolygon MuiPolygon;
#endif

/* 自绘弹层滑条（零 QSlider）：bar 进度条同款已验证鼠标链——click-jump+拖动
 * 全自定义，无 Qt3 QSlider pageStep 怪癖；值变化走 m_bar->onPopupVolume(v)
 * （无 Q_OBJECT，零 moc 开销）。click-jump clamp 2..100：点击不直接落 0 误静音，
 * 静音走弹层右侧专用 checkbox（用户拍板：Apple Music Win 同款） */
class VolumePanelSlider : public QWidget {
public:
    VolumePanelSlider(PlayerBar* bar, QWidget* parent)
        : QWidget(parent)
        , m_bar(bar)
        , m_v(100)
        , m_drag(false)
        , m_hover(false)
    {
        setFixedSize(126, 26);
        setMouseTracking(true);
    }
public:
    void setValue(int v) { if (v != m_v) { m_v = v; update(); } }
    int getValue() const { return m_v; }
    void endDrag() { if (m_drag) { m_drag = false; update(); } }

protected:
    void paintEvent(QPaintEvent*)
    {
        const StyleParams::Palette& pal = muiPalette();
        QPainter p(this);
        QRect tr(0, 10, width(), 6);
        p.setPen(Qt::NoPen);
        p.setBrush(pal.border);
        muiDrawRoundRect(p, tr, 3, 3);
        int vw = tr.width() * m_v / 100;
        if (vw > 0) {
            p.setBrush(pal.accent);
            muiDrawRoundRect(p, QRect(tr.x(), tr.y(), vw, tr.height()), 3, 3);
        }
        int hx = tr.x() + vw - 6;
        QColor hc = (m_drag || m_hover) ? pal.accent : pal.textPrimary;
        p.setPen(QPen(pal.border, 1));
        p.setBrush(hc);
        p.drawEllipse(hx, tr.y() + tr.height() / 2 - 6, 12, 12);
    }
    void mousePressEvent(QMouseEvent* e)
    {
        if (e->button() != Qt::LeftButton) { e->ignore(); return; }
        m_drag = true;
        jumpTo(e->pos().x());
    }
    void mouseMoveEvent(QMouseEvent* e) { if (m_drag) jumpTo(e->pos().x()); }
    void mouseReleaseEvent(QMouseEvent*) { if (m_drag) { m_drag = false; update(); } }
    void enterEvent(QEvent*) { m_hover = true; update(); }
    void leaveEvent(QEvent*) { m_hover = false; update(); }

private:
    void jumpTo(int x)
    {
        int v = qBound(0, x, width() - 1) * 100 / (width() - 1);
        if (v < 2) v = 2;            /* click-jump 不过 0（防误静音；拖动亦守底） */
        if (v == m_v) return;
        m_v = v;
        update();
        m_bar->onPopupVolume(v);
    }
    PlayerBar* m_bar;
    int m_v;
    bool m_drag;
    bool m_hover;
};

/* 弹层容器（Qt3 WType_Popup / Qt4 Qt::Popup）：原生"点外自动关"是框架内置
 * 管线（combobox/QMenu 同款），不做任何自添转发/close 补丁；
 * 仅保留官方 popup-example 的 closeEvent fake release 收尾（close 时才跑） */
class VolumePopupFrame : public QFrame {
public:
    VolumePopupFrame(PlayerBar* bar)
#ifdef QT3_BUILD
        : QFrame(bar, 0, Qt::WType_Popup)
#else
        : QFrame(bar, Qt::Popup)
#endif
        , m_bar(bar)
        , m_panel(0)
    {
        setFrameStyle(QFrame::WinPanel | QFrame::Raised);
    }
    void setPanel(VolumePanelSlider* s) { m_panel = s; }

protected:
    void closeEvent(QCloseEvent* e)
    {
        e->accept();
        if (m_panel) m_panel->endDrag();   /* 关弹层解除 drag 残留（防刷屏） */
        /* popup-example.html 3.3.8 原版做法：popup 可能收到 mouse release 而非
         * popupParent——递假 release 归还 parent（button=LeftButton/state=NoButton） */
        QMouseEvent me(QEvent::MouseButtonRelease, QPoint(0, 0), QPoint(0, 0),
#ifdef QT3_BUILD
                       QMouseEvent::LeftButton, QMouseEvent::NoButton);
#else
                       Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
#endif
        QApplication::sendEvent(m_bar, &me);
    }

private:
    PlayerBar* m_bar;
    VolumePanelSlider* m_panel;
};



namespace {
QString fmtMs(int ms)
{
    int total = ms / 1000;
    int m = total / 60;
    int s = total % 60;
    QString ss = QString::number(s);
    if (s < 10) ss = QString("0") + ss;
    return QString::number(m) + ":" + ss;
}

void drawTriangle(QPainter& p, const QRect& r, bool left, const QColor& c)
{
    MuiPolygon tri(3);
    if (left) {
        tri.setPoint(0, r.right(), r.top());
        tri.setPoint(1, r.right(), r.bottom());
        tri.setPoint(2, r.left(), r.center().y());
    } else {
        tri.setPoint(0, r.left(), r.top());
        tri.setPoint(1, r.left(), r.bottom());
        tri.setPoint(2, r.right(), r.center().y());
    }
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(tri);
}
}

PlayerBar::PlayerBar(QWidget* parent)
    : QWidget(parent), m_hasTrack(false), m_playing(false),
      m_progressMs(0), m_durationMs(0), m_repeatMode(0), m_autoSkip(true),
      m_volume(100), m_lastVolume(100),
      m_tip(0),
      m_hover(HitNone), m_dragging(false),
      m_dlSongId(0), m_dlPct(-1), m_spinTick(0),
      m_spinTimer(0), m_volumePopup(0), m_volumeSlider(0), m_muteCheck(0),
      m_lyrics(0)
{
    setMouseTracking(true);
    setFixedHeight(80);
    QObject::connect(&IconLoader::instance(), SIGNAL(iconsReady()),
                     this, SLOT(refreshCover()));
    retranslateUi();
#ifdef QT3_BUILD
    m_tip = new PlayerBarToolTip(this);
#else
    m_tip = 0;
#endif
    m_lyrics = new LyricsStrip(this);
    m_lyrics->setGeometry(lyricsRect());
}

void PlayerBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_lyrics) m_lyrics->setGeometry(lyricsRect());
}

PlayerBar::~PlayerBar()
{
#ifdef QT3_BUILD
    delete (PlayerBarToolTip*)m_tip;
#endif
    delete (QTimer*)m_spinTimer;
    if (m_volumePopup) { delete (QFrame*)m_volumePopup; m_volumePopup = 0; }
    m_volumeSlider = 0;
    m_muteCheck = 0;   /* checkbox 是 popup 子 widget，随父析构；指针同步置零防悬垂 */
}

void PlayerBar::retranslateUi()
{
    m_tipRepeat = _("player.tip_repeat_off");
    m_tipAutoSkip = _("player.tip_autoskip_on");
    update();
}

void PlayerBar::setTrack(const Track& t)
{
    m_track = t;
    m_hasTrack = true;
    m_progressMs = 0;
    m_durationMs = t.durationMs;
    /* 真实封面优先；无图用 identicon。结果进内存 LRU，高频重绘不再重复生成 */
    m_coverPixmap = IconLoader::instance().cover(
        m_track.coverUrl, m_track.coverSeed, 52);
    update();
}

void PlayerBar::refreshCover()
{
    if (!m_hasTrack)
        return;
    m_coverPixmap = IconLoader::instance().cover(
        m_track.coverUrl, m_track.coverSeed, 52);
    update();
}

void PlayerBar::setProgress(int ms)
{
    if (m_dragging) return;
    m_progressMs = ms;
    update();
}

void PlayerBar::setDuration(int ms)
{
    if (ms > 0) m_durationMs = ms;
    update();
}

void PlayerBar::setPlaying(bool on)
{
    if (m_playing == on) return;
    m_playing = on;
    emit playStateChanged(on);
    update();
}

void PlayerBar::setRepeatMode(int mode)
{
    if (mode < 0 || mode > 2) mode = 0;
    if (m_repeatMode == mode) return;
    m_repeatMode = mode;
    /* 三态 tooltip 文案即时生效（qt3 maybeTip / qt5 event 均现读成员） */
    if (m_repeatMode == 1) m_tipRepeat = _("player.tip_repeat_all");
    else if (m_repeatMode == 2) m_tipRepeat = _("player.tip_repeat_one");
    else m_tipRepeat = _("player.tip_repeat_off");
    update();
}

void PlayerBar::setAutoSkip(bool on)
{
    if (m_autoSkip == on) return;
    m_autoSkip = on;
    m_tipAutoSkip = on ? _("player.tip_autoskip_on") : _("player.tip_autoskip_off");
    update();
}

void PlayerBar::setVolume(int v)
{
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (v > 0) m_lastVolume = v;
    if (m_volume == v) return;
    m_volume = v;
    if (m_volumeSlider) {
        /* popup 已建：同步自绘滑条值 + mute checkbox 选中态（blockSignals 防回环） */
        ((VolumePanelSlider*)m_volumeSlider)->setValue(v);
        if (m_muteCheck) {
            QCheckBox* c = (QCheckBox*)m_muteCheck;
            c->blockSignals(true);
            c->setChecked(v == 0);
            c->blockSignals(false);
        }
    }
    /* 局部刷新（右侧 icon 段），拖动流畅 */
    update(QRect(volumeRect().left(), volumeRect().top() - 2,
                 width() - volumeRect().left(), 28));
}

void PlayerBar::setDownloadState(qint64 songId, int pct)
{
    m_dlSongId = songId;
    m_dlPct = pct;
    if (songId <= 0 || pct == 100) {
        stopSpin();
        /* 完成短暂留痕后清盘（QTimer singleShot 1.2s 隐藏 pie） */
        if (songId > 0) {
            setDownloadHiddenLater();
        } else {
            m_dlPct = -1;
        }
        update();
        return;
    }
    if (pct == -1) startSpinIfIndeterminate();   /* 未定态：转圈 */
    else stopSpin();
    update();
}

void PlayerBar::setDownloadHiddenLater()
{
    /* qt3 无 setSingleShot 成员，走 static 重要管线（同 qt5 static 行为） */
    QTimer::singleShot(1200, this, SLOT(onDlHide()));
}

void PlayerBar::onDlHide()
{
    m_dlSongId = 0;
    m_dlPct = -1;
    update();
}

void PlayerBar::startSpinIfIndeterminate()
{
    if (m_spinTimer) return;
    QTimer* t = new QTimer(this);
    m_spinTimer = t;
    QObject::connect(t, SIGNAL(timeout()), this, SLOT(onSpinTick()));
    t->start(90);
}

void PlayerBar::stopSpin()
{
    if (!m_spinTimer) return;
    ((QTimer*)m_spinTimer)->stop();
    delete (QTimer*)m_spinTimer;
    m_spinTimer = 0;
}

void PlayerBar::onSpinTick()
{
    m_spinTick = (m_spinTick + 30) % 360;
    update();
}

/* B2/B3 点击即时反馈：qt3 QToolTip::tip 静态重弹 / qt5 showText，
 * 配合状态栏文案 —— 解决“点了没反应”（Qt3 tooltip popup 吞点击的兜底可见性） */
void PlayerBar::feedbackTip(const QRect& r, const QString& text)
{
    if (text.isEmpty()) return;
#ifdef QT3_BUILD
    /* qt3 tip() 是 protected 成员（QTipManager 挂钩），经子类公开转发弹 */
    if (m_tip) ((PlayerBarToolTip*)m_tip)->showTip(r, text);
#else
    QToolTip::showText(mapToGlobal(QPoint(r.left(), r.bottom() + 2)), text, this);
#endif
}

void PlayerBar::nudgeAutoSkipTip()
{
    feedbackTip(autoSkipRect(), m_tipAutoSkip);
}

int PlayerBar::lyricLeft() const
{
    /* 歌词区左缘下界 cx+170：顶行中央群右缘 pie cx+151 + 19px 间距
     * （progress 缩短 20% 至 208、喇叭/pie 左移贴 repeat 后中央链收窄，
     * 歌词区左移填补右侧空间）；>=1024 窗时达到 330px 设计宽 */
    int cx = width() / 2;
    return qMax(cx + 170, width() - 352);
}

QRect PlayerBar::lyricsRect() const
{
    /* 三行滚动歌词区贴最右（12px 右缘留白）；窄窗动态缩宽让位中央链 */
    int l = lyricLeft();
    return QRect(l, 10, width() - 12 - l, 60);
}

QRect PlayerBar::volumeRect() const
{
    /* 喇叭：repeat 右邻 8px（用户拍板：音量左移靠近循环设定按钮）；
     * 中央群整体左移 30 回正视觉重心（群含喇叭/pie 后原重心偏右） */
    int cx = width() / 2;
    return QRect(cx + 43, 19, 20, 20);
}

QRect PlayerBar::downloadRect() const
{
    /* 下载 pie：喇叭右邻 8px（跟随喇叭左移，顶行图标连排） */
    int cx = width() / 2;
    return QRect(cx + 71, 19, 20, 20);
}

QRect PlayerBar::playRect() const
{
    int cx = width() / 2;
    return QRect(cx - 75, 14, 30, 30);
}

QRect PlayerBar::prevRect() const
{
    int cx = width() / 2;
    return QRect(cx - 120, 19, 20, 20);
}

QRect PlayerBar::nextRect() const
{
    int cx = width() / 2;
    return QRect(cx - 20, 19, 20, 20);
}

QRect PlayerBar::repeatRect() const
{
    int cx = width() / 2;
    return QRect(cx + 15, 19, 20, 20);      /* next 右邻 */
}

QRect PlayerBar::autoSkipRect() const
{
    int cx = width() / 2;
    return QRect(cx - 155, 19, 20, 20);     /* prev 左邻；中央两行整体左移 30（累计） */
}

QRect PlayerBar::tipRectFor(const QPoint& p) const
{
    if (repeatRect().contains(p)) return repeatRect();
    if (autoSkipRect().contains(p)) return autoSkipRect();
    if (volumeRect().contains(p)) return volumeRect();
    if (m_dlSongId > 0 && downloadRect().contains(p)) return downloadRect();
    return QRect();
}

QString PlayerBar::tipForPos(const QPoint& p) const
{
    if (repeatRect().contains(p)) return m_tipRepeat;
    if (autoSkipRect().contains(p)) return m_tipAutoSkip;
    if (volumeRect().contains(p)) {
        if (m_volume == 0) return _("player.tip_muted");
        return _("player.tip_volume").arg(m_volume);
    }
    if (m_dlSongId > 0 && downloadRect().contains(p)) {
        if (m_dlPct == -2) return _("player.dl_failed");
        if (m_dlPct < 0) return _("player.dl_busy");
        return _("player.dl_downloading").arg(m_dlPct);
    }
    return QString();
}

bool PlayerBar::event(QEvent* e)
{
#ifdef QT3_BUILD
    return QWidget::event(e);
#else
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent* he = static_cast<QHelpEvent*>(e);
        QString t = tipForPos(he->pos());
        if (!t.isEmpty()) {
            QToolTip::showText(he->globalPos(), t, this);
            return true;
        }
    }
    return QWidget::event(e);
#endif
}

QRect PlayerBar::progressRect() const
{
    int cx = width() / 2;
    /* 缩短 20%：260 → 208；随中央两行整体左移 30（累计），释放右侧给歌词区 */
    return QRect(cx - 134, 58, 208, 6);
}

void PlayerBar::setProgressFromX(int x)
{
    QRect pr = progressRect();
    if (!m_hasTrack || pr.width() <= 0) return;
    int rel = qBound(0, x - pr.x(), pr.width());
    m_progressMs = (int)((double)rel / pr.width() * m_durationMs);
    update();
}

/* Qt3/Qt5 painter 兼容小工具（Qt3 无 QRect::adjusted/drawPolygon(QPoint*)） */
static QRect qqAdjusted(const QRect& r, int dl, int dt, int dr, int db)
{
#ifdef QT3_BUILD
    return QRect(r.x() + dl, r.y() + dt, r.width() + (dr - dl), r.height() + (db - dt));
#else
    return r.adjusted(dl, dt, dr, db);
#endif
}

void PlayerBar::paintEvent(QPaintEvent*)
{
    const StyleParams::Palette& pal = muiPalette();
    QPainter p(this);
    p.fillRect(rect(), pal.surfaceBg);
    p.setPen(pal.border);
    p.drawLine(0, 0, width(), 0);

    QFont bold = font();
    bold.setBold(true);
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 2));

    if (m_hasTrack) {
        p.drawPixmap(14, 14, m_coverPixmap);
        p.setFont(bold);
        p.setPen(pal.textPrimary);
        p.drawText(QRect(78, 18, width() / 2 - 100, 22), Qt::AlignVCenter | Qt::AlignLeft, m_track.title);
        p.setFont(small);
        p.setPen(pal.textMuted);
        p.drawText(QRect(78, 42, width() / 2 - 100, 20), Qt::AlignVCenter | Qt::AlignLeft, m_track.artist);
    }

    QColor ctrl = (m_hover == HitPlay || m_hover == HitPrev || m_hover == HitNext
                   || m_hover == HitRepeat || m_hover == HitAutoSkip
                   || m_hover == HitVolume || m_hover == HitDownload)
                      ? pal.accent : pal.textPrimary;

    QRect pv = prevRect();
    p.setPen(Qt::NoPen);
    p.setBrush(ctrl);
    p.drawRect(QRect(pv.left(), pv.top(), 3, pv.height()));
    drawTriangle(p, QRect(pv.left() + 5, pv.top(), pv.width() - 5, pv.height()), true, ctrl);

    QRect pl = playRect();
    if (m_playing) {
        p.setBrush(ctrl);
        p.drawRect(QRect(pl.left() + 5, pl.top(), 7, pl.height()));
        p.drawRect(QRect(pl.right() - 12, pl.top(), 7, pl.height()));
    } else {
        drawTriangle(p, QRect(pl.left() + 4, pl.top(), pl.width() - 4, pl.height()), false, ctrl);
    }

    QRect nx = nextRect();
    drawTriangle(p, QRect(nx.left(), nx.top(), nx.width() - 5, nx.height()), false, ctrl);
    p.setBrush(ctrl);
    p.drawRect(QRect(nx.right() - 3, nx.top(), 3, nx.height()));

    /* 失败跳过开关：圆圈 + 勾线（开=accent、关=textMuted） */
    QRect as = autoSkipRect();
    QColor asCol = (m_hover == HitAutoSkip) ? pal.accent
                   : (m_autoSkip ? pal.accent : pal.textMuted);
    p.save();
    p.setPen(QPen(asCol, 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(qqAdjusted(as, 1, 1, -1, -1));
    p.drawLine(as.left() + 5, as.top() + 10, as.left() + 8, as.top() + 14);
    p.drawLine(as.left() + 8, as.top() + 14, as.left() + 15, as.top() + 5);
    p.restore();

    /* 音量喇叭（Material speaker 简笔）：锥形多边形 + 声波弧；0=斜杠音量关闭 */
    QRect vr = volumeRect();
    QColor avCol = (m_hover == HitVolume) ? pal.accent : pal.textPrimary;
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(avCol);
    {
#ifdef QT3_BUILD
        QPointArray sp(6);
#else
        QPolygon sp(6);
#endif
        sp.setPoint(0, vr.left() + 4, vr.top() + 8);
        sp.setPoint(1, vr.left() + 7, vr.top() + 8);
        sp.setPoint(2, vr.left() + 12, vr.top() + 3);
        sp.setPoint(3, vr.left() + 12, vr.top() + 16);
        sp.setPoint(4, vr.left() + 7, vr.top() + 12);
        sp.setPoint(5, vr.left() + 4, vr.top() + 12);
        p.drawPolygon(sp);
    }
    p.setPen(QPen(avCol, 1));
    p.setBrush(Qt::NoBrush);
    if (m_volume > 0) {
        /* 声波弧两条（音量越大越满：0 静音不画声波、画斜杠） */
        p.drawArc(QRect(vr.left() + 10, vr.top() + 7, 8, 8), -45 * 16, 100 * 16);
        if (m_volume >= 50)
            p.drawArc(QRect(vr.left() + 9, vr.top() + 4, 10, 14), -45 * 16, 100 * 16);
    } else {
        p.setPen(QPen(QColor(0xCC, 0x44, 0x44), 2));
        p.drawLine(vr.left() + 3, vr.top() + 17, vr.left() + 18, vr.top() + 3);
    }
    p.restore();

    /* NCM 下载指示（Material pie 圆环：一图标同时表达状态+进度） */
    if (m_dlSongId > 0) {
        QRect dr = downloadRect();
        QColor dlCol = (m_hover == HitDownload) ? pal.accent
                       : (m_dlPct == -2 ? QColor(0xCC, 0x44, 0x44) : pal.textMuted);
        p.save();
        p.setPen(QPen(dlCol, 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(qqAdjusted(dr, 1, 1, -1, -1));
        if (m_dlPct == -2) {
            /* 失败：红斜杠 */
            p.setPen(QPen(dlCol, 2));
            p.drawLine(dr.left() + 4, dr.top() + 16, dr.left() + 16, dr.top() + 4);
        } else if (m_dlPct < 0) {
            /* 未定态（无 Content-Length）：旋转短弧 */
            p.setPen(QPen(pal.accent, 2));
            p.drawArc(qqAdjusted(dr, 3, 3, -3, -3),
                      (90 - m_spinTick) * 16, -100 * 16);
        } else if (m_dlPct > 0) {
            /* 进度扇形（pie，从 12 点顺时针） */
            p.setBrush(pal.accent);
            p.setPen(Qt::NoPen);
            p.drawPie(qqAdjusted(dr, 2, 2, -2, -2), 90 * 16, -m_dlPct * 3600 / 100);
        }
        p.restore();
    }

    /* 循环重复：两段圆弧 + 箭头；one 态中央画 "1"；off=textMuted */
    QRect rp = repeatRect();
    QColor rpCol = (m_hover == HitRepeat) ? pal.accent
                   : (m_repeatMode ? pal.accent : pal.textMuted);
    p.save();
    p.setPen(QPen(rpCol, 1));
    p.setBrush(Qt::NoBrush);
    if (m_repeatMode == 1) {                    /* 列表循环 */
        p.drawArc(qqAdjusted(rp, 1, 1, -1, -1), 30 * 16, 150 * 16);
        p.drawArc(qqAdjusted(rp, 1, 1, -1, -1), 210 * 16, 150 * 16);
        int axp[3] = { rp.right() - 5, rp.right() - 1, rp.right() - 3 };
        int ayp[3] = { rp.top() + 1, rp.top() + 3, rp.top() + 6 };
        int bxp[3] = { rp.left() + 5, rp.left() + 1, rp.left() + 3 };
        int byp[3] = { rp.bottom() - 1, rp.bottom() - 3, rp.bottom() - 6 };
        p.setBrush(rpCol);
#ifdef QT3_BUILD
        QPointArray ap(3);
        ap.setPoint(0, axp[0], ayp[0]);
        ap.setPoint(1, axp[1], ayp[1]);
        ap.setPoint(2, axp[2], ayp[2]);
        p.drawPolygon(ap);
        QPointArray bp(3);
        bp.setPoint(0, bxp[0], byp[0]);
        bp.setPoint(1, bxp[1], byp[1]);
        bp.setPoint(2, bxp[2], byp[2]);
        p.drawPolygon(bp);
#else
        QPoint ap[3] = { QPoint(axp[0], ayp[0]), QPoint(axp[1], ayp[1]),
                         QPoint(axp[2], ayp[2]) };
        p.drawPolygon(ap, 3);
        QPoint bp[3] = { QPoint(bxp[0], byp[0]), QPoint(bxp[1], byp[1]),
                         QPoint(bxp[2], byp[2]) };
        p.drawPolygon(bp, 3);
#endif
    } else if (m_repeatMode == 2) {             /* 单曲重复：圆 + 中央 1 */
        p.drawEllipse(qqAdjusted(rp, 1, 1, -1, -1));
        p.setFont(small);
        p.setPen(rpCol);
        p.drawText(rp, Qt::AlignCenter, QString::fromUtf8("1"));
    } else {
        p.drawEllipse(qqAdjusted(rp, 1, 1, -1, -1));
        p.setFont(small);
        p.setPen(rpCol);
        p.drawText(rp, Qt::AlignCenter, QString::fromUtf8("⟳"));
    }
    p.restore();

    QRect pr = progressRect();
    p.setPen(Qt::NoPen);
    p.setBrush(pal.border);
    muiDrawRoundRect(p, pr, 3, 3);
    int ratio = m_hasTrack && m_durationMs > 0
                    ? (int)((double)m_progressMs / m_durationMs * pr.width()) : 0;
    ratio = qBound(0, ratio, pr.width());
    if (ratio > 0) {
        p.setBrush(pal.accent);
        muiDrawRoundRect(p, QRect(pr.x(), pr.y(), ratio, pr.height()), 3, 3);
    }

    p.setFont(small);
    p.setPen(pal.textMuted);
    p.drawText(QRect(pr.x() - 44, pr.y() - 8, 40, 20), Qt::AlignVCenter | Qt::AlignRight,
               fmtMs(m_progressMs));
    p.drawText(QRect(pr.right() + 4, pr.y() - 8, 44, 20), Qt::AlignVCenter | Qt::AlignLeft,
               fmtMs(m_hasTrack ? m_durationMs : 0));
}

void PlayerBar::mousePressEvent(QMouseEvent* e)
{
    if (playRect().contains(e->pos())) {
        emit playPauseRequested();
    } else if (prevRect().contains(e->pos())) {
        emit prevRequested();
    } else if (nextRect().contains(e->pos())) {
        emit nextRequested();
    } else if (repeatRect().contains(e->pos())) {
        emit repeatClicked();
    } else if (autoSkipRect().contains(e->pos())) {
        emit autoSkipClicked();
    } else if (volumeRect().contains(e->pos())) {
        toggleVolumePopup();   /* 用户拍板：popup 弹滑条（非常驻） */
    } else if (progressRect().contains(e->pos())) {
        m_dragging = true;
        setProgressFromX(e->pos().x());
    }
}

void PlayerBar::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        setProgressFromX(e->pos().x());
        return;
    }
    Hit h = HitNone;
    if (playRect().contains(e->pos())) h = HitPlay;
    else if (prevRect().contains(e->pos())) h = HitPrev;
    else if (nextRect().contains(e->pos())) h = HitNext;
    else if (repeatRect().contains(e->pos())) h = HitRepeat;
    else if (autoSkipRect().contains(e->pos())) h = HitAutoSkip;
    else if (volumeRect().contains(e->pos())) h = HitVolume;
    else if (m_dlSongId > 0 && downloadRect().contains(e->pos())) h = HitDownload;
    else if (progressRect().contains(e->pos())) h = HitProgress;
    if (h != m_hover) {
        m_hover = h;
        update();
    }
}

void PlayerBar::mouseReleaseEvent(QMouseEvent*)
{
    if (m_dragging) {
        m_dragging = false;
        emit seekRequested(m_progressMs);
    }
}

void PlayerBar::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hover != HitNone) {
        m_hover = HitNone;
        update();
    }
}

/* 播放条滚轮=音量 ±5（VLC/Spotify desktop 惯例；scroll 全域接管） */
void PlayerBar::wheelEvent(QWheelEvent* e)
{
    int dy = qWheelDeltaY(e);   /* compat34 shim：qt3/qt4 delta() / qt5 angleDelta().y() */
    if (dy == 0) { e->ignore(); return; }
    int dir = (dy > 0) ? 1 : -1;
    emit volumeChanged(qBound(0, m_volume + dir * 5, 100));
    e->accept();
}

/* 音量弹出滑条（用户拍板：popup，非常驻；icon 置右）。
 * 弹层 = 自绘 VolumePanelSlider（bar 进度条同款鼠标链，零 QSlider 怪癖，
 * click-jump clamp 2..100 防误静音）+ 右侧静音 checkbox（用户拍板：静音
 * 独立控件，Apple Music Win 同款）。容器 VolumePopupFrame 承接 Qt3
 * popup-example 官方 fake release 范式（关弹层归还被抢的 release） */
void PlayerBar::toggleVolumePopup()
{
    if (m_volumePopup) {
        QWidget* pw = (QWidget*)m_volumePopup;
        if (pw->isVisible()) { pw->close(); return; }
    }
    if (!m_volumePopup) {
        /* 首建原子建齐：容器+自绘滑条+静音 checkbox（右侧独立控件防误触） */
        VolumePopupFrame* f = new VolumePopupFrame(this);
        VolumePanelSlider* s = new VolumePanelSlider(this, f);
        f->setPanel(s);
        s->move(8, 9);
        QCheckBox* c = new QCheckBox(_("player.mute"), f);
        c->move(142, 13);
        QObject::connect(c, SIGNAL(toggled(bool)), this, SLOT(onMuteToggled(bool)));
        f->resize(246, 42);   /* 140 滑条区 + 静音 checkbox */
        m_volumePopup = f;
        m_volumeSlider = s;
        m_muteCheck = c;
    }
    VolumePanelSlider* s = (VolumePanelSlider*)m_volumeSlider;
    s->setValue(m_volume);
    QCheckBox* c = (QCheckBox*)m_muteCheck;
    c->blockSignals(true);
    c->setChecked(m_volume == 0);
    c->blockSignals(false);
    QWidget* f = (QWidget*)m_volumePopup;
    /* icon 右对齐：popup 右边贴 icon 右缘向左展开，不越窗 */
    /* 弹层贴喇叭左下（喇叭已在歌词区左邻；向左展开不越右缘） */
    f->move(mapToGlobal(QPoint(volumeRect().left() - 220, volumeRect().bottom() + 4)));
    f->resize(246, 42);
    f->show();
}

/* 自绘 panel 值回调（无 Q_OBJECT 桥）：转原生 Qt 信号，防控拖动延迟走 debounce */
void PlayerBar::onPopupVolume(int v) { emit volumeChanged(v); }

/* 静音 checkbox（用户拍板：静音独立控件防拖动/点击误触）：
 * on = 音量 0（记录静音前值）；off = 回静音前值（无记忆时 50） */
void PlayerBar::onMuteToggled(bool on)
{
    if (on) {
        if (m_volume > 0) m_lastVolume = m_volume;
        emit volumeChanged(0);
    } else {
        emit volumeChanged(m_lastVolume > 0 ? m_lastVolume : 50);
    }
}

/* ===== 三行滚动歌词转发（ lyricsRect 右侧区；主注入点 mainwindow） ===== */
void PlayerBar::setLyricsText(qint64 songTag, const QString& lrcText)
{
    if (m_lyrics) m_lyrics->setLyrics(songTag, lrcText);
}

void PlayerBar::setLyricsPosition(int ms)
{
    if (m_lyrics) m_lyrics->setPositionMs(ms);
}

void PlayerBar::clearLyrics()
{
    if (m_lyrics) m_lyrics->clearLyrics();
}
