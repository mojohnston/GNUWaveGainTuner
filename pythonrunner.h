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

    void startScript();
    void stopScript();

signals:
    void pythonOutput(const QString &output);
    void scriptFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void scriptStarted();
    void scriptStopped();
    void thresholdDetected(const QString &marker, qint64 window);

private slots:
    void handleReadyRead();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void createProcess();

    QString m_scriptPath;
    QProcess *m_process;
    QList<qint64> m_uTimes;
    QList<qint64> m_nTimes;
};

#endif // PYTHONRUNNER_H
