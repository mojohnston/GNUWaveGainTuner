#include "waveformtuner.h"
#include "amplifierserial.h"
#include "pythoneditor.h"
#include "pythonrunner.h"
#include <QTimer>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>

WaveformTuner::WaveformTuner(QObject *parent)
    : QObject(parent),
    m_currentGain(1),
    m_channel(0),
    m_ampSerial(new AmplifierSerial(this)),
    m_pythonEditor(new PythonEditor(this)),
    m_pythonRunner(nullptr),
    m_delayTimer(new QTimer(this)),
    m_state(Idle)
{
    m_delayTimer->setSingleShot(true);
    connect(m_ampSerial, &AmplifierSerial::ampOutput, this, &WaveformTuner::onAmpOutput);
    connect(m_ampSerial, &AmplifierSerial::ampError, this, &WaveformTuner::onAmpFault);
    qDebug() << "WaveformTuner constructed, initial state Idle";
}

WaveformTuner::~WaveformTuner()
{
}

void WaveformTuner::startTuning(const QString &waveformFile,
                                const QString &ampModel,
                                double minPower,
                                double maxPower,
                                const QString &critical)
{
    qDebug() << "startTuning() called with parameters:" << waveformFile << ampModel << minPower << maxPower << critical;
    m_waveformFile = waveformFile;
    m_ampModel = ampModel;
    m_minPower = minPower;
    m_maxPower = maxPower;
    m_critical = critical;
    m_currentGain = 1;

    qDebug() << "Searching for amplifier devices...";
    m_ampSerial->searchAndConnect();
    m_allAmpDevices = m_ampSerial->connectedDevices();
    qDebug() << "Connected amp devices:" << m_allAmpDevices;
    if (m_allAmpDevices.isEmpty()) {
        emit tuningFailed("No amplifier devices found.");
        return;
    }

    resetRollingAverages();
    qDebug() << "Creating PythonRunner for waveform file:" << m_waveformFile;
    m_pythonRunner = new PythonRunner(m_waveformFile, this);
    connect(m_pythonRunner, &PythonRunner::pythonOutput, this, &WaveformTuner::onPythonOutput);
    transitionToState(SetInitialGain);
}

void WaveformTuner::resetRollingAverages()
{
    qDebug() << "Resetting forward power readings.";
    m_ampReadings.clear();
    m_testingAmpDevices.clear();
}

