#ifndef WAVEFORMTUNER_H
#define WAVEFORMTUNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include "wavelogger.h"

class AmplifierSerial;
class PythonEditor;
class PythonRunner;
class QTimer;

class WaveformTuner : public QObject
{
    Q_OBJECT
public:
    explicit WaveformTuner(QObject *parent = nullptr, WaveLogger *logger = nullptr);
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
    int extractChannelFromFile(const QString &filePath);

private:
    // Final measured values for logging.
    double m_finalStableMin;
    double m_finalStableMax;

    int m_adjustDownCount = 0;
    double m_lastAvg = 0.0;

    int m_alcRangeCount = 0;
    double m_measuredMin;
    int m_gainStep;
    WaveLogger *m_logger = nullptr;
    int m_gainSwapCount = 0;
    int m_lastGainAdjustment = 0;
    int m_initialGain;

    enum TuningState {
        Idle,
        CheckAmpMode,
        InitialModeVVA,
        InitialVvaLevel,
        InitialModeALC,
        InitialAlcLevel,
        SetOnline,
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
        AdjustMinDown,
        StartWaveform_ALC,
        WaitForPythonPrompt_ALC,
        QueryFwdPwrALC,
        WaitForAlcStable,
        FinalizeTuning,
        RecheckMax,
        WaitForMaxStable,
        LogResults,
        RetryAfterFault
    };

    void transitionToState(TuningState newState);
    void resetRollingAverages();
    QStringList targetDevices() const; // Returns the amp devices for the current channel

    // User parameters.
    QString m_waveformFile;
    QString m_ampModel;      // "x300" or "N321"
    double m_minPower;       // Target minimum power (user provided)
    double m_maxPower;       // Target maximum power (user provided)
    QString m_critical;      // "HIGH" or "LOW"

    int m_currentGain;       // Current gain value (set in the python file)
    int m_channel;           // 0 or 1 (0 for L1, 1 for L2)
    bool m_isL1L2;         // True if tuning an L1_L2 file

    AmplifierSerial *m_ampSerial;
    PythonEditor   *m_pythonEditor;
    PythonRunner   *m_pythonRunner;
    QStringList m_allAmpDevices;    // All discovered amplifier devices
    QStringList m_testingAmpDevices; // Devices that responded stably

    QTimer *m_delayTimer;
    QMap<QString, QList<double>> m_ampReadings;
    TuningState m_state;
};

#endif // WAVEFORMTUNER_H
