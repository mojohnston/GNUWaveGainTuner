#ifndef INIEDITOR_H
#define INIEDITOR_H

#include <QObject>

class IniEditor : public QObject
{
    Q_OBJECT
public:
    explicit IniEditor(QObject *parent = nullptr);

signals:
};

#endif // INIEDITOR_H
