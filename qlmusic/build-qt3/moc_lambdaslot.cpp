/****************************************************************************
** LambdaSlot meta object code from reading C++ file 'lambdaslot.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/lambdaslot.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *LambdaSlot::className() const
{
    return "LambdaSlot";
}

QMetaObject *LambdaSlot::metaObj = 0;
static QMetaObjectCleanUp cleanUp_LambdaSlot( "LambdaSlot", &LambdaSlot::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString LambdaSlot::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "LambdaSlot", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString LambdaSlot::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "LambdaSlot", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* LambdaSlot::staticMetaObject()
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
    static const QUMethod slot_0 = {"call", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "call()", &slot_0, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"LambdaSlot", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_LambdaSlot.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* LambdaSlot::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "LambdaSlot" ) )
	return this;
    return QObject::qt_cast( clname );
}

bool LambdaSlot::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: call(); break;
    default:
	return QObject::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool LambdaSlot::qt_emit( int _id, QUObject* _o )
{
    return QObject::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool LambdaSlot::qt_property( int id, int f, QVariant* v)
{
    return QObject::qt_property( id, f, v);
}

bool LambdaSlot::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
