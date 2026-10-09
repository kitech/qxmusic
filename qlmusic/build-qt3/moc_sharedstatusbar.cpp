/****************************************************************************
** SharedStatusBar meta object code from reading C++ file 'sharedstatusbar.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/sharedstatusbar.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *SharedStatusBar::className() const
{
    return "SharedStatusBar";
}

QMetaObject *SharedStatusBar::metaObj = 0;
static QMetaObjectCleanUp cleanUp_SharedStatusBar( "SharedStatusBar", &SharedStatusBar::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString SharedStatusBar::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SharedStatusBar", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString SharedStatusBar::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SharedStatusBar", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* SharedStatusBar::staticMetaObject()
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
    static const QUParameter param_slot_0[] = {
	{ "old", &static_QUType_ptr, "QWidget", QUParameter::In },
	{ "now", &static_QUType_ptr, "QWidget", QUParameter::In }
    };
    static const QUMethod slot_0 = {"onFocusChanged", 2, param_slot_0 };
    static const QUMethod slot_1 = {"onHistoryClicked", 0, 0 };
    static const QUMethod slot_2 = {"onIconTimeout", 0, 0 };
    static const QUMethod slot_3 = {"onDebounceTimeout", 0, 0 };
    static const QUMethod slot_4 = {"retrack", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onFocusChanged(QWidget*,QWidget*)", &slot_0, QMetaData::Private },
	{ "onHistoryClicked()", &slot_1, QMetaData::Private },
	{ "onIconTimeout()", &slot_2, QMetaData::Private },
	{ "onDebounceTimeout()", &slot_3, QMetaData::Private },
	{ "retrack()", &slot_4, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"SharedStatusBar", parentObject,
	slot_tbl, 5,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_SharedStatusBar.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* SharedStatusBar::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "SharedStatusBar" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool SharedStatusBar::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onFocusChanged((QWidget*)static_QUType_ptr.get(_o+1),(QWidget*)static_QUType_ptr.get(_o+2)); break;
    case 1: onHistoryClicked(); break;
    case 2: onIconTimeout(); break;
    case 3: onDebounceTimeout(); break;
    case 4: retrack(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool SharedStatusBar::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool SharedStatusBar::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool SharedStatusBar::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
