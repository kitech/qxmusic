#include "probepage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>

ProbePage::ProbePage(QQuickItem* parent)
    : Page(parent)
{
}

void ProbePage::onCreate(const QVariantMap&, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);
    auto* lbl = new QskTextLabel(QString::fromUtf8("PROBE PAGE OK"), layout);
    lbl->setPreferredHeight(48);
    lbl->setAlignment(Qt::AlignCenter);
}
