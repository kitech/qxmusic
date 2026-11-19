#ifndef COMMENTPAGE_H
#define COMMENTPAGE_H

#include "page.h"
#include "ncmapi.h"

class QskLinearBox;
class QskTextLabel;

/*
 * 评论页（qlmusic 评论弹层的 qsk 等价物）：
 *   launchArgs: {"songId": qint64（网易云 song / B站 aid）, "title": QString}
 *   内容：总数 + 首页 20 条（谁发的 / 绝对日期 / ▷赞 / 💬回复数 / 正文）
 *   返回：页头 ←（finish() → PageManager::back）
 *   注册：Transient + Standard（见 main.cpp comments 注册）
 */
class CommentPage : public Page
{
    Q_OBJECT
public:
    explicit CommentPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onDestroy() override;

private:
    void buildRows();

    qint64 m_songId = 0;
    QString m_title;
    QList<Comment> m_comments;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_status = nullptr;
    bool m_destroyed = false;
};

#endif // COMMENTPAGE_H
