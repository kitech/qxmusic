/****************************************************************************
** Meta object code from reading C++ file 'screenshotoverlay.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../doxhttpd/qlcomp/screenshotoverlay.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'screenshotoverlay.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ScreenshotRegionSelector[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: signature, parameters, type, tag, flags
      42,   26,   25,   25, 0x05,
      72,   25,   25,   25, 0x05,

       0        // eod
};

static const char qt_meta_stringdata_ScreenshotRegionSelector[] = {
    "ScreenshotRegionSelector\0\0rect,fullPixmap\0"
    "regionSelected(QRect,QPixmap)\0cancelled()\0"
};

void ScreenshotRegionSelector::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        ScreenshotRegionSelector *_t = static_cast<ScreenshotRegionSelector *>(_o);
        switch (_id) {
        case 0: _t->regionSelected((*reinterpret_cast< const QRect(*)>(_a[1])),(*reinterpret_cast< const QPixmap(*)>(_a[2]))); break;
        case 1: _t->cancelled(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData ScreenshotRegionSelector::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject ScreenshotRegionSelector::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_ScreenshotRegionSelector,
      qt_meta_data_ScreenshotRegionSelector, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &ScreenshotRegionSelector::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *ScreenshotRegionSelector::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *ScreenshotRegionSelector::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ScreenshotRegionSelector))
        return static_cast<void*>(const_cast< ScreenshotRegionSelector*>(this));
    return QWidget::qt_metacast(_clname);
}

int ScreenshotRegionSelector::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ScreenshotRegionSelector::regionSelected(const QRect & _t1, const QPixmap & _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ScreenshotRegionSelector::cancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 1, 0);
}
QT_END_MOC_NAMESPACE
