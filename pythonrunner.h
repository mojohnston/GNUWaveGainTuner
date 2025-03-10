#ifndef PYTHONRUNNER_H
#define PYTHONRUNNER_H

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>

class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(const QString &scriptPath, QObject *parent = nullptr);
    ~PythonRunner();

    // Starts the python script (using the shebang, so the script must be executable).
    // #!/usr/bin/env python3
    void startScript();
    // Stops the running python script.
    void stopScript();

signals:
    // Emitted when new output is received from the python script.
    void pythonOutput(const QString &output);
    // Emitted when the python script finishes.
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    // Handles incoming output from the process.
    void handleReadyRead();
    // Handles when the process finishes.
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString m_scriptPath;
    QProcess *m_process;

    // Counters and timers to track consecutive characters.
    int m_uCount;        // Consecutive 'U' count
    int m_nCount;        // Consecutive 'N' count
    QElapsedTimer m_uTimer; // The amount of time for an acceptable amount of 'U'
    QElapsedTimer m_nTimer; // The amount of time for an acceptable amount of 'N'
};

#endif // PYTHONRUNNER_H
