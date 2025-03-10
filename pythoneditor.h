#ifndef PYTHONEDITOR_H
#define PYTHONEDITOR_H

#include <QObject>

class PythonEditor : public QObject
{
    Q_OBJECT
public:
    explicit PythonEditor(QObject *parent = nullptr);

signals:
};

#endif // PYTHONEDITOR_H
