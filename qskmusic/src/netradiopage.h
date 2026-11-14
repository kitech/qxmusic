#ifndef NETRADIO_PAGE_H
#define NETRADIO_PAGE_H

#include "page.h"

class QskLinearBox;
class QskTextLabel;

/*
 * 网络电台页（内置固定清单，无请求；musiccom/radiolist 数据源）：
 *   launchArgs 空          → 地区分类列表（中国大陆/港台/亚洲/国际）
 *   launchArgs {region,title} → 该地区电台行列表（直连流可播，HLS 灰态）
 * 播放走 AudioPlayer::playUrl（独立于 NCM 歌单队列）；HLS(m3u8) 暂不支持，
 * 行后缀 "· HLS" 灰态并提示，为后续 HLS 支持留位。
 */
class NetRadioPage : public Page
{
    Q_OBJECT
public:
    explicit NetRadioPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    QskLinearBox* m_layout = nullptr;
};

#endif
