/****************************************************************************
** Meta object code from reading C++ file 'pythonrunner.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../pythonrunner.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'pythonrunner.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_PythonRunner_t {
    QByteArrayData data[15];
    char stringdata0[181];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_PythonRunner_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_PythonRunner_t qt_meta_stringdata_PythonRunner = {
    {
QT_MOC_LITERAL(0, 0, 12), // "PythonRunner"
QT_MOC_LITERAL(1, 13, 12), // "pythonOutput"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 6), // "output"
QT_MOC_LITERAL(4, 34, 14), // "scriptFinished"
QT_MOC_LITERAL(5, 49, 8), // "exitCode"
QT_MOC_LITERAL(6, 58, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(7, 79, 10), // "exitStatus"
QT_MOC_LITERAL(8, 90, 13), // "scriptStarted"
QT_MOC_LITERAL(9, 104, 13), // "scriptStopped"
QT_MOC_LITERAL(10, 118, 17), // "thresholdDetected"
QT_MOC_LITERAL(11, 136, 6), // "marker"
QT_MOC_LITERAL(12, 143, 6), // "window"
QT_MOC_LITERAL(13, 150, 15), // "handleReadyRead"
QT_MOC_LITERAL(14, 166, 14) // "handleFinished"

    },
    "PythonRunner\0pythonOutput\0\0output\0"
    "scriptFinished\0exitCode\0QProcess::ExitStatus\0"
    "exitStatus\0scriptStarted\0scriptStopped\0"
    "thresholdDetected\0marker\0window\0"
    "handleReadyRead\0handleFinished"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_PythonRunner[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   49,    2, 0x06 /* Public */,
       4,    2,   52,    2, 0x06 /* Public */,
       8,    0,   57,    2, 0x06 /* Public */,
       9,    0,   58,    2, 0x06 /* Public */,
      10,    2,   59,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      13,    0,   64,    2, 0x08 /* Private */,
      14,    2,   65,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 6,    5,    7,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::LongLong,   11,   12,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 6,    5,    7,

       0        // eod
};

void PythonRunner::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PythonRunner *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->pythonOutput((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->scriptFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        case 2: _t->scriptStarted(); break;
        case 3: _t->scriptStopped(); break;
        case 4: _t->thresholdDetected((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        case 5: _t->handleReadyRead(); break;
        case 6: _t->handleFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PythonRunner::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PythonRunner::pythonOutput)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PythonRunner::*)(int , QProcess::ExitStatus );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PythonRunner::scriptFinished)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (PythonRunner::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PythonRunner::scriptStarted)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (PythonRunner::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PythonRunner::scriptStopped)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (PythonRunner::*)(const QString & , qint64 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PythonRunner::thresholdDetected)) {
                *result = 4;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject PythonRunner::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_PythonRunner.data,
    qt_meta_data_PythonRunner,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *PythonRunner::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PythonRunner::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PythonRunner.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PythonRunner::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void PythonRunner::pythonOutput(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void PythonRunner::scriptFinished(int _t1, QProcess::ExitStatus _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void PythonRunner::scriptStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void PythonRunner::scriptStopped()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void PythonRunner::thresholdDetected(const QString & _t1, qint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