void WaveformTuner::transitionToState(TuningState newState)
{
    m_state = newState;
    qDebug() << "Transition to state:" << m_state;

    switch(m_state) {
    case SetInitialGain:
    {
        qDebug() << "Step 1: Setting initial gain to" << m_currentGain;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to set initial gain in python file.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    }
    case StartWaveform:
    {
        qDebug() << "Step 2: Starting waveform (VVA phase) via PythonRunner.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt);
        break;
    }
    case WaitForPythonPrompt:
    {
        qDebug() << "Waiting for Python prompt 'Press Enter to quit:' (VVA phase)...";
        break;
    }
    case SetModeVVA_All:
    {
        qDebug() << "Step 3: Setting mode VVA on all amps.";
        const int count = m_allAmpDevices.size();
        for (int i = 0; i < count; ++i) {
            const QString &dev = m_allAmpDevices.at(i);
            m_ampSerial->setMode("VVA", dev);
        }
        transitionToState(SetGain100_All);
        break;
    }
    case SetGain100_All:
    {
        qDebug() << "Step 4: Setting gain level to 100 on all amps.";
        const int count = m_allAmpDevices.size();
        for (int i = 0; i < count; ++i) {
            const QString &dev = m_allAmpDevices.at(i);
            m_ampSerial->setGainLvl(100, dev);
        }
        qDebug() << "Waiting 5 seconds for waveform boot-up (VVA phase).";
        m_delayTimer->singleShot(5000, this, SLOT(proceedAfterDelay()));
        transitionToState(QueryFwdPwr);
        break;
    }
    case QueryFwdPwr:
    {
        qDebug() << "Step 5: Querying FWD_PWR (VVA mode).";
        const int count = m_allAmpDevices.size();
        for (int i = 0; i < count; ++i) {
            const QString &dev = m_allAmpDevices.at(i);
            m_ampReadings[dev].clear();
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(1000, this, SLOT(proceedAfterDelay()));
        transitionToState(WaitForStable);
        break;
    }
    case WaitForStable:
    {
        bool stableFound = false;
        qDebug() << "Step 5 (continued): Checking for stable FWD_PWR (3 consecutive identical readings).";
        const int count = m_allAmpDevices.size();
        for (int i = 0; i < count; ++i) {
            const QString &dev = m_allAmpDevices.at(i);
            int numReadings = m_ampReadings[dev].size();
            if (numReadings < 3) {
                qDebug() << "Device" << dev << "does not yet have 3 readings; count:" << numReadings;
            } else {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                qDebug() << "Device" << dev << "last three readings:" << lastThree;
                if (qAbs(lastThree[0] - lastThree[1]) < 0.01 &&
                    qAbs(lastThree[1] - lastThree[2]) < 0.01) {
                    if (!m_testingAmpDevices.contains(dev))
                        m_testingAmpDevices.append(dev);
                    stableFound = true;
                }
            }
        }
        if (stableFound) {
            transitionToState(StopWaveform);
            return;
        }
        {
            const int count = m_allAmpDevices.size();
            for (int i = 0; i < count; ++i)
                m_ampSerial->getFwdPwr(m_allAmpDevices.at(i));
        }
        m_delayTimer->singleShot(1000, this, SLOT(proceedAfterDelay()));
        break;
    }
    case StopWaveform:
    {
        qDebug() << "Step 6: Stopping waveform (VVA phase).";
        m_pythonRunner->stopScript();
        transitionToState(WaitForPythonStop);
        break;
    }
    case WaitForPythonStop:
    {
        qDebug() << "Waiting 3 seconds for python script to stop (VVA phase).";
        QTimer::singleShot(3000, this, [this](){ transitionToState(ComparePower); });
        break;
    }
    case ComparePower:
    {
        qDebug() << "Step 7: Comparing target max power" << m_maxPower << "with measured power (VVA phase).";
        double total = 0;
        int count = 0;
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i) {
            const QString &dev = m_testingAmpDevices.at(i);
            int numReadings = m_ampReadings[dev].size();
            if (numReadings >= 3) {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                double sum = 0;
                for (int j = 0; j < lastThree.size(); ++j)
                    sum += lastThree.at(j);
                double devAvg = sum / lastThree.size();
                total += devAvg;
                ++count;
            }
        }
        double avg = (count > 0) ? total / count : 0;
        double diff = m_maxPower - avg;
        qDebug() << "Measured average:" << avg << "Difference:" << diff;
        // Accept if measured power is within 0.1 dBm below target or up to 0.3 dBm above.
        if (diff > 0.1)
            transitionToState(AdjustGainUp);
        else if (diff < -0.3)
            transitionToState(AdjustGainDown);
        else
            transitionToState(SetModeALC);
        break;
    }
    case AdjustGainUp:
    {
        qDebug() << "Step 8a: Incrementing gain. New gain:" << (m_currentGain + 1);
        m_currentGain++;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to increment gain.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    }
    case AdjustGainDown:
    {
        qDebug() << "Step 8b: Decrementing gain. New gain:" << (m_currentGain - 1);
        m_currentGain--;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to decrement gain.");
            return;
        }
        transitionToState(StartWaveform);
        break;
    }
    case SetModeALC:
    {
        qDebug() << "Step 8c: Setting mode ALC on testing amps.";
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i)
            m_ampSerial->setMode("ALC", m_testingAmpDevices.at(i));
        // Transition into ALC phase: restart the python waveform
        transitionToState(StartWaveform_ALC);
        break;
    }
    case StartWaveform_ALC:
    {
        qDebug() << "Step 9: Starting waveform (ALC phase) via PythonRunner.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt_ALC);
        break;
    }
    case WaitForPythonPrompt_ALC:
    {
        qDebug() << "Waiting for Python prompt 'Press Enter to quit:' (ALC phase)...";
        break;
    }
    case SetAlcLevel:
    {
        qDebug() << "Step 10: Setting ALC level to" << m_minPower;
        // Clear previous FWD_PWR readings for ALC phase.
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i)
            m_ampReadings[m_testingAmpDevices.at(i)].clear();
        for (int i = 0; i < testCount; ++i)
            m_ampSerial->setAlcLvl(m_minPower, m_testingAmpDevices.at(i));
        transitionToState(QueryFwdPwrALC);
        break;
    }
    case QueryFwdPwrALC:
    {
        qDebug() << "Step 11: Querying FWD_PWR in ALC mode.";
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i)
            m_ampSerial->getFwdPwr(m_testingAmpDevices.at(i));
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        transitionToState(WaitForAlcStable);
        break;
    }
    case WaitForAlcStable:
    {
        bool stableFound = false;
        qDebug() << "Step 11 (continued): Checking for stable ALC FWD_PWR (3 consecutive readings).";
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i) {
            const QString &dev = m_testingAmpDevices.at(i);
            int numReadings = m_ampReadings[dev].size();
            if (numReadings >= 3) {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                double avg = (lastThree[0] + lastThree[1] + lastThree[2]) / 3.0;
                qDebug() << "ALC Device" << dev << "average:" << avg;
                if (qAbs(avg - m_minPower) <= 0.2)
                    stableFound = true;
                if (avg - m_minPower > 0.2) {
                    stableFound = false;
                    break;
                }
            }
        }
        if (stableFound) {
            transitionToState(FinalizeTuning);
            return;
        }
        const int testCount2 = m_testingAmpDevices.size();
        for (int i = 0; i < testCount2; ++i)
            m_ampSerial->getFwdPwr(m_testingAmpDevices.at(i));
        m_delayTimer->singleShot(300, this, SLOT(proceedAfterDelay()));
        break;
    }
    case FinalizeTuning:
    {
        qDebug() << "Step 12a: Finalizing tuning.";
        const int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i) {
            const QString &dev = m_testingAmpDevices.at(i);
            m_ampSerial->setMode("VVA", dev);
            m_ampSerial->setGainLvl(100, dev);
        }
        qDebug() << "Waiting 5 seconds for final stabilization.";
        m_delayTimer->singleShot(5000, this, SLOT(proceedAfterDelay()));
        for (int i = 0; i < testCount; ++i)
            m_ampSerial->getFwdPwr(m_testingAmpDevices.at(i));
        qDebug() << "Tuning complete. Final forward power logged.";
        m_pythonRunner->stopScript();
        emit tuningFinished();
        break;
    }
    case RetryAfterFault:
    {
        qDebug() << "Step 12b: Fault encountered. Retrying after fault.";
        m_pythonRunner->stopScript();
        m_currentGain--;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to adjust gain after fault.");
            return;
        }
        m_pythonRunner->startScript();
        transitionToState(SetModeALC);
        break;
    }
    default:
        qDebug() << "Unknown state encountered.";
        break;
    }
}

