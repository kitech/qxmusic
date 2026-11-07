#ifndef LYRIC_POPUP_H
#define LYRIC_POPUP_H

#include <QskPopup.h>

class QskBox;
class QskLinearBox;
class QskScrollArea;
class QskTextLabel;

/*
 * 歌词弹层（dialogpopup 面板范式）：半屏面板 + 标题 + LRC 滚动文本。
 * 本轮纯文本展示（[mm:ss.xx] 标签剥离）；逐行时间高亮下轮。
 */
class LyricPopup : public QskPopup
{
    Q_OBJECT
public:
    explicit LyricPopup(QQuickItem* parent = nullptr);

    void setLyric(const QString& title, const QString& lrcText);

protected:
    void updateLayout() override;

private:
    void updateGeometry();
    void rebuildLines(const QString& lrcText);

    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    QskTextLabel* m_titleLabel = nullptr;
    QskScrollArea* m_scroll = nullptr;
    QskLinearBox* m_linesBox = nullptr;
};

#endif
