#ifndef PROBE_PAGE_H
#define PROBE_PAGE_H

#include "page.h"

/*
 * DIAG 探针页（临时）：只有一行文本，用来区分"开任意页都崩"
 * （导航/转场层）与"特定页面内容崩"。定位后删除。
 */
class ProbePage : public Page
{
    Q_OBJECT
public:
    explicit ProbePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
};

#endif
