#ifndef COMMENTPOPUP_H
#define COMMENTPOPUP_H

#include "compat34.h"

class QLabel;
class QTextEdit;

/* 评论弹层：Qt3 WType_Popup / Qt4 Qt::Popup（点外自动关，仿音量 popup）。
 * 内容由 MainWindow 解析为纯文本喂入，避免用户评论里的 HTML 注入。 */
class CommentPopup : public QWidget {
public:
    explicit CommentPopup(QWidget* parent = 0);
    void setComments(const QString& title, const QString& text);

private:
    QLabel* m_title;
    QTextEdit* m_view;
};

#endif // COMMENTPOPUP_H
