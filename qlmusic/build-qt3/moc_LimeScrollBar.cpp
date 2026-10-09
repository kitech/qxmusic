/****************************************************************************
** LimeScrollBar meta object code from reading C++ file 'LimeScrollBar.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/LimeScrollBar.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *LimeScrollBar::className() const
{
    return "LimeScrollBar";
}

QMetaObject *LimeScrollBar::metaObj = 0;
static QMetaObjectCleanUp cleanUp_LimeScrollBar( "LimeScrollBar", &LimeScrollBar::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString LimeScrollBar::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "LimeScrollBar", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString LimeScrollBar::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "LimeScrollBar", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* LimeScrollBar::staticMetaObject()
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
    QMetaObject* parentObject = QScrollBar::staticMetaObject();
    static const QUMethod slot_0 = {"onFadeIn", 0, 0 };
    static const QUMethod slot_1 = {"onFadeOut", 0, 0 };
    static const QUMethod slot_2 = {"onHideTimeout", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onFadeIn()", &slot_0, QMetaData::Private },
	{ "onFadeOut()", &slot_1, QMetaData::Private },
	{ "onHideTimeout()", &slot_2, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"LimeScrollBar", parentObject,
	slot_tbl, 3,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_LimeScrollBar.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* LimeScrollBar::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "LimeScrollBar" ) )
	return this;
    return QScrollBar::qt_cast( clname );
}

bool LimeScrollBar::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onFadeIn(); break;
    case 1: onFadeOut(); break;
    case 2: onHideTimeout(); break;
    default:
	return QScrollBar::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool LimeScrollBar::qt_emit( int _id, QUObject* _o )
{
    return QScrollBar::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool LimeScrollBar::qt_property( int id, int f, QVariant* v)
{
    return QScrollBar::qt_property( id, f, v);
}

bool LimeScrollBar::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
