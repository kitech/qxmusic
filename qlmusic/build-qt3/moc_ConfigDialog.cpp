/****************************************************************************
** ConfigDialog meta object code from reading C++ file 'ConfigDialog.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/ConfigDialog.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *ConfigDialog::className() const
{
    return "ConfigDialog";
}

QMetaObject *ConfigDialog::metaObj = 0;
static QMetaObjectCleanUp cleanUp_ConfigDialog( "ConfigDialog", &ConfigDialog::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString ConfigDialog::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ConfigDialog", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString ConfigDialog::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ConfigDialog", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* ConfigDialog::staticMetaObject()
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
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ "index", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"onCategoryChanged", 1, param_slot_0 };
    static const QUMethod slot_1 = {"onAccepted", 0, 0 };
    static const QUMethod slot_2 = {"onApply", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onCategoryChanged(int)", &slot_0, QMetaData::Private },
	{ "onAccepted()", &slot_1, QMetaData::Private },
	{ "onApply()", &slot_2, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "changed", &static_QUType_ptr, "SettingsChangedMap", QUParameter::In }
    };
    static const QUMethod signal_0 = {"settingsSaved", 1, param_signal_0 };
    static const QMetaData signal_tbl[] = {
	{ "settingsSaved(const SettingsChangedMap&)", &signal_0, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"ConfigDialog", parentObject,
	slot_tbl, 3,
	signal_tbl, 1,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_ConfigDialog.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* ConfigDialog::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "ConfigDialog" ) )
	return this;
    return QDialog::qt_cast( clname );
}

#include <qobjectdefs.h>
#include <qsignalslotimp.h>

// SIGNAL settingsSaved
void ConfigDialog::settingsSaved( const SettingsChangedMap& t0 )
{
    if ( signalsBlocked() )
	return;
    QConnectionList *clist = receivers( staticMetaObject()->signalOffset() + 0 );
    if ( !clist )
	return;
    QUObject o[2];
    static_QUType_ptr.set(o+1,&t0);
    o[1].isLastObject = true;
    activate_signal( clist, o );
}

bool ConfigDialog::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onCategoryChanged((int)static_QUType_int.get(_o+1)); break;
    case 1: onAccepted(); break;
    case 2: onApply(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool ConfigDialog::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: settingsSaved((const SettingsChangedMap&)*((const SettingsChangedMap*)static_QUType_ptr.get(_o+1))); break;
    default:
	return QDialog::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool ConfigDialog::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool ConfigDialog::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *CategoryPage::className() const
{
    return "CategoryPage";
}

QMetaObject *CategoryPage::metaObj = 0;
static QMetaObjectCleanUp cleanUp_CategoryPage( "CategoryPage", &CategoryPage::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString CategoryPage::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CategoryPage", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString CategoryPage::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "CategoryPage", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* CategoryPage::staticMetaObject()
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
    metaObj = QMetaObject::new_metaobject(
	"CategoryPage", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_CategoryPage.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* CategoryPage::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "CategoryPage" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool CategoryPage::qt_invoke( int _id, QUObject* _o )
{
    return QWidget::qt_invoke(_id,_o);
}

bool CategoryPage::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool CategoryPage::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool CategoryPage::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES


const char *ConfigGroupBox::className() const
{
    return "ConfigGroupBox";
}

QMetaObject *ConfigGroupBox::metaObj = 0;
static QMetaObjectCleanUp cleanUp_ConfigGroupBox( "ConfigGroupBox", &ConfigGroupBox::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString ConfigGroupBox::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ConfigGroupBox", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString ConfigGroupBox::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ConfigGroupBox", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* ConfigGroupBox::staticMetaObject()
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
	"ConfigGroupBox", parentObject,
	0, 0,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_ConfigGroupBox.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* ConfigGroupBox::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "ConfigGroupBox" ) )
	return this;
    return QGroupBox::qt_cast( clname );
}

bool ConfigGroupBox::qt_invoke( int _id, QUObject* _o )
{
    return QGroupBox::qt_invoke(_id,_o);
}

bool ConfigGroupBox::qt_emit( int _id, QUObject* _o )
{
    return QGroupBox::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool ConfigGroupBox::qt_property( int id, int f, QVariant* v)
{
    return QGroupBox::qt_property( id, f, v);
}

bool ConfigGroupBox::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
