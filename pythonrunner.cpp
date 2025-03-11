#include "pythonrunner.h"
#include <QDebug>
#include <QDateTime>

PythonRunner::PythonRunner(const QString &scriptPath, QObject *parent)
    : QObject(parent),
    m_scriptPath(scriptPath),
    m_process(new QProcess(this))
{
    // Connect signals to slots.
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PythonRunner::handleReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonRunner::handleFinished);
}

PythonRunner::~PythonRunner()
{
    stopScript();
}

void PythonRunner::startScript()
{
    if (m_process->state() == QProcess::NotRunning) {
        // Launch the script file directly (make sure it is executable).
        m_process->start(m_scriptPath);
        if (!m_process->waitForStarted(3000)) {
            qWarning() << "Failed to start python script:" << m_scriptPath;
        } else {
            qDebug() << "Started python script:" << m_scriptPath;
            emit scriptStarted();
        }
    }
}

void PythonRunner::stopScript()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        // Wait a short time; if it doesn't stop, force kill it.
        if (!m_process->waitForFinished(500))
            m_process->kill();
        qDebug() << "Python script stopped.";
        emit scriptStopped();
    }
}

void PythonRunner::handleReadyRead()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    qDebug() << "Python output:" << output;
    emit pythonOutput(output);

    // Process each character individually to detect thresholds.
    for (const QChar &c : qAsConst(output)) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // Process 'U' characters.
        if (c == 'U') {
            m_uTimes.append(now);
            if (m_uTimes.size() > 16)
                m_uTimes.removeFirst();
            if (m_uTimes.size() == 16) {
                qint64 window = m_uTimes.last() - m_uTimes.first();
                if (window < 500) {  // 16 U's within 500ms.
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

        // Process 'N' characters.
        if (c == 'N') {
            m_nTimes.append(now);
            if (m_nTimes.size() > 16)
                m_nTimes.removeFirst();
            if (m_nTimes.size() == 16) {
                qint64 window = m_nTimes.last() - m_nTimes.first();
                if (window < 500) {  // 16 N's within 500ms.
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
    qDebug() << "Python script finished with exit code:" << exitCode
             << "and exit status:" << exitStatus;
    emit scriptFinished(exitCode, exitStatus);
}
