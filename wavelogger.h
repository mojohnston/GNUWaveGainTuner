#ifndef WAVELOGGER_H
#define WAVELOGGER_H

#include <QObject>
#include <QFile>
#include <QString>

class WaveLogger : public QObject
{
    Q_OBJECT
public:
    explicit WaveLogger(QObject *parent = nullptr);
    ~WaveLogger();

    void debug(const QString &msg);
    void logToFile(const QString &msg);
    void debugAndLog(const QString &msg);

private:
    QString formatMessage(const QString &msg);
    QFile m_logFile;
};

#endif // WAVELOGGER_H
