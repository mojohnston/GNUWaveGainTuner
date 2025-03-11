#ifndef AMPLIFIERSERIAL_H
#define AMPLIFIERSERIAL_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QSerialPort>
#include <QSerialPortInfo>

class AmplifierSerial : public QObject
{
    Q_OBJECT
public:
    explicit AmplifierSerial(QObject *parent = nullptr);
    ~AmplifierSerial();

    // Searches available serial ports for amp devices and connects to them.
    void searchAndConnect();

    // Sends a command (with a newline) to the amp specified by device name.
    void sendCommand(const QString &command, const QString &device);

    // Convenience functions for amplifier commands:
    void getMode(const QString &device);
    void setMode(const QString &mode, const QString &device); // mode should be "ALC" or "VVA"
    void setStandby(const QString &device);
    void setOnline(const QString &device);
    void getFwdPwr(const QString &device);
    void getRevPwr(const QString &device);
    void getAlcLvl(const QString &device);
    void setAlcLvl(double level, const QString &device);
    void getGainLvl(const QString &device);
    void setGainLvl(double level, const QString &device);
    void sendAckFaults(const QString &device);
    void getFaults(const QString &device);
    void getSerialId(const QString &device);
    void getModelId(const QString &device);

signals:
    // Emitted when an amp outputs data.
    void ampOutput(const QString &device, const QString &output);
    // Emitted when an amp outputs an error message.
    void ampError(const QString &device, const QString &errorMessage);

private slots:
    // Slot to handle output from each serial port.
    void handleReadyRead();

private:
    // Map of device name (system location) to the corresponding QSerialPort pointer.
    QMap<QString, QSerialPort*> m_ports;
};

#endif // AMPLIFIERSERIAL_H
