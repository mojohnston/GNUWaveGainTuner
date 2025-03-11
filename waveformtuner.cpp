#include "waveformtuner.h"
#include "amplifierserial.h"
#include "pythoneditor.h"
#include "pythonrunner.h"
#include <QTimer>
#include <QDebug>
#include <QtMath>

WaveformTuner::WaveformTuner(QObject *parent)
    : QObject(parent),
    m_currentGain(10), // initial gain (adjust as needed)
    m_channel(0),
    m_ampSerial(new AmplifierSerial(this)),
    m_pythonEditor(new PythonEditor(this)),
    m_pythonRunner(nullptr),
    m_delayTimer(new QTimer(this)),
    m_state(Idle)
{
    m_delayTimer->setSingleShot(true);
    // Connect amplifier signals.
    connect(m_ampSerial, &AmplifierSerial::ampOutput,
            this, &WaveformTuner::onAmpOutput);
    connect(m_ampSerial, &AmplifierSerial::ampError,
            this, &WaveformTuner::onAmpFault);
}

WaveformTuner::~WaveformTuner()
{
    // Objects with parent will be deleted automatically.
}

void WaveformTuner::startTuning(
    const QString &waveformFile,
    const QString &ampModel,
    int minPower,
    int maxPower,
    const QString &critical)
{
    m_waveformFile = waveformFile;
    m_ampModel = ampModel;
    m_minPower = minPower;
    m_maxPower = maxPower;
    m_critical = critical;
    m_currentGain = 10; // starting gain.

    // Discover amp devices via serial.
    m_ampSerial->searchAndConnect();
    m_allAmpDevices = m_ampSerial->connectedDevices();
    if (m_allAmpDevices.isEmpty()) {
        emit tuningFailed("No amplifier devices found.");
        return;
    }
    qDebug() << "Found amp devices:" << m_allAmpDevices;

    // Clear any previous readings.
    m_ampReadings.clear();
    m_stableCounts.clear();

    // Create a PythonRunner for the waveform file.
    m_pythonRunner = new PythonRunner(m_waveformFile, this);

    // Begin the tuning state machine.
    transitionToState(SetInitialGain);
}

