#ifndef PYTHONEDITOR_H
#define PYTHONEDITOR_H

#include <QObject>

class PythonEditor : public QObject
{
    Q_OBJECT
public:
    explicit PythonEditor(QObject *parent = nullptr);

    bool editGainValue(const QString &filePath, int newGain, int channel = -1);

signals:

};

#endif // PYTHONEDITOR_H
