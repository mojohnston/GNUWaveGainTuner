#include "waveformtuner.h"
#include "amplifierserial.h"
#include "pythoneditor.h"
#include "pythonrunner.h"
#include "wavelogger.h"
#include <QTimer>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <QFileInfo>

// Constructor
WaveformTuner::WaveformTuner(QObject *parent, WaveLogger *logger)
    : QObject(parent),
    m_currentGain(1),
    m_channel(0),
    m_ampSerial(new AmplifierSerial(this)),
    m_pythonEditor(new PythonEditor(this)),
    m_pythonRunner(nullptr),
    m_delayTimer(new QTimer(this)),
    m_state(Idle),
    m_logger(logger ? logger : new WaveLogger(this)),
    m_gainStep(1),
    m_gainSwapCount(0),
    m_lastGainAdjustment(0),
    m_measuredMin(0.0),
    m_alcRangeCount(0),
    m_finalStableMin(0.0),
    m_finalStableMax(0.0)
{
    m_delayTimer->setSingleShot(true);
    connect(m_ampSerial, &AmplifierSerial::ampOutput, this, &WaveformTuner::onAmpOutput);
    connect(m_ampSerial, &AmplifierSerial::ampError, this, &WaveformTuner::onAmpFault);
    qDebug() << "WaveformTuner constructed, initial state Idle";
}

// Destructor
WaveformTuner::~WaveformTuner()
{
    m_ampSerial->disconnectAll();
}

void WaveformTuner::startTuning(const QString &waveformFile,
                                const QString &ampModel,
                                double minPower,
                                double maxPower,
                                const QString &critical)
{
    qDebug() << "Starting tuning for" << waveformFile << "for" << ampModel
             << "with target min:" << minPower << "dBm and target max:" << maxPower
             << "dBm, favoring" << critical << "power.";
    m_waveformFile = waveformFile;
    m_ampModel = ampModel;
    m_minPower = minPower;
    m_maxPower = maxPower;
    m_critical = critical;

    if (ampModel.compare("x300", Qt::CaseInsensitive) == 0)
        m_currentGain = 0;
    else if (ampModel.compare("N321", Qt::CaseInsensitive) == 0)
        m_currentGain = 12;
    else
        m_currentGain = 0;

    m_ampSerial->disconnectAll();
    qDebug() << "Searching for amplifier devices...";
    m_ampSerial->searchAndConnect();
    m_allAmpDevices = m_ampSerial->connectedDevices();
    qDebug() << "Connected amp devices:" << m_allAmpDevices;
    if (m_allAmpDevices.isEmpty()) {
        emit tuningFailed("No amplifier devices found.");
        return;
    }

    resetRollingAverages();
    m_pythonRunner = new PythonRunner(m_waveformFile, this);
    connect(m_pythonRunner, &PythonRunner::pythonOutput, this, &WaveformTuner::onPythonOutput);
    m_delayTimer->singleShot(1000, this, [this](){ transitionToState(CheckAmpMode); });
}

void WaveformTuner::resetRollingAverages()
{
    m_ampReadings.clear();
    m_testingAmpDevices.clear();
}

