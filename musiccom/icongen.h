#ifndef MUSICCOM_ICONGEN_H
#define MUSICCOM_ICONGEN_H

#include <qstring.h>
#include <qpixmap.h>

/*
 * 无网络图时的兜底图标（双版本共享，视觉风格一致）：
 *   identicon    — 专辑/歌单无封面（5×5 对称图案，FNV-1a 派生，无加密依赖）
 *   letterAvatar — 歌手头像（首字母，可圆形）
 *   categoryIcon — 分类图标（首字圆角色块）
 */
namespace qximage {

QPixmap identicon(const QString& seed, int size);
QPixmap letterAvatar(const QString& name, int size, bool circular);
QPixmap categoryIcon(const QString& name, int size);

} // namespace qximage

#endif // MUSICCOM_ICONGEN_H
