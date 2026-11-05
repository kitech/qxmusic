#ifndef QUEUE_POPUP_H
#define QUEUE_POPUP_H

#include <QskPopup.h>
#include <QStringList>

class QskBox;
class QskLinearBox;

/*
 * 播放队列弹层初版（menuoverlay 模式简化版，桩列表）。
 * 展示当前队列歌曲行；选中行发 trackSelected(index) 由宿主处理。
 */
class QueuePopup : public QskPopup
{
    Q_OBJECT
public:
    explicit QueuePopup(QQuickItem* parent = nullptr);

    void setTracks(const QStringList& titles);
    void setCurrentIndex(int idx);

Q_SIGNALS:
    void trackSelected(int index);

protected:
    void updateLayout() override;

private:
    void updateGeometry();
    void rebuildRows();

    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    QskLinearBox* m_rowsBox = nullptr;
    QStringList m_titles;
    int m_currentIndex = -1;
};

#endif