void WaveformTuner::transitionToState(TuningState newState)
{
    m_state = newState;
    switch(m_state) {
    case CheckAmpMode:
        qDebug() << "Checking amplifier status...";
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampSerial->sendCommand("MODE?", dev);
        }
        break;
    case InitialModeVVA:
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setMode("VVA", dev);
        m_delayTimer->singleShot(1200, this, [this](){ transitionToState(InitialVvaLevel); });
        break;
    case InitialVvaLevel:
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setGainLvl(100, dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(InitialModeALC); });
        break;
    case InitialModeALC:
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setMode("ALC", dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(InitialAlcLevel); });
        break;
    case InitialAlcLevel:
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setAlcLvl(m_minPower, dev);
        // Wait a bit, then transition to the new SetOnline state.
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetOnline); });
        break;

    case SetOnline:
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->sendCommand("ONLINE", dev);
        // After a short delay, move to the next state.
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(SetInitialGain); });
        break;
    case SetInitialGain:
        qDebug() << "Step 1: Setting initial gain to" << m_currentGain << "dBm.";
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to set initial gain in python file.");
            return;
        }
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(StartWaveform); });
        break;
    case StartWaveform:
        qDebug() << "Step 2: Starting waveform.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt);
        break;
    case WaitForPythonPrompt:
        qDebug() << "Waiting for waveform to start...";
        break;
    case SetModeVVA_All:
        qDebug() << "Step 3: Setting mode VVA (Gain) on all amps.";
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setMode("VVA", dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetGain100_All); });
        break;
    case SetGain100_All:
        qDebug() << "Setting gain level to 100 on all amps.";
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampSerial->setGainLvl(100, dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(QueryFwdPwr); });
        break;
    case QueryFwdPwr:
        qDebug() << "Step 4: Querying forward power in VVA (gain) mode.";
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            m_ampReadings[dev].clear();
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(WaitForStable); });
        break;
    case WaitForStable: {
        bool stableFound = false;
        qDebug() << "Checking for stable forward power...";
        for (const QString &dev : qAsConst(m_allAmpDevices)) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings < 3)
                ;
            else {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                if (qAbs(lastThree[0] - lastThree[1]) < 0.01 &&
                    qAbs(lastThree[1] - lastThree[2]) < 0.01) {
                    if (!m_testingAmpDevices.contains(dev))
                        m_testingAmpDevices.append(dev);
                    stableFound = true;
                }
            }
        }
        if (stableFound)
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(StopWaveform); });
        else {
            for (const QString &dev : qAsConst(m_allAmpDevices))
                m_ampSerial->getFwdPwr(dev);
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(WaitForStable); });
        }
        break;
    }
    case StopWaveform:
        qDebug() << "Step 5: Stopping waveform (VVA phase).";
        m_pythonRunner->stopScript();
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(ComparePower); });
        break;
    case ComparePower: {
        qDebug() << "Step 6: Comparing results to target" << m_maxPower << "dBm.";
        double total = 0;
        int count = 0;
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings >= 3) {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                double sum = 0;
                for (double r : lastThree)
                    sum += r;
                double devAvg = sum / lastThree.size();
                total += devAvg;
                ++count;
            }
        }
        double avg = (count > 0) ? total / count : 0;
        double diff = m_maxPower - avg;
        qDebug() << "Measured average:" << avg << "Difference:" << diff;
        if (diff > 0.1) {
            if (diff > 1.2)
                m_gainStep = 3;
            else if (diff > 0.6)
                m_gainStep = 2;
            else
                m_gainStep = 1;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(AdjustGainUp); });
        }
        else if (diff < -0.4) {
            m_gainStep = 1;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(AdjustGainDown); });
        }
        else {
            m_finalStableMax = avg;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetModeALC); });
        }
        break;
    }
    case AdjustGainUp:
        qDebug() << "Step 7: Increasing gain. New gain:" << (m_currentGain + m_gainStep);
        if (m_lastGainAdjustment != 1)
            m_gainSwapCount++;
        m_lastGainAdjustment = 1;
        m_currentGain += m_gainStep;
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampReadings[dev].clear();
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to increment gain.");
            return;
        }
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(StartWaveform); });
        break;
    case AdjustGainDown:
        qDebug() << "Step 7: Lowering gain. New gain:" << (m_currentGain - m_gainStep);
        if (m_lastGainAdjustment != -1)
            m_gainSwapCount++;
        m_lastGainAdjustment = -1;
        m_currentGain -= m_gainStep;
        for (const QString &dev : qAsConst(m_allAmpDevices))
            m_ampReadings[dev].clear();
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to decrement gain.");
            return;
        }
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(StartWaveform); });
        break;
    case SetModeALC:
        qDebug() << "Step 8: Setting up ALC test for minimum power.";
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampReadings[dev].clear();
            m_ampSerial->setMode("ALC", dev);
        }
        m_delayTimer->singleShot(1500, this, [this](){ transitionToState(PreSetAlc); });
        break;
    case PreSetAlc:
        qDebug() << "Setting ALC level to" << m_minPower << "dBm.";
        for (const QString &dev : qAsConst(m_testingAmpDevices))
            m_ampSerial->setAlcLvl(m_minPower, dev);
        m_delayTimer->singleShot(1500, this, [this](){ transitionToState(StartWaveform_ALC); });
        break;
    case StartWaveform_ALC:
        qDebug() << "Step 9: Starting waveform in ALC mode.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt_ALC);
        break;
    case WaitForPythonPrompt_ALC:
        qDebug() << "Waiting for waveform to start...";
        break;
    case QueryFwdPwrALC:
        qDebug() << "Step 10: Querying forward power in ALC mode.";
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(WaitForAlcStable); });
        break;
    case WaitForAlcStable: {
        const double tolerance = 0.2;
        bool allReady = true;
        double total = 0;
        int count = 0;
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings < 3) {
                allReady = false;
            } else {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                if (qAbs(lastThree[0] - lastThree[1]) < tolerance &&
                    qAbs(lastThree[1] - lastThree[2]) < tolerance) {
                    double avg = (lastThree[0] + lastThree[1] + lastThree[2]) / 3.0;
                    total += avg;
                    ++count;
                } else {
                    allReady = false;
                }
            }
        }
        if (!allReady || count == 0) {
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(QueryFwdPwrALC); });
            break;
        }
        double avgALC = total / count;
        if (m_critical.compare("LOW", Qt::CaseInsensitive) == 0 && ((avgALC - m_minPower) > 0.2)) {
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(AdjustMinDown); });
        } else {
            m_finalStableMin = avgALC;
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(FinalizeTuning); });
        }
        break;
    }

    case AdjustMinDown:
        qDebug() << "Adjusting minimum: lowering gain. New gain:" << (m_currentGain - 1);
        // Prevent gain from going below 0.
        if (m_currentGain <= 0) {
            qDebug() << "Gain is already 0. Cannot lower further.";
            if (m_logger) {
                m_logger->debugAndLog("Tuning failed: gain cannot be lowered further for LOW critical tuning.");
            }
            emit tuningFailed("Gain cannot be lowered further for LOW critical tuning.");
            return;
        }
        m_currentGain--;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to lower gain for LOW critical.");
            return;
        }
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampReadings[dev].clear();
        }
        m_pythonRunner->stopScript();
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(StartWaveform_ALC); });
        break;
    case FinalizeTuning: {
        qDebug() << "Step 11: Finalizing tuning.";
        int testCount = m_testingAmpDevices.size();
        for (int i = 0; i < testCount; ++i) {
            QString dev = m_testingAmpDevices.at(i);
            m_ampSerial->setMode("VVA", dev);
            m_delayTimer->singleShot(500, this, [this, dev]() {
                m_ampSerial->setGainLvl(100, dev);
                m_ampReadings[dev].clear();
            });
        }
        m_delayTimer->singleShot(2500, this, [this]() { transitionToState(RecheckMax); });
        break;
    }
    case RecheckMax: {
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            m_ampReadings[dev].clear();
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(WaitForMaxStable); });
        break;
    }
    case WaitForMaxStable: {
        const double tolerance = 0.01;
        bool allReady = true;
        double total = 0;
        int count = 0;
        for (const QString &dev : qAsConst(m_testingAmpDevices)) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings < 3) {
                allReady = false;
            } else {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                if (qAbs(lastThree[0] - lastThree[1]) < tolerance &&
                    qAbs(lastThree[1] - lastThree[2]) < tolerance) {
                    double avg = (lastThree[0] + lastThree[1] + lastThree[2]) / 3.0;
                    total += avg;
                    ++count;
                } else {
                    allReady = false;
                }
            }
        }
        if (!allReady || count == 0) {
            qDebug() << "Final maximum readings not yet stable, scheduling another query.";
            for (const QString &dev : qAsConst(m_testingAmpDevices)) {
                m_ampSerial->getFwdPwr(dev);
            }
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(WaitForMaxStable); });
            break;
        }
        double avgMax = total / count;
        m_finalStableMax = avgMax;
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(LogResults); });
        break;
    }
    case LogResults: {
        // Log the measured values.
        QFileInfo fileInfo(m_waveformFile);
        QString fileName = fileInfo.fileName();
        qDebug() << "Waveform " << fileName << " is tuned to a minimum power of " << m_finalStableMin << " dBm and a maximum power of " << m_finalStableMax << " dBm";
        QString logMsg = QString("Waveform %1 is tuned to a minimum power of %2 dBm and a maximum power of %3 dBm")
                             .arg(fileName)
                             .arg(m_finalStableMin, 0, 'f', 1)
                             .arg(m_finalStableMax, 0, 'f', 1);
        if (m_logger)
            m_logger->debugAndLog(logMsg);
        m_pythonRunner->stopScript();
        emit tuningFinished();
        break;
    }
    case RetryAfterFault:
        qDebug() << "Fault encountered. Retrying after fault...";
        m_pythonRunner->stopScript();
        m_currentGain--;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to adjust gain after fault.");
            return;
        }
        m_pythonRunner->startScript();
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetModeALC); });
        break;
    default:
        qDebug() << "Unknown state encountered.";
        break;
    }
}