void WaveformTuner::transitionToState(TuningState newState)
{
    m_state = newState;
    qDebug() << "Transition to state:" << m_state;

    switch(m_state) {
    case SetInitialGain:
        // Step 1: Set initial gain in python file.
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to set initial gain in python file.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    case StartWaveform:
        // Step 2: Start the waveform.
        m_pythonRunner->startScript();
        transitionToState(SetModeVVA_All);
        break;
    case SetModeVVA_All:
        // Step 3: Set mode to VVA on ALL discovered amps.
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampSerial->setMode("VVA", dev);
        }
        transitionToState(SetGain100_All);
        break;
    case SetGain100_All:
        // Step 4: Set gain level to 100 on ALL discovered amps.
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampSerial->setGainLvl(100, dev);
        }
        // Wait 5 seconds for waveform to boot.
        m_delayTimer->singleShot(5000, this, SLOT(proceedAfterDelay()));
        break;
    case QueryFwdPwr:
        // Step 5: Query forward power.
        // Clear previous readings.
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampReadings[dev].clear();
            m_stableCounts[dev] = 0;
        }
        // Query once for each amp.
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        // Schedule next query after 300ms.
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        break;
    case WaitForStable:
        // Check if at least one amp has stable readings.
        {
            bool stableFound = false;
            // For each amp, if we have 10 readings, compute the average.
            for (const QString &dev : qAsConst(m_allAmpDevices)) {
                if (m_ampReadings[dev].size() >= 10) {
                    double sum = 0;
                    for (double v : qAsConst(m_ampReadings[dev]))
                        sum += v;
                    double avg = sum / m_ampReadings[dev].size();
                    qDebug() << "Device" << dev << "rolling average:" << avg;
                    // If reading is not 0 and stable within ±0.05 of target:
                    if (avg > 0 && qAbs(avg - m_maxPower) <= 0.05) {
                        m_stableCounts[dev]++;
                        if (m_stableCounts[dev] >= 3) {
                            // Mark this device as testing amp.
                            if (!m_testingAmpDevices.contains(dev))
                                m_testingAmpDevices.append(dev);
                        }
                    } else {
                        m_stableCounts[dev] = 0;
                    }
                }
            }
            // If at least one testing amp is found, move on.
            if (!m_testingAmpDevices.isEmpty()) {
                transitionToState(StopWaveform);
                return;
            }
        }
        // Otherwise, continue querying.
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        break;
    case StopWaveform:
        // Step 6: Stop the waveform.
        m_pythonRunner->stopScript();
        transitionToState(ComparePower);
        break;
    case ComparePower: {
        // Step 7: Compare desired max power and measured average from testing amps.
        double total = 0;
        int count = 0;
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            if (!m_ampReadings[dev].isEmpty()) {
                double sum = 0;
                for (double v : qAsConst(m_ampReadings[dev]))
                    sum += v;
                total += (sum / m_ampReadings[dev].size());
                count++;
            }
        }
        double avg = (count > 0) ? total / count : 0;
        double diff = m_maxPower - avg;
        qDebug() << "ComparePower: desired:" << m_maxPower << "avg:" << avg << "diff:" << diff;
        if (diff > 0.1) {
            transitionToState(AdjustGainUp);
        } else if (diff < -0.1) {
            transitionToState(AdjustGainDown);
        } else {
            transitionToState(SetModeALC);
        }
        break;
    }
    case AdjustGainUp:
        m_currentGain++; // increment gain
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to increment gain.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    case AdjustGainDown:
        m_currentGain--; // decrement gain
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to decrement gain.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    case SetModeALC:
        // Step 8c: Set mode to ALC on the testing amp(s)
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->setMode("ALC", dev);
        }
        transitionToState(SetAlcLevel);
        break;
    case SetAlcLevel:
        // Step 9: Set ALC level to the user-specified minimum power on testing amp(s)
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->setAlcLvl(m_minPower, dev);
        }
        transitionToState(QueryFwdPwrALC);
        break;
    case QueryFwdPwrALC:
        // Step 10: Query forward power in ALC mode.
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        transitionToState(WaitForAlcStable);
        break;
    case WaitForAlcStable:
    {
        bool stableFound = false;
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            if (m_ampReadings[dev].size() >= 10) {
                double sum = 0;
                for (double v : qAsConst(m_ampReadings[dev]))
                    sum += v;
                double avg = sum / m_ampReadings[dev].size();
                qDebug() << "ALC Device" << dev << "rolling average:" << avg;
                if (qAbs(avg - m_minPower) <= 0.1) {
                    stableFound = true;
                }
            }
        }
        if (stableFound) {
            transitionToState(FinalizeTuning);
            return;
        }
    }
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        break;
    case FinalizeTuning:
        // Step 11: Finalize tuning.
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->setMode("VVA", dev);
            m_ampSerial->setGainLvl(100, dev);
        }
        m_delayTimer->singleShot(5000, this, SLOT(proceedAfterDelay()));
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        qDebug() << "Tuning complete. Final forward power logged.";
        emit tuningFinished();
        break;
    case RetryAfterFault:
        // Step 11b: In case of fault.
        m_pythonRunner->stopScript();
        m_currentGain--; // Decrease gain.
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to adjust gain after fault.");
            return;
        }
        m_pythonRunner->startScript();
        transitionToState(SetModeALC);
        break;
    default:
        break;
    }
}

void WaveformTuner::proceedAfterDelay()
{
    // Simply call transitionToState with the current state to trigger the next step.
    transitionToState(m_state);
}

void WaveformTuner::onAmpOutput(const QString &device, const QString &output)
{
    // Parse the output to extract a numeric forward power reading.
    bool ok = false;
    double value = output.split(" ").first().toDouble(&ok);
    if (ok) {
        m_ampReadings[device].append(value);
        if (m_ampReadings[device].size() > 10)
            m_ampReadings[device].removeFirst();
    }
}

void WaveformTuner::onAmpFault(const QString &device, const QString &error)
{
    Q_UNUSED(device);
    qWarning() << "Fault detected:" << error;
    transitionToState(RetryAfterFault);
}

void WaveformTuner::resetRollingAverages()
{
    m_ampReadings.clear();
    m_stableCounts.clear();
}
