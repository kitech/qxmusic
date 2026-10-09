/****************************************************************************
** Meta object code from reading C++ file 'screenshotpreview.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../doxhttpd/qlcomp/screenshotpreview.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'screenshotpreview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ScreenshotPreviewDialog[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: signature, parameters, type, tag, flags
      34,   25,   24,   24, 0x05,
      57,   25,   24,   24, 0x05,
      80,   24,   24,   24, 0x05,

 // slots: signature, parameters, type, tag, flags
      92,   24,   24,   24, 0x08,
     108,   24,   24,   24, 0x08,
     124,   24,   24,   24, 0x08,
     140,   24,   24,   24, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_ScreenshotPreviewDialog[] = {
    "ScreenshotPreviewDialog\0\0filePath\0"
    "sendRequested(QString)\0saveRequested(QString)\0"
    "cancelled()\0onSendClicked()\0onCopyClicked()\0"
    "onSaveClicked()\0onCancelClicked()\0"
};

void ScreenshotPreviewDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        ScreenshotPreviewDialog *_t = static_cast<ScreenshotPreviewDialog *>(_o);
        switch (_id) {
        case 0: _t->sendRequested((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->saveRequested((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->cancelled(); break;
        case 3: _t->onSendClicked(); break;
        case 4: _t->onCopyClicked(); break;
        case 5: _t->onSaveClicked(); break;
        case 6: _t->onCancelClicked(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData ScreenshotPreviewDialog::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject ScreenshotPreviewDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_ScreenshotPreviewDialog,
      qt_meta_data_ScreenshotPreviewDialog, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &ScreenshotPreviewDialog::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *ScreenshotPreviewDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *ScreenshotPreviewDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ScreenshotPreviewDialog))
        return static_cast<void*>(const_cast< ScreenshotPreviewDialog*>(this));
    return QDialog::qt_metacast(_clname);
}

int ScreenshotPreviewDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void ScreenshotPreviewDialog::sendRequested(const QString & _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ScreenshotPreviewDialog::saveRequested(const QString & _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ScreenshotPreviewDialog::cancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 2, 0);
}
QT_END_MOC_NAMESPACE
