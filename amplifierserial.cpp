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
    // List of candidate device names for amp connections. Need to check systems' udev rules
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

    // First, clear any existing connections.
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
            port->setObjectName(sysLoc); // Save system location as object's name.

            // Set standard parameters for the amp.
            port->setBaudRate(QSerialPort::Baud9600);
            port->setDataBits(QSerialPort::Data8);
            port->setParity(QSerialPort::NoParity);
            port->setStopBits(QSerialPort::OneStop);
            port->setFlowControl(QSerialPort::NoFlowControl);
            if (port->open(QIODevice::ReadWrite)) {
                qDebug() << "Connected to amp:" << sysLoc;
                // Connect the readyRead signal to our slot.
                connect(port, &QSerialPort::readyRead, this, &AmplifierSerial::handleReadyRead);
                m_ports.insert(sysLoc, port);
            } else {
                qWarning() << "Failed to open amp:" << sysLoc << ":" << port->errorString();
                delete port;
            }
        }
    }
}

void AmplifierSerial::checkStatus()
{
    // Send "STATUS" to each amp.
    for (auto it = m_ports.begin(); it != m_ports.end(); ++it) {
        QSerialPort *port = it.value();
        if (port->isOpen()) {
            port->write("STATUS\n");
            qDebug() << "Sent STATUS command to" << it.key();
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

void AmplifierSerial::sendStop(const QString &device)
{
    sendCommand("STOP", device);
}

void AmplifierSerial::sendALC(const QString &device)
{
    sendCommand("ALC", device);
}

void AmplifierSerial::sendGAIN(const QString &device)
{
    sendCommand("GAIN", device);
}

void AmplifierSerial::handleReadyRead()
{
    // Determine which port emitted the signal.
    QSerialPort *port = qobject_cast<QSerialPort*>(sender());
    if (!port)
        return;

    QString device = port->objectName();  // Use the object's name that we set earlier.
    QByteArray data = port->readAll();
    QString output = QString::fromUtf8(data);
    qDebug() << "Received from" << device << ":" << output;

    // If the output contains an error message, emit ampError.
    if (output.contains("ERROR:"))
        emit ampError(device, output);

    // Always emit the output signal.
    emit ampOutput(device, output);
}
