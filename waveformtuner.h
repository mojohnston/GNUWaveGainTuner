#ifndef WAVEFORMTUNER_H
#define WAVEFORMTUNER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QList>

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

    // Now minPower and maxPower are doubles (e.g. 30.0, 48.2)
    void startTuning(const QString &waveformFile,
                     const QString &ampModel,
                     double minPower,
                     double maxPower,
                     const QString &critical);

signals:
    void tuningFinished();
    void tuningFailed(const QString &reason);

private slots:
    void proceedAfterDelay();
    void onAmpOutput(const QString &device, const QString &output);
    void onAmpFault(const QString &device, const QString &error);
    void onPythonOutput(const QString &output);

private:
    int m_gainSwapCount = 0;
    int m_lastGainAdjustment = 0;
    // Updated state enumeration (new states for ALC phase added)
    enum TuningState {
        Idle,
        SetInitialGain,         // Step 1
        StartWaveform,          // Step 2 (VVA phase)
        WaitForPythonPrompt,    // Step 2b (VVA phase)
        SetModeVVA_All,         // Step 3
        SetGain100_All,         // Step 4
        QueryFwdPwr,            // Step 5
        WaitForStable,          // Step 5 continued (VVA mode)
        StopWaveform,           // Step 6 (stop VVA waveform)
        WaitForPythonStop,      // Wait for VVA python script to stop
        ComparePower,           // Step 7 (compare VVA measured vs. target max)
        AdjustGainUp,           // Step 8a
        AdjustGainDown,         // Step 8b
        // Now transition into ALC phase:
        SetModeALC,             // Step 8c: set mode ALC on testing amps
        StartWaveform_ALC,      // Step 9: start python runner for ALC phase
        WaitForPythonPrompt_ALC,// Step 9b: wait for "Press Enter to quit:" in ALC phase and wait 5 sec
        SetAlcLevel,            // Step 10: set ALC level to desired min power
        QueryFwdPwrALC,         // Step 11: query FWD_PWR in ALC mode
        WaitForAlcStable,       // Step 11 continued (ALC mode stable check)
        FinalizeTuning,         // Step 12a: tuning complete – finalize
        RetryAfterFault         // Step 12b: fault encountered in ALC mode; adjust and retry
    };

    void transitionToState(TuningState newState);
    void resetRollingAverages();

    // User parameters – now using doubles for power values.
    QString m_waveformFile;
    QString m_ampModel;      // "x300" or "N321"
    double m_minPower;       // Minimum power (ALC setpoint)
    double m_maxPower;       // Maximum forward power desired
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
