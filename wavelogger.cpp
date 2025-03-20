#include "wavelogger.h"
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QFileInfo>

WaveLogger::WaveLogger(QObject *parent)
    : QObject(parent)
{
    QString dateStr = QDateTime::currentDateTimeUtc().toString("MM-dd-yy");
    QString baseName = "waveLog-" + dateStr;
    QString fileName;
    int counter = 1;
    do {
        fileName = baseName + QString("-%1.txt").arg(counter);
        counter++;
    } while (QFile::exists(fileName));

    m_logFile.setFileName(fileName);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Could not open log file:" << fileName;
    }
}

WaveLogger::~WaveLogger()
{
    if (m_logFile.isOpen())
        m_logFile.close();
}

QString WaveLogger::formatMessage(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTimeUtc().toString("MM-dd-yy HH:mm:ss");
    return QString("<%1 Z> %2").arg(timestamp, msg);
}

void WaveLogger::debug(const QString &msg)
{
    qDebug().noquote() << formatMessage(msg);
}

void WaveLogger::logToFile(const QString &msg)
{
    if (m_logFile.isOpen()) {
        QTextStream out(&m_logFile);
        out << formatMessage(msg) << "\n";
        m_logFile.flush();
    } else {
        qWarning() << "Log file is not open.";
    }
}

void WaveLogger::debugAndLog(const QString &msg)
{
    debug(msg);
    logToFile(msg);
}
