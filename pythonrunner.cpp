#include "pythonrunner.h"
#include <QDebug>
#include <QDateTime>

PythonRunner::PythonRunner(const QString &scriptPath, QObject *parent)
    : QObject(parent),
    m_scriptPath(scriptPath),
    m_process(nullptr)
{
}

PythonRunner::~PythonRunner()
{
    stopScript();
    if(m_process)
        m_process->deleteLater();
}

void PythonRunner::createProcess()
{
    if(m_process)
        m_process->deleteLater();
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PythonRunner::handleReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonRunner::handleFinished);
}

void PythonRunner::startScript()
{
    createProcess();
    m_process->start(m_scriptPath, QStringList(), QIODevice::ReadWrite);
    if (!m_process->waitForStarted(3000)) {
        qWarning() << "Failed to start python script:" << m_scriptPath;
    } else {
        emit scriptStarted();
    }
}

void PythonRunner::stopScript()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
        emit scriptStopped();
    }
}

void PythonRunner::handleReadyRead()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    emit pythonOutput(output);

    for (const QChar &c : qAsConst(output)) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (c == 'U') {
            m_uTimes.append(now);
            if (m_uTimes.size() > 16)
                m_uTimes.removeFirst();
            if (m_uTimes.size() == 16) {
                qint64 window = m_uTimes.last() - m_uTimes.first();
                if (window < 500) {
                    qDebug() << "Detected 16 consecutive 'U' characters within" << window << "ms; stopping script.";
                    emit thresholdDetected("U", window);
                    stopScript();
                    m_uTimes.clear();
                    break;
                }
            }
        } else {
            m_uTimes.clear();
        }
        if (c == 'N') {
            m_nTimes.append(now);
            if (m_nTimes.size() > 16)
                m_nTimes.removeFirst();
            if (m_nTimes.size() == 16) {
                qint64 window = m_nTimes.last() - m_nTimes.first();
                if (window < 500) {
                    qDebug() << "Detected 16 consecutive 'N' characters within" << window << "ms; stopping script.";
                    emit thresholdDetected("N", window);
                    stopScript();
                    m_nTimes.clear();
                    break;
                }
            }
        } else {
            m_nTimes.clear();
        }
    }
}

void PythonRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit scriptFinished(exitCode, exitStatus);
}
