#include "amplifierserial.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QDebug>
#include <QDir>

AmplifierSerial::AmplifierSerial(QObject *parent)
    : QObject(parent)
{
}

AmplifierSerial::~AmplifierSerial()
{
    for (QSerialPort *port : qAsConst(m_ports)) {
        if (port->isOpen())
            port->close();
        delete port;
    }
    m_ports.clear();
}

void AmplifierSerial::searchAndConnect()
{
    const auto availablePorts = QSerialPortInfo::availablePorts();

    // Scan /dev for symlinks matching our expected udev names.
    QDir devDir("/dev");
    QStringList filters;
    filters << "ttyUSB_*amp*"; // adjust filter as needed - this grabs all devices with 'amp' in their name - not case sensitive
    QFileInfoList symlinkList = devDir.entryInfoList(filters, QDir::NoDotAndDotDot | QDir::Files);
    QMap<QString, QString> symlinkMapping; // Maps target -> symlink name.
    for (const QFileInfo &info : symlinkList) {
        if (info.isSymLink()) {
            QString symlinkName = info.absoluteFilePath();
            QString target = info.symLinkTarget();
            qDebug() << "Found symlink:" << symlinkName << "->" << target;
            symlinkMapping.insert(target, symlinkName);
        }
    }

    // Use a regex to match any device name that contains "amp" (case-insensitive).
    static const QRegularExpression ampRegex("(?i).*amp.*");

    // Clear any existing connections.
    for (QSerialPort *port : qAsConst(m_ports)) {
        if (port->isOpen())
            port->close();
        delete port;
    }
    m_ports.clear();
    m_buffers.clear();

    // Loop over available serial ports.
    for (const QSerialPortInfo &info : availablePorts) {
        QString sysLoc = info.systemLocation();
        // If there's a symlink mapping for this target, use the symlink name.
        if (symlinkMapping.contains(sysLoc)) {
            sysLoc = symlinkMapping.value(sysLoc);
        }
        QRegularExpressionMatch match = ampRegex.match(sysLoc);
        if (match.hasMatch()) {
            qDebug() << "Found amp device:" << sysLoc;
            QSerialPort *port = new QSerialPort(info, this);
            port->setObjectName(sysLoc); // Save the device name (symlink name if available)

            // Set standard parameters for the amp.
            port->setBaudRate(QSerialPort::Baud9600);
            port->setDataBits(QSerialPort::Data8);
            port->setParity(QSerialPort::NoParity);
            port->setStopBits(QSerialPort::OneStop);
            port->setFlowControl(QSerialPort::NoFlowControl);
            if (port->open(QIODevice::ReadWrite)) {
                qDebug() << "Connected to amp:" << sysLoc;
                // Initialize the buffer for this device.
                m_buffers.insert(sysLoc, QByteArray());
                // Connect readyRead signal to our slot.
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

// Convenience amplifier commands:
void AmplifierSerial::getMode(const QString &device) { sendCommand("MODE?", device); }
void AmplifierSerial::setMode(const QString &mode, const QString &device) { sendCommand("MODE " + mode, device); }
void AmplifierSerial::setStandby(const QString &device) { sendCommand("STANDBY", device); }
void AmplifierSerial::setOnline(const QString &device) { sendCommand("ONLINE", device); }
void AmplifierSerial::getFwdPwr(const QString &device) { sendCommand("FWD_PWR?", device); }
void AmplifierSerial::getRevPwr(const QString &device) { sendCommand("REV_PWR?", device); }
void AmplifierSerial::getAlcLvl(const QString &device) { sendCommand("ALC_LEVEL?", device); }
void AmplifierSerial::setAlcLvl(double level, const QString &device)
{
    QString command = QString("ALC_LEVEL %1").arg(level, 0, 'f', 1);
    sendCommand(command, device);
}
void AmplifierSerial::getGainLvl(const QString &device) { sendCommand("VVA_LEVEL?", device); }
void AmplifierSerial::setGainLvl(double level, const QString &device)
{
    QString command = QString("VVA_LEVEL %1").arg(level, 0, 'f', 1);
    sendCommand(command, device);
}
void AmplifierSerial::sendAckFaults(const QString &device) { sendCommand("ACK_FAULTS", device); }
void AmplifierSerial::getFaults(const QString &device) { sendCommand("FAULTS?", device); }
void AmplifierSerial::getSerialId(const QString &device) { sendCommand("SERIAL?", device); }
void AmplifierSerial::getModelId(const QString &device) { sendCommand("MODEL?", device); }

void AmplifierSerial::handleReadyRead()
{
    QSerialPort *port = qobject_cast<QSerialPort*>(sender());
    if (!port)
        return;

    QString device = port->objectName();
    m_buffers[device].append(port->readAll());
    QString response = QString::fromUtf8(m_buffers[device]).trimmed();

    // If the response contains "dBm", assume the entire response is complete.
    if (response.contains("dBm")) {
        qDebug() << "Received from" << device << ":" << response;
        // currently, 'ERROR' is not a valid response from the amplifiers
        if (response.contains("ERROR:"))
            emit ampError(device, response);
        else
            emit ampOutput(device, response);
        m_buffers[device].clear();
    }
}

QStringList AmplifierSerial::connectedDevices() const
{
    return m_ports.keys();
}