void WaveformTuner::onAmpOutput(const QString &device, const QString &output)
{
    if (m_state == CheckAmpMode) {
        if (output.contains("STANDBY, VVA")) {
            qDebug() << "Amp" << device << "is in STANDBY, VVA mode. Moving on.";
            transitionToState(InitialModeVVA);
            return;
        }
        if (output.contains("STANDBY, ALC")) {
            m_ampSerial->sendCommand("MODE VVA", device);
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(CheckAmpMode); });
            return;
        }
        if (output.contains("ONLINE, VVA")) {
            m_ampSerial->sendCommand("STANDBY", device);
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(CheckAmpMode); });
            return;
        }
        if (output.contains("ONLINE, ALC")) {
            m_ampSerial->sendCommand("STANDBY", device);
            m_delayTimer->singleShot(500, this, [this, device](){
                m_ampSerial->sendCommand("MODE VVA", device);
                transitionToState(CheckAmpMode);
            });
            return;
        }
    }
    if (m_state == QueryFwdPwrALC || m_state == WaitForAlcStable) {
        if (output.contains("ALC Range")) {
            m_alcRangeCount++;
        } else {
            m_alcRangeCount = 0;
        }
    }
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
        if (m_ampReadings[device].size() > 10)
            m_ampReadings[device].removeFirst();
    } else {
        qDebug() << "Failed to parse the forward power reading from amp output:" << output;
    }
}

void WaveformTuner::onAmpFault(const QString &device, const QString &error)
{
    Q_UNUSED(device);
    qWarning() << "Fault detected:" << error;
    m_delayTimer->singleShot(1000, this, [this](){ transitionToState(RetryAfterFault); });
}

void WaveformTuner::onPythonOutput(const QString &output)
{
    if ((m_state == WaitForPythonPrompt || m_state == WaitForPythonPrompt_ALC) &&
        output.contains("Press Enter to quit"))
    {
        if (m_state == WaitForPythonPrompt_ALC)
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(QueryFwdPwrALC); });
        else
            m_delayTimer->singleShot(500, this, [this]() { transitionToState(SetModeVVA_All); });
    }
}
