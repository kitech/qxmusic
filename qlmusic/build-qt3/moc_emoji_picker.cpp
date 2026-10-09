/****************************************************************************
** EmojiPicker meta object code from reading C++ file 'emoji_picker.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/emoji_picker.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *EmojiPicker::className() const
{
    return "EmojiPicker";
}

QMetaObject *EmojiPicker::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiPicker( "EmojiPicker", &EmojiPicker::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiPicker::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiPicker", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiPicker::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiPicker", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiPicker::staticMetaObject()
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
	{ "query", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"onSearchChanged", 1, param_slot_0 };
    static const QUMethod slot_1 = {"onTabClicked", 0, 0 };
    static const QUMethod slot_2 = {"onEmojiButtonClicked", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onSearchChanged(const QString&)", &slot_0, QMetaData::Private },
	{ "onTabClicked()", &slot_1, QMetaData::Private },
	{ "onEmojiButtonClicked()", &slot_2, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "emoji", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_0 = {"emojiSelected", 1, param_signal_0 };
    static const QMetaData signal_tbl[] = {
	{ "emojiSelected(const QString&)", &signal_0, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"EmojiPicker", parentObject,
	slot_tbl, 3,
	signal_tbl, 1,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiPicker.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiPicker::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiPicker" ) )
	return this;
    return QWidget::qt_cast( clname );
}

// SIGNAL emojiSelected
void EmojiPicker::emojiSelected( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 0, t0 );
}

bool EmojiPicker::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onSearchChanged((const QString&)static_QUType_QString.get(_o+1)); break;
    case 1: onTabClicked(); break;
    case 2: onEmojiButtonClicked(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool EmojiPicker::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: emojiSelected((const QString&)static_QUType_QString.get(_o+1)); break;
    default:
	return QWidget::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool EmojiPicker::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool EmojiPicker::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
