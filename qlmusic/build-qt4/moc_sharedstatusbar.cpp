/****************************************************************************
** Meta object code from reading C++ file 'sharedstatusbar.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../doxhttpd/qlcomp/sharedstatusbar.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sharedstatusbar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_SharedStatusBar[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      25,   17,   16,   16, 0x08,
      59,   16,   16,   16, 0x08,
      78,   16,   16,   16, 0x08,
      94,   16,   16,   16, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_SharedStatusBar[] = {
    "SharedStatusBar\0\0old,now\0"
    "onFocusChanged(QWidget*,QWidget*)\0"
    "onHistoryClicked()\0onIconTimeout()\0"
    "retrack()\0"
};

void SharedStatusBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        SharedStatusBar *_t = static_cast<SharedStatusBar *>(_o);
        switch (_id) {
        case 0: _t->onFocusChanged((*reinterpret_cast< QWidget*(*)>(_a[1])),(*reinterpret_cast< QWidget*(*)>(_a[2]))); break;
        case 1: _t->onHistoryClicked(); break;
        case 2: _t->onIconTimeout(); break;
        case 3: _t->retrack(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData SharedStatusBar::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject SharedStatusBar::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SharedStatusBar,
      qt_meta_data_SharedStatusBar, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &SharedStatusBar::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *SharedStatusBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *SharedStatusBar::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_SharedStatusBar))
        return static_cast<void*>(const_cast< SharedStatusBar*>(this));
    return QWidget::qt_metacast(_clname);
}

int SharedStatusBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
