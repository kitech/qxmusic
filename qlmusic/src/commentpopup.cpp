#include "commentpopup.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qtextedit.h>

CommentPopup::CommentPopup(QWidget* parent)
#ifdef QT3_BUILD
    : QWidget(parent, 0, Qt::WType_Popup)
#else
    : QWidget(parent, Qt::Popup)
#endif
    , m_title(0), m_view(0)
{
    qSetWindowTitle(this, QString::fromUtf8("评论"));
    QBoxLayout* lay = qNewBoxLayout(this, QBoxLayout::TopToBottom, 6);

    m_title = new QLabel(this);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    lay->addWidget(m_title);

    m_view = new QTextEdit(this);
    m_view->setReadOnly(true);
    qTextEditWrapNoHScroll(m_view);
    lay->addWidget(m_view);

    resize(460, 520);
}

void CommentPopup::setComments(const QString& title, const QString& text)
{
    m_title->setText(title);
    qTextEditSetText(m_view, text);
}
