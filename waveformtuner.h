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

    // Start the tuning process with the following parameters:
    // waveformFile: the path to the python waveform file
    // ampModel: "x300" or "N321"
    // minPower: desired minimum power (ALC setpoint)
    // maxPower: desired maximum forward power
    // critical: "HIGH" or "LOW"
    void startTuning(const QString &waveformFile,
                     const QString &ampModel,
                     int minPower,
                     int maxPower,
                     const QString &critical);

signals:
    // Emitted when tuning is successfully finished.
    void tuningFinished();
    // Emitted when tuning fails.
    void tuningFailed(const QString &reason);

private slots:
    // Slot triggered by QTimer to proceed with the next state.
    void proceedAfterDelay();
    // Slot to handle amplifier output.
    void onAmpOutput(const QString &device, const QString &output);
    // Slot to handle amplifier fault.
    void onAmpFault(const QString &device, const QString &error);

private:
    enum TuningState {
        Idle,
        SetInitialGain,    // Use PythonEditor to set initial gain value in the waveform file.
        StartWaveform,     // Start waveform using PythonRunner.
        SetModeVVA_All,    // For every discovered amp, set mode to VVA.
        SetGain100_All,    // For every discovered amp, set gain level to 100.
        WaitInitial,       // Wait 5 seconds to let the waveform “boot up.”
        QueryFwdPwr,       // Query forward power repeatedly.
        WaitForStable,     // Wait until the rolling average (of 10 readings) is stable for 3 iterations.
        DetermineTestingAmps, // Decide which amp(s) are providing valid readings.
        StopWaveform,      // Stop the waveform.
        ComparePower,      // Compare measured power vs desired max power.
        AdjustGainUp,      // Increment gain and restart.
        AdjustGainDown,    // Decrement gain and restart.
        SetModeALC,        // Set amplifier mode to ALC.
        SetAlcLevel,       // Set ALC level to the user-specified minimum power.
        QueryFwdPwrALC,    // Query forward power while in ALC mode.
        WaitForAlcStable,  // Wait until the ALC forward power is stable.
        FinalizeTuning,    // Final adjustments and finish.
        RetryAfterFault    // In case of a fault, adjust gain and retry.
    };

    void transitionToState(TuningState newState);
    void resetRollingAverages();

    // User parameters.
    QString m_waveformFile;
    QString m_ampModel;      // "x300" or "N321"
    int m_minPower;          // Minimum power (ALC setpoint)
    int m_maxPower;          // Maximum forward power desired
    QString m_critical;      // "HIGH" or "LOW"

    int m_currentGain;       // Current gain value stored in the python file.
    // For now, we assume the python file has one channel (0). (You can expand later.)
    int m_channel;

    // Helper objects.
    AmplifierSerial *m_ampSerial;
    PythonEditor   *m_pythonEditor;
    PythonRunner   *m_pythonRunner; // Created when tuning starts.
    QStringList m_allAmpDevices;   // List of amp devices discovered.
    // After querying forward power, we decide which amp(s) will be used for testing.
    QStringList m_testingAmpDevices;

    QTimer *m_delayTimer;          // For inserting delays between states.

    // For forward power measurement.
    // We store rolling readings for each device.
    QMap<QString, QList<double>> m_ampReadings;
    QMap<QString, int> m_stableCounts;  // Count how many consecutive stable iterations per amp.

    TuningState m_state;
};

#endif // WAVEFORMTUNER_H
