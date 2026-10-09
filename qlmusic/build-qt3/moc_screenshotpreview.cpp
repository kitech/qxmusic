/****************************************************************************
** ScreenshotPreviewDialog meta object code from reading C++ file 'screenshotpreview.h'
**
** Created by: The Qt Meta Object Compiler version 26 (Qt 3.5.0)
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "../../../doxhttpd/qlcomp/screenshotpreview.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.5.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *ScreenshotPreviewDialog::className() const
{
    return "ScreenshotPreviewDialog";
}

QMetaObject *ScreenshotPreviewDialog::metaObj = 0;
static QMetaObjectCleanUp cleanUp_ScreenshotPreviewDialog( "ScreenshotPreviewDialog", &ScreenshotPreviewDialog::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString ScreenshotPreviewDialog::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ScreenshotPreviewDialog", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString ScreenshotPreviewDialog::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "ScreenshotPreviewDialog", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* ScreenshotPreviewDialog::staticMetaObject()
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
    static const QUMethod slot_0 = {"onSendClicked", 0, 0 };
    static const QUMethod slot_1 = {"onCopyClicked", 0, 0 };
    static const QUMethod slot_2 = {"onSaveClicked", 0, 0 };
    static const QUMethod slot_3 = {"onCancelClicked", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onSendClicked()", &slot_0, QMetaData::Private },
	{ "onCopyClicked()", &slot_1, QMetaData::Private },
	{ "onSaveClicked()", &slot_2, QMetaData::Private },
	{ "onCancelClicked()", &slot_3, QMetaData::Private }
    };
    static const QUParameter param_signal_0[] = {
	{ "filePath", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_0 = {"sendRequested", 1, param_signal_0 };
    static const QUParameter param_signal_1[] = {
	{ "filePath", &static_QUType_QString, 0, QUParameter::In }
    };
    static const QUMethod signal_1 = {"saveRequested", 1, param_signal_1 };
    static const QUMethod signal_2 = {"cancelled", 0, 0 };
    static const QMetaData signal_tbl[] = {
	{ "sendRequested(const QString&)", &signal_0, QMetaData::Public },
	{ "saveRequested(const QString&)", &signal_1, QMetaData::Public },
	{ "cancelled()", &signal_2, QMetaData::Public }
    };
    metaObj = QMetaObject::new_metaobject(
	"ScreenshotPreviewDialog", parentObject,
	slot_tbl, 4,
	signal_tbl, 3,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_ScreenshotPreviewDialog.setMetaObject( metaObj );
#ifdef QT_THREAD_SUPPORT
    if (qt_sharedMetaObjectMutex) qt_sharedMetaObjectMutex->unlock();
#endif // QT_THREAD_SUPPORT
    return metaObj;
}

void* ScreenshotPreviewDialog::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "ScreenshotPreviewDialog" ) )
	return this;
    return QDialog::qt_cast( clname );
}

// SIGNAL sendRequested
void ScreenshotPreviewDialog::sendRequested( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 0, t0 );
}

// SIGNAL saveRequested
void ScreenshotPreviewDialog::saveRequested( const QString& t0 )
{
    activate_signal( staticMetaObject()->signalOffset() + 1, t0 );
}

// SIGNAL cancelled
void ScreenshotPreviewDialog::cancelled()
{
    activate_signal( staticMetaObject()->signalOffset() + 2 );
}

bool ScreenshotPreviewDialog::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onSendClicked(); break;
    case 1: onCopyClicked(); break;
    case 2: onSaveClicked(); break;
    case 3: onCancelClicked(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool ScreenshotPreviewDialog::qt_emit( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->signalOffset() ) {
    case 0: sendRequested((const QString&)static_QUType_QString.get(_o+1)); break;
    case 1: saveRequested((const QString&)static_QUType_QString.get(_o+1)); break;
    case 2: cancelled(); break;
    default:
	return QDialog::qt_emit(_id,_o);
    }
    return TRUE;
}
#ifndef QT_NO_PROPERTIES

bool ScreenshotPreviewDialog::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool ScreenshotPreviewDialog::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
