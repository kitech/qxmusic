/****************************************************************************
** ScreenshotManager meta object code from reading C++ file 'screenshotmanager.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/screenshotmanager.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *ScreenshotManager::className() const
{
    return "ScreenshotManager";
}

QMetaObject *ScreenshotManager::metaObj = 0;
static QMetaObjectCleanUp cleanUp_ScreenshotManager( "ScreenshotManager", &ScreenshotManager::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString ScreenshotManager::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ScreenshotManager", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString ScreenshotManager::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ScreenshotManager", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* ScreenshotManager::staticMetaObject()
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
    QMetaObject* parentObject = QObject::staticMetaObject();
    static const QUMethod slot_0 = {"doCaptureFullScreen", 0, 0 };
    static const QUMethod slot_1 = {"doCaptureActiveWindow", 0, 0 };
    static const QUMethod slot_2 = {"doCaptureForRegion", 0, 0 };
    static const QUParameter param_slot_3[] = {
	{ "rect", &static_QUType_varptr, "\x08", QUParameter::In },
	{ "fullPixmap", &static_QUType_varptr, "\x06", QUParameter::In }
    };
    static const QUMethod slot_3 = {"onRegionSelected", 2, param_slot_3 };
    static const QMetaData slot_tbl[] = {
	{ "doCaptureFullScreen()", &slot_0, QMetaData::Private },
	{ "doCaptureActiveWindow()", &slot_1, QMetaData::Private },
	{ "doCaptureForRegion()", &slot_2, QMetaData::Private },
	{ "onRegionSelected(const QRect&,const QPixmap&)", &slot_3, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "filePath", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_0 = {"screenshotReady", 1, param_signal_0 };
    static const QUMethod signal_1 = {"cancelled", 0, 0 };
    static const QMetaData signal_tbl[] = {
	{ "screenshotReady(const QString&)", &signal_0, QMetaData::Public },
	{ "cancelled()", &signal_1, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"ScreenshotManager", parentObject,
	slot_tbl, 4,
	signal_tbl, 2,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_ScreenshotManager.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* ScreenshotManager::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "ScreenshotManager" ) )
	return this;
    return QObject::qt_cast( clname );
}

// SIGNAL screenshotReady
void ScreenshotManager::screenshotReady( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 0, t0 );
}

// SIGNAL cancelled
void ScreenshotManager::cancelled()
{
    activate_signal( staticMetaObject()->signalOffset() + 1 );
}

bool ScreenshotManager::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: doCaptureFullScreen(); break;
    case 1: doCaptureActiveWindow(); break;
    case 2: doCaptureForRegion(); break;
    case 3: onRegionSelected((const QRect&)*((const QRect*)static_QUType_ptr.get(_o+1)),(const QPixmap&)*((const QPixmap*)static_QUType_ptr.get(_o+2))); break;
    default:
	return QObject::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool ScreenshotManager::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: screenshotReady((const QString&)static_QUType_QString.get(_o+1)); break;
    case 1: cancelled(); break;
    default:
	return QObject::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool ScreenshotManager::qt_property( int id, int f, QVariant* v)
{
    return QObject::qt_property( id, f, v);
}

bool ScreenshotManager::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
