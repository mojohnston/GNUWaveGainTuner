#ifndef WAVEFORMTUNER_H
#define WAVEFORMTUNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QList>
#include "wavelogger.h"

class AmplifierSerial;
class PythonEditor;
class PythonRunner;
class QTimer;

class WaveformTuner : public QObject
{
    Q_OBJECT
public:
    explicit WaveformTuner(QObject *parent = nullptr);
    ~WaveformTuner();

    void startTuning(const QString &waveformFile,
                     const QString &ampModel,
                     double minPower,
                     double maxPower,
                     const QString &critical);

signals:
    void tuningFinished();
    void tuningFailed(const QString &reason);

private slots:
    void onAmpOutput(const QString &device, const QString &output);
    void onAmpFault(const QString &device, const QString &error);
    void onPythonOutput(const QString &output);

private:
    int m_alcRangeCount = 0;
    double m_measuredMin;
    int m_gainStep;
    WaveLogger *m_logger = nullptr;
    int m_gainSwapCount = 0;
    int m_lastGainAdjustment = 0;
    enum TuningState {
        Idle,
        CheckAmpMode,
        InitialModeVVA,
        InitialVvaLevel,
        InitialModeALC,
        InitialAlcLevel,        
        SetInitialGain,
        StartWaveform,
        WaitForPythonPrompt,
        SetModeVVA_All,
        SetGain100_All,
        QueryFwdPwr,
        WaitForStable,
        StopWaveform,
        ComparePower,
        AdjustGainUp,
        AdjustGainDown,
        SetModeALC,
        PreSetAlc,
        StartWaveform_ALC,
        WaitForPythonPrompt_ALC,
        SetAlcLevel,
        QueryFwdPwrALC,
        WaitForAlcStable,
        FinalizeTuning,
        AdjustMinDown,
        LogResults,
        RetryAfterFault
    };

    void transitionToState(TuningState newState);
    void resetRollingAverages();

    // User parameters – now using doubles for power values.
    QString m_waveformFile;
    QString m_ampModel;      // "x300" or "N321"
    double m_minPower;       // Minimum forward power
    double m_maxPower;       // Maximum forward power
    QString m_critical;      // "HIGH" or "LOW"

    int m_currentGain;       // Current gain (stored in python file)
    int m_channel;           // (Assumed 0 for now.)

    AmplifierSerial *m_ampSerial;
    PythonEditor   *m_pythonEditor;
    PythonRunner   *m_pythonRunner;
    QStringList m_allAmpDevices;   // Discovered amplifier devices
    QStringList m_testingAmpDevices; // Devices that responded stably

    QTimer *m_delayTimer;
    QMap<QString, QList<double>> m_ampReadings;
    TuningState m_state;
};

#endif // WAVEFORMTUNER_H