void WaveformTuner::proceedAfterDelay()
{
    qDebug() << "Delay complete. Proceeding with state:" << m_state;
    transitionToState(m_state);
}

void WaveformTuner::onAmpOutput(const QString &device, const QString &output)
{
    qDebug() << "onAmpOutput from" << device << ":" << output;
    static const QRegularExpression rx("([-+]?\\d*\\.?\\d+)");
    QRegularExpressionMatch match = rx.match(output);
    bool ok = false;
    double value = 0.0;
    if (match.hasMatch())
        value = match.captured(1).toDouble(&ok);
    else
        qDebug() << "No numeric value found in output:" << output;
    if (ok) {
        m_ampReadings[device].append(value);
        qDebug() << "Appended reading" << value << "for device" << device
                 << "; total count:" << m_ampReadings[device].size();
        if (m_ampReadings[device].size() > 10)
            m_ampReadings[device].removeFirst();
    } else {
        qDebug() << "Failed to parse FWD_PWR reading from output:" << output;
    }
}

void WaveformTuner::onAmpFault(const QString &device, const QString &error)
{
    Q_UNUSED(device);
    qWarning() << "Fault detected:" << error;
    transitionToState(RetryAfterFault);
}

void WaveformTuner::onPythonOutput(const QString &output)
{
    qDebug() << "onPythonOutput:" << output;
    // For both VVA and ALC phases, wait until the prompt is received.
    if ((m_state == WaitForPythonPrompt || m_state == WaitForPythonPrompt_ALC) &&
        output.contains("Press Enter to quit:"))
    {
        qDebug() << "Python prompt detected. Waiting 5 seconds before proceeding.";
        m_delayTimer->singleShot(5000, this, SLOT(proceedAfterDelay()));
        if (m_state == WaitForPythonPrompt)
            transitionToState(SetModeVVA_All);
        else
            transitionToState(SetAlcLevel);
    }
}
