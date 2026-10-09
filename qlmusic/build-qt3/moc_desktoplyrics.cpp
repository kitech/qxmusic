/****************************************************************************
** DesktopLyrics meta object code from reading C++ file 'desktoplyrics.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/desktoplyrics.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#include <qvariant.h>
const char *DesktopLyrics::className() const
{
    return "DesktopLyrics";
}

QMetaObject *DesktopLyrics::metaObj = 0;
static QMetaObjectCleanUp cleanUp_DesktopLyrics( "DesktopLyrics", &DesktopLyrics::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString DesktopLyrics::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "DesktopLyrics", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString DesktopLyrics::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "DesktopLyrics", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* DesktopLyrics::staticMetaObject()
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
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod signal_0 = {"aboutToShow", 0, 0 };
    static const QUParameter param_signal_1[] = {
	{ "msec", &static_QUType_ptr, "long long", QUParameter::In }
    };
    static const QUMethod signal_1 = {"positionClicked", 1, param_signal_1 };
    static const QUMethod signal_2 = {"hideRequested", 0, 0 };
    static const QMetaData signal_tbl[] = {
	{ "aboutToShow()", &signal_0, QMetaData::Public },
	{ "positionClicked(long long)", &signal_1, QMetaData::Public },
	{ "hideRequested()", &signal_2, QMetaData::Public }
    };
#ifndef QT_NO_PROPERTIES
    static const QMetaProperty props_tbl[1] = {
 	{ "float","progress", 0x010f, &DesktopLyrics::metaObj, 0, -1 }
    };
#endif // QT_NO_PROPERTIES
    metaObj = QMetaObject::new_metaobject(
	"DesktopLyrics", parentObject,
	0, 0,
	signal_tbl, 3,
#ifndef QT_NO_PROPERTIES
	props_tbl, 1,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_DesktopLyrics.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* DesktopLyrics::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "DesktopLyrics" ) )
	return this;
    return QWidget::qt_cast( clname );
}

// SIGNAL aboutToShow
void DesktopLyrics::aboutToShow()
{
    activate_signal( staticMetaObject()->signalOffset() + 0 );
}

#include <qobjectdefs.h>
#include <qsignalslotimp.h>

// SIGNAL positionClicked
void DesktopLyrics::positionClicked( long long t0 )
{
    if ( signalsBlocked() )
	return;
    QConnectionList *clist = receivers( staticMetaObject()->signalOffset() + 1 );
    if ( !clist )
	return;
    QUObject o[2];
    static_QUType_ptr.set(o+1,&t0);
    o[1].isLastObject = true;
    activate_signal( clist, o );
}

// SIGNAL hideRequested
void DesktopLyrics::hideRequested()
{
    activate_signal( staticMetaObject()->signalOffset() + 2 );
}

bool DesktopLyrics::qt_invoke( int _id, QUObject* _o )
{
    return QWidget::qt_invoke(_id,_o);
}

bool DesktopLyrics::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: aboutToShow(); break;
    case 1: positionClicked((long long)(*((long long*)static_QUType_ptr.get(_o+1)))); break;
    case 2: hideRequested(); break;
    default:
	return QWidget::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool DesktopLyrics::qt_property( int id, int f, QVariant* v)
{
    switch ( id - staticMetaObject()->propertyOffset() ) {
    case 0: switch( f ) {
	case 0: setProgress((float&)v->asInt()); break;
	case 1: *v = QVariant( (int)this->progress() ); break;
	case 3: case 4: case 5: break;
	default: return FALSE;
    } break;
    default:
	return QWidget::qt_property( id, f, v );
    }
    return TRUE;
}

bool DesktopLyrics::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
