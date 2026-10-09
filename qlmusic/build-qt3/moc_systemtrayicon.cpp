/****************************************************************************
** SystemTrayIcon meta object code from reading C++ file 'systemtrayicon.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/systemtrayicon.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *SystemTrayIcon::className() const
{
    return "SystemTrayIcon";
}

QMetaObject *SystemTrayIcon::metaObj = 0;
static QMetaObjectCleanUp cleanUp_SystemTrayIcon( "SystemTrayIcon", &SystemTrayIcon::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString SystemTrayIcon::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SystemTrayIcon", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString SystemTrayIcon::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SystemTrayIcon", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* SystemTrayIcon::staticMetaObject()
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
    static const QUParameter param_slot_0[] = {
	{ "reason", &static_QUType_ptr, "TrayActivationReason", QUParameter::In }
    };
    static const QUMethod slot_0 = {"nativeActivated", 1, param_slot_0 };
    static const QUMethod slot_1 = {"nativeMessageClicked", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "nativeActivated(TrayActivationReason)", &slot_0, QMetaData::Private },
	{ "nativeMessageClicked()", &slot_1, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "reason", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod signal_0 = {"activated", 1, param_signal_0 };
    static const QUMethod signal_1 = {"messageClicked", 0, 0 };
    static const QMetaData signal_tbl[] = {
	{ "activated(int)", &signal_0, QMetaData::Public },
	{ "messageClicked()", &signal_1, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"SystemTrayIcon", parentObject,
	slot_tbl, 2,
	signal_tbl, 2,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_SystemTrayIcon.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* SystemTrayIcon::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "SystemTrayIcon" ) )
	return this;
    return QObject::qt_cast( clname );
}

// SIGNAL activated
void SystemTrayIcon::activated( int t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 0, t0 );
}

// SIGNAL messageClicked
void SystemTrayIcon::messageClicked()
{
    activate_signal( staticMetaObject()->signalOffset() + 1 );
}

bool SystemTrayIcon::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: nativeActivated((TrayActivationReason)(*((TrayActivationReason*)static_QUType_ptr.get(_o+1)))); break;
    case 1: nativeMessageClicked(); break;
    default:
	return QObject::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool SystemTrayIcon::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: activated((int)static_QUType_int.get(_o+1)); break;
    case 1: messageClicked(); break;
    default:
	return QObject::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool SystemTrayIcon::qt_property( int id, int f, QVariant* v)
{
    return QObject::qt_property( id, f, v);
}

bool SystemTrayIcon::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
