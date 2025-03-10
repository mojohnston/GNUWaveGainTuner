#include "pythonrunner.h"
#include <QDebug>

PythonRunner::PythonRunner(const QString &scriptPath, QObject *parent)
    : QObject(parent),
    m_scriptPath(scriptPath),
    m_process(new QProcess(this)),
    m_uCount(0),
    m_nCount(0)
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
        // Launch the script file directly.
        m_process->start(m_scriptPath);
        if (!m_process->waitForStarted(3000)) {
            qWarning() << "Failed to start python script:" << m_scriptPath;
        } else {
            qDebug() << "Started python script:" << m_scriptPath;
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
    }
}

void PythonRunner::handleReadyRead()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    qDebug() << "Python output:" << output;
    emit pythonOutput(output);

    // Process each character to check for consecutive U's and N's.
    for (const QChar &c : qAsConst(output)) {
        // Process U's: 8 consecutive U's within 1 second.
        if (c == 'U') {
            if (m_uCount == 0) {
                m_uTimer.restart(); // start timer when sequence begins.
            }
            m_uCount++;
            if (m_uCount >= 8 && m_uTimer.elapsed() <= 1000) {
                qDebug() << "Detected 8 consecutive 'U's within 1 second; stopping script.";
                stopScript();
                m_uCount = 0;
                break;
            }
            if (m_uTimer.elapsed() > 1000) {
                m_uCount = 1;
                m_uTimer.restart();
            }
        } else {
            m_uCount = 0;
        }

        // Process N's: 10 consecutive N's within 2 seconds.
        if (c == 'N') {
            if (m_nCount == 0) {
                m_nTimer.restart();
            }
            m_nCount++;
            if (m_nCount >= 10 && m_nTimer.elapsed() <= 2000) {
                qDebug() << "Detected 10 consecutive 'N's within 2 seconds; stopping script.";
                stopScript();
                m_nCount = 0;
                break;
            }
            if (m_nTimer.elapsed() > 2000) {
                m_nCount = 1;
                m_nTimer.restart();
            }
        } else {
            m_nCount = 0;
        }
    }
}

void PythonRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Python script finished with exit code:" << exitCode
             << "and exit status:" << exitStatus;
    emit scriptFinished(exitCode, exitStatus);
}
