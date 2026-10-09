/****************************************************************************
** EmojiLabel meta object code from reading C++ file 'emojiwidgets.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/emojiwidgets.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *EmojiLabel::className() const
{
    return "EmojiLabel";
}

QMetaObject *EmojiLabel::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiLabel( "EmojiLabel", &EmojiLabel::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiLabel::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiLabel", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiLabel::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiLabel", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiLabel::staticMetaObject()
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
    QMetaObject* parentObject = QLabel::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiLabel", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiLabel.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiLabel::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiLabel" ) )
	return this;
    return QLabel::qt_cast( clname );
}

bool EmojiLabel::qt_invoke( int _id, QUObject* _o )
{
    return QLabel::qt_invoke(_id,_o);
}

bool EmojiLabel::qt_emit( int _id, QUObject* _o )
{
    return QLabel::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiLabel::qt_property( int id, int f, QVariant* v)
{
    return QLabel::qt_property( id, f, v);
}

bool EmojiLabel::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *EmojiPushButton::className() const
{
    return "EmojiPushButton";
}

QMetaObject *EmojiPushButton::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiPushButton( "EmojiPushButton", &EmojiPushButton::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiPushButton::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiPushButton", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiPushButton::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiPushButton", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiPushButton::staticMetaObject()
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
    QMetaObject* parentObject = QPushButton::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiPushButton", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiPushButton.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiPushButton::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiPushButton" ) )
	return this;
    return QPushButton::qt_cast( clname );
}

bool EmojiPushButton::qt_invoke( int _id, QUObject* _o )
{
    return QPushButton::qt_invoke(_id,_o);
}

bool EmojiPushButton::qt_emit( int _id, QUObject* _o )
{
    return QPushButton::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiPushButton::qt_property( int id, int f, QVariant* v)
{
    return QPushButton::qt_property( id, f, v);
}

bool EmojiPushButton::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *EmojiToolButton::className() const
{
    return "EmojiToolButton";
}

QMetaObject *EmojiToolButton::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiToolButton( "EmojiToolButton", &EmojiToolButton::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiToolButton::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiToolButton", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiToolButton::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiToolButton", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiToolButton::staticMetaObject()
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
    QMetaObject* parentObject = QToolButton::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiToolButton", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiToolButton.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiToolButton::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiToolButton" ) )
	return this;
    return QToolButton::qt_cast( clname );
}

bool EmojiToolButton::qt_invoke( int _id, QUObject* _o )
{
    return QToolButton::qt_invoke(_id,_o);
}

bool EmojiToolButton::qt_emit( int _id, QUObject* _o )
{
    return QToolButton::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiToolButton::qt_property( int id, int f, QVariant* v)
{
    return QToolButton::qt_property( id, f, v);
}

bool EmojiToolButton::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *EmojiCheckBox::className() const
{
    return "EmojiCheckBox";
}

QMetaObject *EmojiCheckBox::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiCheckBox( "EmojiCheckBox", &EmojiCheckBox::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiCheckBox::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiCheckBox", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiCheckBox::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiCheckBox", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiCheckBox::staticMetaObject()
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
    QMetaObject* parentObject = QCheckBox::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiCheckBox", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiCheckBox.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiCheckBox::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiCheckBox" ) )
	return this;
    return QCheckBox::qt_cast( clname );
}

bool EmojiCheckBox::qt_invoke( int _id, QUObject* _o )
{
    return QCheckBox::qt_invoke(_id,_o);
}

bool EmojiCheckBox::qt_emit( int _id, QUObject* _o )
{
    return QCheckBox::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiCheckBox::qt_property( int id, int f, QVariant* v)
{
    return QCheckBox::qt_property( id, f, v);
}

bool EmojiCheckBox::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *EmojiGroupBox::className() const
{
    return "EmojiGroupBox";
}

QMetaObject *EmojiGroupBox::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiGroupBox( "EmojiGroupBox", &EmojiGroupBox::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiGroupBox::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiGroupBox", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiGroupBox::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiGroupBox", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiGroupBox::staticMetaObject()
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
    QMetaObject* parentObject = QGroupBox::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiGroupBox", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiGroupBox.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiGroupBox::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiGroupBox" ) )
	return this;
    return QGroupBox::qt_cast( clname );
}

bool EmojiGroupBox::qt_invoke( int _id, QUObject* _o )
{
    return QGroupBox::qt_invoke(_id,_o);
}

bool EmojiGroupBox::qt_emit( int _id, QUObject* _o )
{
    return QGroupBox::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiGroupBox::qt_property( int id, int f, QVariant* v)
{
    return QGroupBox::qt_property( id, f, v);
}

bool EmojiGroupBox::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *EmojiLineEdit::className() const
{
    return "EmojiLineEdit";
}

QMetaObject *EmojiLineEdit::metaObj = 0;
static QMetaObjectCleanUp cleanUp_EmojiLineEdit( "EmojiLineEdit", &EmojiLineEdit::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString EmojiLineEdit::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiLineEdit", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString EmojiLineEdit::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "EmojiLineEdit", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* EmojiLineEdit::staticMetaObject()
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
    QMetaObject* parentObject = QLineEdit::staticMetaObject();
    metaObj = QMetaObject::new_metaobject(
	"EmojiLineEdit", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_EmojiLineEdit.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* EmojiLineEdit::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "EmojiLineEdit" ) )
	return this;
    return QLineEdit::qt_cast( clname );
}

bool EmojiLineEdit::qt_invoke( int _id, QUObject* _o )
{
    return QLineEdit::qt_invoke(_id,_o);
}

bool EmojiLineEdit::qt_emit( int _id, QUObject* _o )
{
    return QLineEdit::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool EmojiLineEdit::qt_property( int id, int f, QVariant* v)
{
    return QLineEdit::qt_property( id, f, v);
}

bool EmojiLineEdit::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
