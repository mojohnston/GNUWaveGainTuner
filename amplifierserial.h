#ifndef AMPLIFIERSERIAL_H
#define AMPLIFIERSERIAL_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

class AmplifierSerial : public QObject
{
    Q_OBJECT
public:
    explicit AmplifierSerial(QObject *parent = nullptr);
    ~AmplifierSerial();

    void searchAndConnect();
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

    // Public accessor to retrieve the list of connected amplifier device names.
    QStringList connectedDevices() const;

signals:
    // Emitted when an amp outputs a complete line.
    void ampOutput(const QString &device, const QString &output);
    // Emitted when an amp outputs an error message. this needs editing to be useful with all available error codes, but this would affect logic, especially ALC mode checks
    void ampError(const QString &device, const QString &errorMessage);

private slots:
    void handleReadyRead();

private:
    QMap<QString, QSerialPort*> m_ports;
    QMap<QString, QByteArray> m_buffers;
};

#endif // AMPLIFIERSERIAL_H
