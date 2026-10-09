/****************************************************************************
** FramelessHelper meta object code from reading C++ file 'FramelessHelper.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/FramelessHelper.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *FramelessHelper::className() const
{
    return "FramelessHelper";
}

QMetaObject *FramelessHelper::metaObj = 0;
static QMetaObjectCleanUp cleanUp_FramelessHelper( "FramelessHelper", &FramelessHelper::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString FramelessHelper::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "FramelessHelper", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString FramelessHelper::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "FramelessHelper", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* FramelessHelper::staticMetaObject()
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
	{ "pos", &static_QUType_varptr, "\x0e", QUParameter::In }
    };
    static const QUMethod slot_0 = {"showSystemMenu", 1, param_slot_0 };
    static const QUMethod slot_1 = {"showSystemMenuFromSys", 0, 0 };
    static const QUMethod slot_2 = {"toggleMaximize", 0, 0 };
    static const QUMethod slot_3 = {"onMinClicked", 0, 0 };
    static const QUMethod slot_4 = {"onMaxClicked", 0, 0 };
    static const QUMethod slot_5 = {"onCloseClicked", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "showSystemMenu(const QPoint&)", &slot_0, QMetaData::Public },
	{ "showSystemMenuFromSys()", &slot_1, QMetaData::Public },
	{ "toggleMaximize()", &slot_2, QMetaData::Public },
	{ "onMinClicked()", &slot_3, QMetaData::Public },
	{ "onMaxClicked()", &slot_4, QMetaData::Public },
	{ "onCloseClicked()", &slot_5, QMetaData::Public }
    };
    static const QUParameter param_signal_0[] = {
	{ "widget", &static_QUType_ptr, "QWidget", QUParameter::In }
    };
    static const QUMethod signal_0 = {"titleBarChanged", 1, param_signal_0 };
    static const QMetaData signal_tbl[] = {
	{ "titleBarChanged(QWidget*)", &signal_0, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"FramelessHelper", parentObject,
	slot_tbl, 6,
	signal_tbl, 1,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_FramelessHelper.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* FramelessHelper::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "FramelessHelper" ) )
	return this;
    return QObject::qt_cast( clname );
}

#include <qobjectdefs.h>
#include <qsignalslotimp.h>

// SIGNAL titleBarChanged
void FramelessHelper::titleBarChanged( QWidget* t0 )
{
    if ( signalsBlocked() )
	return;
    QConnectionList *clist = receivers( staticMetaObject()->signalOffset() + 0 );
    if ( !clist )
	return;
    QUObject o[2];
    static_QUType_ptr.set(o+1,t0);
    o[1].isLastObject = true;
    activate_signal( clist, o );
}

bool FramelessHelper::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: showSystemMenu((const QPoint&)*((const QPoint*)static_QUType_ptr.get(_o+1))); break;
    case 1: showSystemMenuFromSys(); break;
    case 2: toggleMaximize(); break;
    case 3: onMinClicked(); break;
    case 4: onMaxClicked(); break;
    case 5: onCloseClicked(); break;
    default:
	return QObject::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool FramelessHelper::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: titleBarChanged((QWidget*)static_QUType_ptr.get(_o+1)); break;
    default:
	return QObject::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool FramelessHelper::qt_property( int id, int f, QVariant* v)
{
    return QObject::qt_property( id, f, v);
}

bool FramelessHelper::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
