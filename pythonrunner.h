#ifndef PYTHONRUNNER_H
#define PYTHONRUNNER_H

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QList>

class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(const QString &scriptPath, QObject *parent = nullptr);
    ~PythonRunner();

    // The file must be executable with a proper shebang
    // #!/usr/bin/env python3
    void startScript();
    void stopScript();

signals:
    void pythonOutput(const QString &output);
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void scriptStarted();
    void scriptStopped();
    // Emitted when a threshold is detected.
    // marker: "U" or "N"; window: the measured window in ms.
    void thresholdDetected(const QString &marker, qint64 window);

private slots:
    void handleReadyRead();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString m_scriptPath;
    QProcess *m_process;

    QList<qint64> m_uTimes;
    QList<qint64> m_nTimes;
};

#endif // PYTHONRUNNER_H
