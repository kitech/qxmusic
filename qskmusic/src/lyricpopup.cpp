#include "lyricpopup.h"
#include <QskBox.h>
#include <QskLinearBox.h>
#include <QskScrollArea.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskBoxShapeMetrics.h>
#include <QskGradient.h>
#include <QskFontRole.h>
#include <QQuickWindow>
#include <QColor>
#include <QPair>
#include <QStringList>
#include <algorithm>

namespace {

// 与 qskcomp/dialogpopup 相同范式：坐标基准 = 父项所在窗口的尺寸
QRectF parentRectOf(QQuickItem* parent)
{
    if (!parent)
        return {};
    if (auto* w = parent->window())
        return QRectF(QPointF(), w->size());
    return QRectF(-parent->x(), -parent->y(), parent->width(), parent->height());
}

// 面板不透明度：只改皮肤填充色的 alpha（保留主题配色）
void applyPanelOpacity(QskBox* panel, qreal opacity)
{
    if (!panel)
        return;
    QskGradient g = panel->fillGradient();
    if (!g.isValid())
        return;
    g.setAlpha(qRound(qBound(0.0, opacity, 1.0) * 255.0));
    panel->setFillGradient(g);
}

// 解析行首所有 [mm:ss.xx] / [mm:ss] 标签（多标签行返回多个时间），返回正文
QString takeTags(const QString& line, QList<int>& times)
{
    QString rest = line;
    while (rest.startsWith(QLatin1Char('['))) {
        const int close = rest.indexOf(QLatin1Char(']'));
        if (close < 0) break;
        const QString tag = rest.mid(1, close - 1);
        const auto parts = tag.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            bool ok1 = false, ok2 = false;
            const int mm = parts.at(0).toInt(&ok1);
            const double ss = parts.at(1).toDouble(&ok2);
            if (ok1 && ok2)
                times.append(mm * 60000 + int(ss * 1000));
        }
        rest = rest.mid(close + 1);
    }
    return rest.trimmed();
}

} // namespace

LyricPopup::LyricPopup(QQuickItem* parent)
    : QskPopup(parent)
{
    setOverlay(true);
    setPolishOnResize(true);
    setPolishOnParentResize(true);

    m_panel = new QskBox(this);
    m_panel->setBoxShapeHint(QskBox::Panel, QskBoxShapeMetrics(14, Qt::AbsoluteSize));
    applyPanelOpacity(m_panel, 0.92);   // 消除弹层过透（dialogpopup 同款）

    m_layout = new QskLinearBox(Qt::Vertical, m_panel);
    m_layout->setMargins(18);
    m_layout->setSpacing(10);

    // 左上歌词图标（emoji 字符，绝对定位见 updateGeometry）
    m_icon = new QskTextLabel(QString::fromUtf8("📃"), m_panel);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));

    // 右上关闭（明确出口）
    m_closeBtn = new QskPushButton(QString::fromUtf8("✕"), m_panel);
    connect(m_closeBtn, &QskAbstractButton::clicked, this, [this]() { close(); });

    m_titleLabel = new QskTextLabel(m_layout);
    m_titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    m_scroll = new QskScrollArea(m_layout);
    m_scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_linesBox = new QskLinearBox(Qt::Vertical);
    m_linesBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    m_scroll->setScrolledItem(m_linesBox);
}

void LyricPopup::setLyric(const QString& title, const QString& lrcText)
{
    m_titleLabel->setText(title);
    m_currentLine = -1;
    parseAndBuild(lrcText);
}

void LyricPopup::parseAndBuild(const QString& lrcText)
{
    const auto children = m_linesBox->childItems();
    for (auto* c : children) c->deleteLater();
    m_lines.clear();

    const QString text = lrcText.trimmed();
    if (text.isEmpty()) {
        // 空态：无歌词（不再永远"加载中"）
        auto* lbl = new QskTextLabel(QString::fromUtf8("暂无歌词"), m_linesBox);
        lbl->setPreferredHeight(40);
        lbl->setAlignment(Qt::AlignCenter);
        m_linesBox->update();
        return;
    }

    QList<QPair<int, QString>> timed;
    QStringList plain;
    const auto rawLines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto& raw : rawLines) {
        QList<int> times;
        const QString body = takeTags(raw, times);
        if (body.isEmpty())
            continue;
        if (times.isEmpty()) {
            plain << body;                 // 无时间戳行（纯文本歌词）
        } else {
            for (int t : times)
                timed.append({ t, body });
        }
    }
    std::sort(timed.begin(), timed.end(),
        [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
            return a.first < b.first;
        });

    for (const auto& p : timed) {
        auto* lbl = new QskTextLabel(p.second, m_linesBox);
        lbl->setPreferredHeight(34);
        lbl->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        m_lines.append({ p.first, lbl });
    }
    for (const auto& s : plain) {
        auto* lbl = new QskTextLabel(s, m_linesBox);
        lbl->setPreferredHeight(34);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    }
    m_linesBox->update();
}

void LyricPopup::setProgressMs(int ms)
{
    if (!isOpen() || m_lines.isEmpty())
        return;

    int idx = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i).startMs <= ms)
            idx = i;
        else
            break;
    }
    setCurrentLine(idx);
}

void LyricPopup::setCurrentLine(int idx)
{
    if (idx == m_currentLine)
        return;
    m_currentLine = idx;

    for (int i = 0; i < m_lines.size(); ++i) {
        auto* lbl = m_lines.at(i).label;
        lbl->setTextColor(i == idx ? QColor(0x4c, 0xaf, 0x50)    // 当前行：绿
                                   : QColor(0x8c, 0x8c, 0x8c));  // 其余：灰
    }
    if (idx >= 0 && m_scroll) {
        const auto& line = m_lines.at(idx);
        const qreal y = line.label->y() - m_scroll->height() / 2 + 17;
        m_scroll->scrollTo(QPointF(0, qMax<qreal>(0, y)));
    }
}

void LyricPopup::updateLayout()
{
    updateGeometry();
    if (m_layout && m_panel)
        m_layout->setGeometry(QRectF(0, 0, m_panel->width(), m_panel->height()));
}

void LyricPopup::updateGeometry()
{
    const auto parentRect = parentRectOf(parentItem());
    if (parentRect.isEmpty())
        return;

    const qreal w = qBound<qreal>(280, parentRect.width() * 0.86, 520);
    const qreal h = qMin(parentRect.height() * 0.6, parentRect.height() * 0.9);
    QRectF popupRect(0, 0, w, h);
    popupRect.moveCenter(parentRect.center());
    setGeometry(popupRect);                    // 关键：弹层自身定位（此前缺失）
    m_panel->setGeometry(QRectF(0, 0, w, h));  // 面板填满弹层

    // 左上图标 / 右上关闭按钮（面板内绝对定位）
    m_icon->setGeometry(QRectF(10, 8, 22, 22));
    m_closeBtn->setGeometry(QRectF(w - 38, 6, 30, 30));
}
