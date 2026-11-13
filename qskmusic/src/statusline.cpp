#include "statusline.h"
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskSizePolicy.h>
#include <QColor>

StatusLine::StatusLine(QQuickItem* parent)
    : QskLinearBox(Qt::Horizontal, parent)
{
    setSpacing(6);
    setPreferredHeight(28);
    setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);

    m_icon = new QskTextLabel(this);
    m_icon->setPreferredWidth(22);
    m_icon->setAlignment(Qt::AlignCenter);

    m_text = new QskTextLabel(this);
    m_text->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_text->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_retry = new QskPushButton(tr("Retry"), this);
    m_retry->setPreferredHeight(26);
    connect(m_retry, &QskAbstractButton::clicked,
            this, &StatusLine::retryClicked);

    setStatus(Hidden, QString());
}

void StatusLine::setStatus(Level level, const QString& text)
{
    m_level = level;

    const bool show = (level != Hidden) && !text.isEmpty();
    setVisible(show);
    if (!show)
        return;

    const QColor gray(0x8c, 0x8c, 0x8c);
    const QColor yellow(0xd4, 0xa0, 0x17);
    const QColor red(0xc0, 0x39, 0x2b);
    QColor color = gray;
    QString icon;

    switch (level) {
    case Loading: icon = QString::fromUtf8("⏳"); break;
    case Info:    icon = QString::fromUtf8("ℹ️"); break;
    case Warn:    icon = QString::fromUtf8("⚠️"); color = yellow; break;
    case Error:   icon = QString::fromUtf8("⛔"); color = red; break;
    default: break;
    }

    m_icon->setText(icon);
    m_text->setText(text);
    m_text->setTextColor(color);
    m_retry->setVisible(level == Error);
}
