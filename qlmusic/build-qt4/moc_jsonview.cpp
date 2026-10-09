/****************************************************************************
** Meta object code from reading C++ file 'jsonview.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../doxhttpd/qlcomp/jsonview.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'jsonview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_JsonViewWidget[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      16,   15,   15,   15, 0x0a,
      28,   15,   15,   15, 0x0a,
      42,   15,   15,   15, 0x08,
      52,   15,   15,   15, 0x08,
      63,   15,   15,   15, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_JsonViewWidget[] = {
    "JsonViewWidget\0\0expandAll()\0collapseAll()\0"
    "onPaste()\0onToggle()\0onToggleRaw()\0"
};

void JsonViewWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        JsonViewWidget *_t = static_cast<JsonViewWidget *>(_o);
        switch (_id) {
        case 0: _t->expandAll(); break;
        case 1: _t->collapseAll(); break;
        case 2: _t->onPaste(); break;
        case 3: _t->onToggle(); break;
        case 4: _t->onToggleRaw(); break;
        default: ;
        }
    }
    Q_UNUSED(_a);
}

const QMetaObjectExtraData JsonViewWidget::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject JsonViewWidget::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_JsonViewWidget,
      qt_meta_data_JsonViewWidget, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &JsonViewWidget::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *JsonViewWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *JsonViewWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_JsonViewWidget))
        return static_cast<void*>(const_cast< JsonViewWidget*>(this));
    return QWidget::qt_metacast(_clname);
}

int JsonViewWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
