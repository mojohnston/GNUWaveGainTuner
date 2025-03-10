#ifndef PYTHONRUNNER_H
#define PYTHONRUNNER_H

#include <QObject>

class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(QObject *parent = nullptr);

signals:
};

#endif // PYTHONRUNNER_H
