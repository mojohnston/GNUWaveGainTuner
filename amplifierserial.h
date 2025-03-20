#ifndef AMPLIFIERSERIAL_H
#define AMPLIFIERSERIAL_H

#include <QObject>
#include <QSerialPort>
#include <QMap>
#include <QByteArray>

class AmplifierSerial : public QObject
{
    Q_OBJECT
public:
    explicit AmplifierSerial(QObject *parent = nullptr);
    ~AmplifierSerial();
    void disconnectAll();
    void searchAndConnect();
    void sendCommand(const QString &command, const QString &device);

    // Convenience amplifier commands
    void getMode(const QString &device);
    void setMode(const QString &mode, const QString &device);
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

    QStringList connectedDevices() const;

signals:
    void ampOutput(const QString &device, const QString &output);
    void ampError(const QString &device, const QString &error);

private slots:
    void handleReadyRead();

private:
    QMap<QString, QSerialPort*> m_ports; // Maps devices to their corresponding serial ports
    QMap<QString, QByteArray> m_buffers; // Maps devices to their buffers for responses
};

#endif // AMPLIFIERSERIAL_H
