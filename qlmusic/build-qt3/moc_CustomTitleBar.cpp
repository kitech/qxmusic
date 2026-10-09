/****************************************************************************
** CustomTitleBar meta object code from reading C++ file 'CustomTitleBar.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/CustomTitleBar.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *CustomTitleBar::className() const
{
    return "CustomTitleBar";
}

QMetaObject *CustomTitleBar::metaObj = 0;
static QMetaObjectCleanUp cleanUp_CustomTitleBar( "CustomTitleBar", &CustomTitleBar::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString CustomTitleBar::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CustomTitleBar", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString CustomTitleBar::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CustomTitleBar", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* CustomTitleBar::staticMetaObject()
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
    static const QUMethod slot_0 = {"toggleMenu", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "toggleMenu()", &slot_0, QMetaData::Private }
    };
    static const QUMethod signal_0 = {"appMenuClicked", 0, 0 };
    static const QMetaData signal_tbl[] = {
	{ "appMenuClicked()", &signal_0, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"CustomTitleBar", parentObject,
	slot_tbl, 1,
	signal_tbl, 1,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_CustomTitleBar.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* CustomTitleBar::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "CustomTitleBar" ) )
	return this;
    return QWidget::qt_cast( clname );
}

// SIGNAL appMenuClicked
void CustomTitleBar::appMenuClicked()
{
    activate_signal( staticMetaObject()->signalOffset() + 0 );
}

bool CustomTitleBar::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: toggleMenu(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool CustomTitleBar::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: appMenuClicked(); break;
    default:
	return QWidget::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool CustomTitleBar::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool CustomTitleBar::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
