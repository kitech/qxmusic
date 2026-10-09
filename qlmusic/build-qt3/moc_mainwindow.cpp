/****************************************************************************
** MainWindow meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../src/mainwindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *MainWindow::className() const
{
    return "MainWindow";
}

QMetaObject *MainWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_MainWindow( "MainWindow", &MainWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString MainWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MainWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString MainWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "MainWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* MainWindow::staticMetaObject()
{
    if ( metaObj ) {
	return metaObj;
}
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->lock();
    if ( metaObj ) {
	if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
	return metaObj;
    }
#endif // QT_THREAD_SUPPORT
    QMetaObject* parentObject = QMainWindow::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ "index", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"onTitleUilang", 1, param_slot_0 };
    static const QUParameter param_slot_1[] = {
	{ "index", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_1 = {"onTitleStyle", 1, param_slot_1 };
    static const QUParameter param_slot_2[] = {
	{ "on", &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_2 = {"onTitleDark", 1, param_slot_2 };
    static const QUMethod slot_3 = {"onDarkMenuItem", 0, 0 };
    static const QUMethod slot_4 = {"retranslateUi", 0, 0 };
    static const QUParameter param_slot_5[] = {
	{ "reason", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_5 = {"trayActivated", 1, param_slot_5 };
    static const QUMethod slot_6 = {"trayShowMainWindow", 0, 0 };
    static const QUMethod slot_7 = {"quitApp", 0, 0 };
    static const QUMethod slot_8 = {"onMenu1Stub", 0, 0 };
    static const QUMethod slot_9 = {"onDemoStatusInfo", 0, 0 };
    static const QUMethod slot_10 = {"onDemoStatusWarning", 0, 0 };
    static const QUMethod slot_11 = {"onDemoStatusError", 0, 0 };
    static const QUMethod slot_12 = {"onDemoStatusClear", 0, 0 };
    static const QUMethod slot_13 = {"onDemoTrayBubble", 0, 0 };
    static const QUMethod slot_14 = {"onDemoToggleStatusWidgets", 0, 0 };
    static const QUMethod slot_15 = {"onDemoApiRequest", 0, 0 };
    static const QUMethod slot_16 = {"onAboutApp", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onTitleUilang(int)", &slot_0, QMetaData::Private },
	{ "onTitleStyle(int)", &slot_1, QMetaData::Private },
	{ "onTitleDark(bool)", &slot_2, QMetaData::Private },
	{ "onDarkMenuItem()", &slot_3, QMetaData::Private },
	{ "retranslateUi()", &slot_4, QMetaData::Private },
	{ "trayActivated(int)", &slot_5, QMetaData::Private },
	{ "trayShowMainWindow()", &slot_6, QMetaData::Private },
	{ "quitApp()", &slot_7, QMetaData::Private },
	{ "onMenu1Stub()", &slot_8, QMetaData::Private },
	{ "onDemoStatusInfo()", &slot_9, QMetaData::Private },
	{ "onDemoStatusWarning()", &slot_10, QMetaData::Private },
	{ "onDemoStatusError()", &slot_11, QMetaData::Private },
	{ "onDemoStatusClear()", &slot_12, QMetaData::Private },
	{ "onDemoTrayBubble()", &slot_13, QMetaData::Private },
	{ "onDemoToggleStatusWidgets()", &slot_14, QMetaData::Private },
	{ "onDemoApiRequest()", &slot_15, QMetaData::Private },
	{ "onAboutApp()", &slot_16, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"MainWindow", parentObject,
	slot_tbl, 17,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_MainWindow.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* MainWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "MainWindow" ) )
	return this;
    return QMainWindow::qt_cast( clname );
}

bool MainWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onTitleUilang((int)static_QUType_int.get(_o+1)); break;
    case 1: onTitleStyle((int)static_QUType_int.get(_o+1)); break;
    case 2: onTitleDark((bool)static_QUType_bool.get(_o+1)); break;
    case 3: onDarkMenuItem(); break;
    case 4: retranslateUi(); break;
    case 5: trayActivated((int)static_QUType_int.get(_o+1)); break;
    case 6: trayShowMainWindow(); break;
    case 7: quitApp(); break;
    case 8: onMenu1Stub(); break;
    case 9: onDemoStatusInfo(); break;
    case 10: onDemoStatusWarning(); break;
    case 11: onDemoStatusError(); break;
    case 12: onDemoStatusClear(); break;
    case 13: onDemoTrayBubble(); break;
    case 14: onDemoToggleStatusWidgets(); break;
    case 15: onDemoApiRequest(); break;
    case 16: onAboutApp(); break;
    default:
	return QMainWindow::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool MainWindow::qt_emit( int _id, QUObject* _o )
{
    return QMainWindow::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool MainWindow::qt_property( int id, int f, QVariant* v)
{
    return QMainWindow::qt_property( id, f, v);
}

bool MainWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
