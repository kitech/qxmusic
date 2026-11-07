#include "lyricpopup.h"
#include <QskBox.h>
#include <QskLinearBox.h>
#include <QskScrollArea.h>
#include <QskTextLabel.h>
#include <QskBoxShapeMetrics.h>
#include <QQuickWindow>

namespace {
// 剥离行首 [xx:xx.xx] 类时间标签，取最后一个 ']' 之后的正文
QString stripTags(const QString& line)
{
    if (!line.startsWith(QLatin1Char('[')))
        return line;
    const int pos = line.lastIndexOf(QLatin1Char(']'));
    return pos >= 0 ? line.mid(pos + 1) : line;
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

    m_layout = new QskLinearBox(Qt::Vertical, m_panel);
    m_layout->setMargins(18);
    m_layout->setSpacing(10);

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
    rebuildLines(lrcText);
}

void LyricPopup::rebuildLines(const QString& lrcText)
{
    const auto children = m_linesBox->childItems();
    for (auto* c : children) c->deleteLater();

    const auto lines = lrcText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto& raw : lines) {
        const QString text = stripTags(raw).trimmed();
        if (text.isEmpty())
            continue;
        auto* lbl = new QskTextLabel(text, m_linesBox);
        lbl->setPreferredHeight(34);
        lbl->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    }
    m_linesBox->update();
}

void LyricPopup::updateLayout()
{
    updateGeometry();
}

void LyricPopup::updateGeometry()
{
    const auto rect = this->rect();
    if (rect.isEmpty())
        return;

    const qreal w = qBound<qreal>(280, rect.width() * 0.86, 520);
    const qreal h = rect.height() * 0.6;
    m_panel->setGeometry((rect.width() - w) / 2, (rect.height() - h) / 2, w, h);
    m_layout->setGeometry(QRectF(0, 0, w, h));
}
