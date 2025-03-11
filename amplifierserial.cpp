#include "amplifierserial.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

AmplifierSerial::AmplifierSerial(QObject *parent)
    : QObject(parent)
{
}

AmplifierSerial::~AmplifierSerial()
{
    // Close and delete all serial port connections.
    for (QSerialPort *port : qAsConst(m_ports)) {
        if (port->isOpen())
            port->close();
        delete port;
    }
    m_ports.clear();
}

void AmplifierSerial::searchAndConnect()
{
    // List of candidate device names for amp connections (as defined by udev rules).
    QStringList candidateDevices = {
        "/dev/ttyS_AMP",
        "/dev/ttyS_AMP1",
        "/dev/ttyS_AMP2",
        "/dev/ttyUSB_AMP",
        "/dev/ttyUSB_AMP1",
        "/dev/ttyUSB_AMP2",
        "/dev/ttyUSB_AMPL1",
        "/dev/ttyUSB_AMPL2",
        "/dev/ttyUSB_AMPL1L2"
    };

    // Clear any existing connections.
    for (QSerialPort *port : qAsConst(m_ports)) {
        if (port->isOpen())
            port->close();
        delete port;
    }
    m_ports.clear();

    // Loop over available serial ports.
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : availablePorts) {
        QString sysLoc = info.systemLocation();
        if (candidateDevices.contains(sysLoc)) {
            qDebug() << "Found amp device:" << sysLoc;
            QSerialPort *port = new QSerialPort(info, this);
            port->setObjectName(sysLoc); // Save system location for later

            port->setBaudRate(QSerialPort::Baud9600);
            port->setDataBits(QSerialPort::Data8);
            port->setParity(QSerialPort::NoParity);
            port->setStopBits(QSerialPort::OneStop);
            port->setFlowControl(QSerialPort::NoFlowControl);
            if (port->open(QIODevice::ReadWrite)) {
                qDebug() << "Connected to amp:" << sysLoc;
                // Connect readyRead
                connect(port, &QSerialPort::readyRead, this, &AmplifierSerial::handleReadyRead);
                m_ports.insert(sysLoc, port);
            } else {
                qWarning() << "Failed to open amp:" << sysLoc << ":" << port->errorString();
                delete port;
            }
        }
    }
}

void AmplifierSerial::sendCommand(const QString &command, const QString &device)
{
    // Look up the QSerialPort for the given device.
    if (m_ports.contains(device)) {
        QSerialPort *port = m_ports.value(device);
        if (port->isOpen()) {
            QByteArray cmd = command.toUtf8() + "\n";
            port->write(cmd);
            qDebug() << "Sent command" << command << "to device" << device;
        } else {
            qWarning() << "Port for" << device << "is not open.";
        }
    } else {
        qWarning() << "Device" << device << "not found.";
    }
}

// Amplifier Commands

void AmplifierSerial::getMode(const QString &device)
{
    sendCommand("MODE?", device);
}

void AmplifierSerial::setMode(const QString &mode, const QString &device)
{
    // mode should be "ALC" or "VVA"
    sendCommand("MODE " + mode, device);
}

void AmplifierSerial::setStandby(const QString &device)
{
    sendCommand("STANDBY", device);
}

void AmplifierSerial::setOnline(const QString &device)
{
    sendCommand("ONLINE", device);
}

void AmplifierSerial::getFwdPwr(const QString &device)
{
    sendCommand("FWD_PWR?", device);
}

void AmplifierSerial::getRevPwr(const QString &device)
{
    sendCommand("REV_PWR?", device);
}

void AmplifierSerial::getAlcLvl(const QString &device)
{
    sendCommand("ALC_LEVEL?", device);
}

void AmplifierSerial::setAlcLvl(double level, const QString &device)
{
    // Format level as a floating point number (one decimal).
    QString command = QString("ALC_LEVEL %1").arg(level, 0, 'f', 1);
    sendCommand(command, device);
}

void AmplifierSerial::getGainLvl(const QString &device)
{
    sendCommand("VVA_LEVEL?", device);
}

void AmplifierSerial::setGainLvl(double level, const QString &device)
{
    QString command = QString("VVA_LEVEL %1").arg(level, 0, 'f', 1);
    sendCommand(command, device);
}

void AmplifierSerial::sendAckFaults(const QString &device)
{
    sendCommand("ACK_FAULTS", device);
}

void AmplifierSerial::getFaults(const QString &device)
{
    sendCommand("FAULTS?", device);
}

void AmplifierSerial::getSerialId(const QString &device)
{
    sendCommand("SERIAL?", device);
}

void AmplifierSerial::getModelId(const QString &device)
{
    sendCommand("MODEL?", device);
}

void AmplifierSerial::handleReadyRead()
{
    // Determine which port emitted the signal.
    QSerialPort *port = qobject_cast<QSerialPort*>(sender());
    if (!port)
        return;

    QString device = port->objectName();  // Retrieve the stored system location.
    QByteArray data = port->readAll();
    QString output = QString::fromUtf8(data);
    qDebug() << "Received from" << device << ":" << output;

    // If the output contains an error message, emit ampError.
    if (output.contains("ERROR:"))
        emit ampError(device, output);

    // Always emit the output signal.
    emit ampOutput(device, output);
}
