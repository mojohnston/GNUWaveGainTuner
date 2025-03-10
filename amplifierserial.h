#ifndef AMPLIFIERSERIAL_H
#define AMPLIFIERSERIAL_H

#include <QObject>

class AmplifierSerial : public QObject
{
    Q_OBJECT
public:
    explicit AmplifierSerial(QObject *parent = nullptr);

signals:
};

#endif // AMPLIFIERSERIAL_H
