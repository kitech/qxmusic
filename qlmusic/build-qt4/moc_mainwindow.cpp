/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/mainwindow.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_MainWindow[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      18,   12,   11,   11, 0x08,
      37,   12,   11,   11, 0x08,
      58,   55,   11,   11, 0x08,
      76,   11,   11,   11, 0x08,
      93,   11,   11,   11, 0x08,
     116,  109,   11,   11, 0x08,
     135,   11,   11,   11, 0x08,
     156,   11,   11,   11, 0x08,
     166,   11,   11,   11, 0x08,
     180,   11,   11,   11, 0x08,
     199,   11,   11,   11, 0x08,
     221,   11,   11,   11, 0x08,
     241,   11,   11,   11, 0x08,
     261,   11,   11,   11, 0x08,
     280,   11,   11,   11, 0x08,
     308,   11,   11,   11, 0x08,
     327,   11,   11,   11, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_MainWindow[] = {
    "MainWindow\0\0index\0onTitleUilang(int)\0"
    "onTitleStyle(int)\0on\0onTitleDark(bool)\0"
    "onDarkMenuItem()\0retranslateUi()\0"
    "reason\0trayActivated(int)\0"
    "trayShowMainWindow()\0quitApp()\0"
    "onMenu1Stub()\0onDemoStatusInfo()\0"
    "onDemoStatusWarning()\0onDemoStatusError()\0"
    "onDemoStatusClear()\0onDemoTrayBubble()\0"
    "onDemoToggleStatusWidgets()\0"
    "onDemoApiRequest()\0onAboutApp()\0"
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        MainWindow *_t = static_cast<MainWindow *>(_o);
        switch (_id) {
        case 0: _t->onTitleUilang((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->onTitleStyle((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onTitleDark((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->onDarkMenuItem(); break;
        case 4: _t->retranslateUi(); break;
        case 5: _t->trayActivated((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->trayShowMainWindow(); break;
        case 7: _t->quitApp(); break;
        case 8: _t->onMenu1Stub(); break;
        case 9: _t->onDemoStatusInfo(); break;
        case 10: _t->onDemoStatusWarning(); break;
        case 11: _t->onDemoStatusError(); break;
        case 12: _t->onDemoStatusClear(); break;
        case 13: _t->onDemoTrayBubble(); break;
        case 14: _t->onDemoToggleStatusWidgets(); break;
        case 15: _t->onDemoApiRequest(); break;
        case 16: _t->onAboutApp(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData MainWindow::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject MainWindow::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_MainWindow,
      qt_meta_data_MainWindow, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &MainWindow::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow))
        return static_cast<void*>(const_cast< MainWindow*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
