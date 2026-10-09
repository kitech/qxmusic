/****************************************************************************
** Meta object code from reading C++ file 'screenshotmanager.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../doxhttpd/qlcomp/screenshotmanager.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'screenshotmanager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ScreenshotManager[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: signature, parameters, type, tag, flags
      28,   19,   18,   18, 0x05,
      53,   18,   18,   18, 0x05,

 // slots: signature, parameters, type, tag, flags
      65,   18,   18,   18, 0x08,
      87,   18,   18,   18, 0x08,
     111,   18,   18,   18, 0x08,
     148,  132,   18,   18, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_ScreenshotManager[] = {
    "ScreenshotManager\0\0filePath\0"
    "screenshotReady(QString)\0cancelled()\0"
    "doCaptureFullScreen()\0doCaptureActiveWindow()\0"
    "doCaptureForRegion()\0rect,fullPixmap\0"
    "onRegionSelected(QRect,QPixmap)\0"
};

void ScreenshotManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        ScreenshotManager *_t = static_cast<ScreenshotManager *>(_o);
        switch (_id) {
        case 0: _t->screenshotReady((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->cancelled(); break;
        case 2: _t->doCaptureFullScreen(); break;
        case 3: _t->doCaptureActiveWindow(); break;
        case 4: _t->doCaptureForRegion(); break;
        case 5: _t->onRegionSelected((*reinterpret_cast< const QRect(*)>(_a[1])),(*reinterpret_cast< const QPixmap(*)>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData ScreenshotManager::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject ScreenshotManager::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_ScreenshotManager,
      qt_meta_data_ScreenshotManager, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &ScreenshotManager::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *ScreenshotManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *ScreenshotManager::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ScreenshotManager))
        return static_cast<void*>(const_cast< ScreenshotManager*>(this));
    return QObject::qt_metacast(_clname);
}

int ScreenshotManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void ScreenshotManager::screenshotReady(const QString & _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ScreenshotManager::cancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 1, 0);
}
QT_END_MOC_NAMESPACE
