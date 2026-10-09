/****************************************************************************
** SearchLineEdit meta object code from reading C++ file 'searchline.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/searchline.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *SearchLineEdit::className() const
{
    return "SearchLineEdit";
}

QMetaObject *SearchLineEdit::metaObj = 0;
static QMetaObjectCleanUp cleanUp_SearchLineEdit( "SearchLineEdit", &SearchLineEdit::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString SearchLineEdit::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SearchLineEdit", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString SearchLineEdit::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "SearchLineEdit", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* SearchLineEdit::staticMetaObject()
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
    QMetaObject* parentObject = QFrame::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ "text", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"onEditTextChanged", 1, param_slot_0 };
    static const QUMethod slot_1 = {"onClearClicked", 0, 0 };
    static const QUMethod slot_2 = {"onSuggestionClicked", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onEditTextChanged(const QString&)", &slot_0, QMetaData::Private },
	{ "onClearClicked()", &slot_1, QMetaData::Private },
	{ "onSuggestionClicked()", &slot_2, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "text", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_0 = {"textChanged", 1, param_signal_0 };
    static const QUMethod signal_1 = {"cleared", 0, 0 };
    static const QUParameter param_signal_2[] = {
	{ "text", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_2 = {"triggered", 1, param_signal_2 };
    static const QMetaData signal_tbl[] = {
	{ "textChanged(const QString&)", &signal_0, QMetaData::Public },
	{ "cleared()", &signal_1, QMetaData::Public },
	{ "triggered(const QString&)", &signal_2, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"SearchLineEdit", parentObject,
	slot_tbl, 3,
	signal_tbl, 3,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_SearchLineEdit.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* SearchLineEdit::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "SearchLineEdit" ) )
	return this;
    return QFrame::qt_cast( clname );
}

// SIGNAL textChanged
void SearchLineEdit::textChanged( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 0, t0 );
}

// SIGNAL cleared
void SearchLineEdit::cleared()
{
    activate_signal( staticMetaObject()->signalOffset() + 1 );
}

// SIGNAL triggered
void SearchLineEdit::triggered( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 2, t0 );
}

bool SearchLineEdit::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onEditTextChanged((const QString&)static_QUType_QString.get(_o+1)); break;
    case 1: onClearClicked(); break;
    case 2: onSuggestionClicked(); break;
    default:
	return QFrame::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool SearchLineEdit::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: textChanged((const QString&)static_QUType_QString.get(_o+1)); break;
    case 1: cleared(); break;
    case 2: triggered((const QString&)static_QUType_QString.get(_o+1)); break;
    default:
	return QFrame::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool SearchLineEdit::qt_property( int id, int f, QVariant* v)
{
    return QFrame::qt_property( id, f, v);
}

bool SearchLineEdit::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
