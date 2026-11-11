#ifndef LYRIC_POPUP_H
#define LYRIC_POPUP_H

#include <QskPopup.h>
#include <QList>

class QskBox;
class QskLinearBox;
class QskScrollArea;
class QskTextLabel;
class QskPushButton;

/*
 * 歌词弹层（dialogpopup 面板范式）：居中面板 + 左上「词」徽标 + 右上关闭
 * + 标题 + LRC 时间轴歌词。
 *   几何基准 = 父项所在窗口尺寸（dialogpopup 同范式），面板居中；
 *   解析 [mm:ss.xx]（多标签展开/按时间排序）；
 *   setProgressMs() 当前行高亮（绿）+ 自动滚动居中；
 *   空歌词显示「暂无歌词」（不再永远"加载中"）。
 */
class LyricPopup : public QskPopup
{
    Q_OBJECT
public:
    explicit LyricPopup(QQuickItem* parent = nullptr);

    void setLyric(const QString& title, const QString& lrcText);
    void setProgressMs(int ms);      // 未打开/无时间轴时无操作
    void relayout() { updateLayout(); }   // 打开前/后补布局（main.cpp 调用）

protected:
    void updateLayout() override;

private:
    struct LyricLine { int startMs = 0; QskTextLabel* label = nullptr; };

    void updateGeometry();
    void parseAndBuild(const QString& lrcText);
    void setCurrentLine(int idx);

    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    QskTextLabel* m_icon = nullptr;       // 左上「词」徽标
    QskPushButton* m_closeBtn = nullptr;  // 右上关闭
    QskTextLabel* m_titleLabel = nullptr;
    QskScrollArea* m_scroll = nullptr;
    QskLinearBox* m_linesBox = nullptr;
    QList<LyricLine> m_lines;        // 仅含带时间戳的行
    int m_currentLine = -1;
};

#endif
