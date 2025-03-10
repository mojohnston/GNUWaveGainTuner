#ifndef WAVEFORMTUNER_H
#define WAVEFORMTUNER_H

#include <QObject>

class WaveformTuner : public QObject
{
    Q_OBJECT
public:
    explicit WaveformTuner(QObject *parent = nullptr);

signals:
};

#endif // WAVEFORMTUNER_H
