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
#include <QFile>
#include <QTextStream>

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

int WaveformTuner::extractChannelFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return 0; // default to channel 0 if file cannot be opened
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Regex to match .set_gain(<number>, <channel>)
    QRegularExpression re("\\.set_gain\\s*\\(\\s*[-+]?\\d+\\s*,\\s*([01])\\s*\\)");
    QRegularExpressionMatch match = re.match(content);
    if (match.hasMatch()) {
        bool ok = false;
        int channel = match.captured(1).toInt(&ok);
        if (ok)
            return channel;
    }
    return 0;
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

    // Determine initial gain based on amplifier model.
    if (ampModel.compare("x300", Qt::CaseInsensitive) == 0)
        m_initialGain = 0;
    else if (ampModel.compare("N321", Qt::CaseInsensitive) == 0)
        m_initialGain = 12;
    else
        m_initialGain = 0;
    m_currentGain = m_initialGain;

    // Determine the channel.
    QString fileName = QFileInfo(m_waveformFile).fileName();
    if (fileName.startsWith("L1_L2_")) {
        m_isL1L2 = true;
        m_channel = 0;  // For L1_L2, start with channel 0 (later switch to channel 1)
    }
    else if (fileName.startsWith("L2_")) {
        m_isL1L2 = false;
        m_channel = extractChannelFromFile(m_waveformFile);
    }
    else {
        m_isL1L2 = false;
        m_channel = 0;
    }

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
    m_delayTimer->singleShot(1000, this, [this]() { transitionToState(CheckAmpMode); });
}

void WaveformTuner::resetRollingAverages()
{
    m_ampReadings.clear();
    m_testingAmpDevices.clear();
}

QStringList WaveformTuner::targetDevices() const {
    // If only one amp was found, always return that amp.
    if (m_allAmpDevices.size() == 1)
        return m_allAmpDevices;

    // If two or more are available, choose one based on m_channel.
    if (m_allAmpDevices.size() >= 2) {
        QStringList result;
        if (m_channel == 0) {
            // Look for a device name that clearly indicates "L1" (but not "L1L2" or "L2")
            for (const QString &dev : m_allAmpDevices) {
                if (dev.contains("L1", Qt::CaseInsensitive) &&
                    !dev.contains("L1L2", Qt::CaseInsensitive) &&
                    !dev.contains("L2", Qt::CaseInsensitive)) {
                    result << dev;
                    break;
                }
            }
            if (result.isEmpty())
                result << m_allAmpDevices.first();
        }
        else if (m_channel == 1) {
            // Look for a device name that clearly indicates "L2" (and not "L1L2")
            for (const QString &dev : m_allAmpDevices) {
                if (dev.contains("L2", Qt::CaseInsensitive) &&
                    !dev.contains("L1L2", Qt::CaseInsensitive)) {
                    result << dev;
                    break;
                }
            }
            if (result.isEmpty()) {
                // Fallback: if at least two exist, pick the second one.
                if (m_allAmpDevices.size() >= 2)
                    result << m_allAmpDevices.at(1);
                else
                    result << m_allAmpDevices.first();
            }
        }
        return result;
    }
    return m_allAmpDevices;
}

void WaveformTuner::transitionToState(TuningState newState)
{
    m_state = newState;
    switch(m_state) {
    case CheckAmpMode: {
        qDebug() << "Checking amplifier status...";
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->sendCommand("MODE?", dev);
    }
    break;
    case InitialModeVVA: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setMode("VVA", dev);
        m_delayTimer->singleShot(1200, this, [this](){ transitionToState(InitialVvaLevel); });
    }
    break;
    case InitialVvaLevel: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setGainLvl(100, dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(InitialModeALC); });
    }
    break;
    case InitialModeALC: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setMode("ALC", dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(InitialAlcLevel); });
    }
    break;
    case InitialAlcLevel: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setAlcLvl(m_minPower, dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetOnline); });
    }
    break;
    case SetOnline: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->sendCommand("ONLINE", dev);
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(SetInitialGain); });
    }
    break;
    case SetInitialGain:
        qDebug() << "Step 1: Setting initial gain to" << m_currentGain << "dBm.";
        if (m_isL1L2) {
            if (m_channel == 0) {
                // For channel 0 on an L1_L2 file, update both gain lines.
                if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, 0)) {
                    emit tuningFailed("Failed to set initial gain for channel 0.");
                    return;
                }
                if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, 1)) {
                    emit tuningFailed("Failed to set initial gain for channel 1.");
                    return;
                }
            } else {
                // For channel 1 tuning, update only the channel 1 line.
                if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, 1)) {
                    emit tuningFailed("Failed to set initial gain for channel 1.");
                    return;
                }
            }
        } else {
            if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
                emit tuningFailed("Failed to set initial gain.");
                return;
            }
        }
        m_delayTimer->singleShot(500, this, [this]() { transitionToState(StartWaveform); });
        break;
    case StartWaveform:
        qDebug() << "Step 2: Starting waveform.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt);
        break;
    case WaitForPythonPrompt:
        qDebug() << "Waiting for waveform to start...";
        break;
    case SetModeVVA_All: {
        qDebug() << "Step 3: Setting mode VVA (Gain) on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setMode("VVA", dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetGain100_All); });
    }
    break;
    case SetGain100_All: {
        qDebug() << "Setting gain level to 100 on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setGainLvl(100, dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(QueryFwdPwr); });
    }
    break;
    case QueryFwdPwr: {
        qDebug() << "Step 4: Querying forward power on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            m_ampReadings[dev].clear();
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(WaitForStable); });
    }
    break;
    case WaitForStable: {
        bool stableFound = false;
        qDebug() << "Checking for stable forward power on target amp...";
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings >= 3) {
                QList<double> lastThree = m_ampReadings[dev].mid(numReadings - 3, 3);
                if (qAbs(lastThree[0] - lastThree[1]) < 0.1 &&
                    qAbs(lastThree[1] - lastThree[2]) < 0.1) {
                    if (!m_testingAmpDevices.contains(dev))
                        m_testingAmpDevices.append(dev);
                    stableFound = true;
                }
            }
        }
        if (stableFound)
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(StopWaveform); });
        else {
            QStringList targets = targetDevices();
            for (const QString &dev : targets)
                m_ampSerial->getFwdPwr(dev);
            m_delayTimer->singleShot(500, this, [this](){ transitionToState(WaitForStable); });
        }
    }
    break;
    case StopWaveform:
        qDebug() << "Step 5: Stopping waveform.";
        m_pythonRunner->stopScript();
        m_delayTimer->singleShot(500, this, [this](){ transitionToState(ComparePower); });
        break;
    case ComparePower: {
        qDebug() << "Step 6: Comparing results to target" << m_maxPower << "dBm on target amp.";
        double total = 0;
        int count = 0;
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
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
            if (diff > 2.2)
                m_gainStep = 5;
            else if (diff > 1.8)
                m_gainStep = 4;
            else if (diff > 1.2)
                m_gainStep = 3;
            else if (diff > 0.6)
                m_gainStep = 2;
            else
                m_gainStep = 1;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(AdjustGainUp); });
        }
        else if (diff < -0.3) {
            m_gainStep = 1;
            // Store the computed average for later use in AdjustGainDown.
            m_lastAvg = avg;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(AdjustGainDown); });
        }
        else {
            m_finalStableMax = avg;
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetModeALC); });
        }
    }
    break;
    case AdjustGainUp: {
        qDebug() << "Step 7: Increasing gain. New gain:" << (m_currentGain + m_gainStep);
        m_lastGainAdjustment = 1;
        m_currentGain += m_gainStep;
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampReadings[dev].clear();
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to increment gain.");
            return;
        }
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(StartWaveform); });
    }
    break;
    case AdjustGainDown: {
        qDebug() << "Step 7: Lowering gain. New gain:" << (m_currentGain - m_gainStep);
        m_lastGainAdjustment = -1;
        // Increment the down-adjust counter.
        m_adjustDownCount++;
        // If we've reached 3 downward adjustments, accept the higher gain value.
        if (m_adjustDownCount >= 3) {
            qDebug() << "AdjustGainDown reached 3 times; accepting stable max:" << m_lastAvg;
            m_finalStableMax = m_lastAvg;
            m_adjustDownCount = 0; // reset counter for future use.
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(SetModeALC); });
        } else {
            m_currentGain -= m_gainStep;
            QStringList targets = targetDevices();
            for (const QString &dev : targets)
                m_ampReadings[dev].clear();
            if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
                emit tuningFailed("Failed to decrement gain.");
                return;
            }
            m_delayTimer->singleShot(1000, this, [this](){ transitionToState(StartWaveform); });
        }
    }
    break;
    case SetModeALC: {
        qDebug() << "Step 8: Setting up ALC test for minimum power on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            m_ampReadings[dev].clear();
            m_ampSerial->setMode("ALC", dev);
        }
        m_delayTimer->singleShot(1500, this, [this](){ transitionToState(PreSetAlc); });
    }
    break;
    case PreSetAlc: {
        qDebug() << "Setting ALC level to" << m_minPower << "dBm on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->setAlcLvl(m_minPower, dev);
        m_delayTimer->singleShot(1500, this, [this](){ transitionToState(StartWaveform_ALC); });
    }
    break;
    case StartWaveform_ALC:
        qDebug() << "Step 9: Starting waveform in ALC mode.";
        m_pythonRunner->startScript();
        transitionToState(WaitForPythonPrompt_ALC);
        break;
    case WaitForPythonPrompt_ALC:
        qDebug() << "Waiting for waveform to start in ALC mode...";
        break;
    case QueryFwdPwrALC: {
        qDebug() << "Step 10: Querying forward power in ALC mode on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets)
            m_ampSerial->getFwdPwr(dev);
        m_delayTimer->singleShot(1000, this, [this](){ transitionToState(WaitForAlcStable); });
    }
    break;
    case WaitForAlcStable: {
        const double tolerance = 0.2;
        bool allReady = true;
        double total = 0;
        int count = 0;
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
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
    }
    break;
    case AdjustMinDown:
        qDebug() << "Adjusting minimum: lowering gain. New gain:" << (m_currentGain - 1);
        if (m_currentGain <= 0) {
            qDebug() << "Gain is already 0. Cannot lower further.";
            if (m_logger)
                m_logger->debugAndLog("Tuning failed: gain cannot be lowered further for LOW critical tuning.");
            emit tuningFailed("Gain cannot be lowered further for LOW critical tuning.");
            return;
        }
        m_currentGain--;
        if (!m_pythonEditor->editGainValue(m_waveformFile, m_currentGain, m_channel)) {
            emit tuningFailed("Failed to lower gain for LOW critical.");
            return;
        }
        {
            QStringList targets = targetDevices();
            for (const QString &dev : targets)
                m_ampReadings[dev].clear();
        }
        m_pythonRunner->stopScript();
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(StartWaveform_ALC); });
        break;
    case FinalizeTuning: {
        qDebug() << "Step 11: Finalizing tuning on target amp.";
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            m_ampSerial->setMode("VVA", dev);
            m_delayTimer->singleShot(500, this, [this, dev]() {
                m_ampSerial->setGainLvl(100, dev);
                m_ampReadings[dev].clear();
            });
        }
        m_delayTimer->singleShot(2500, this, [this]() { transitionToState(RecheckMax); });
    }
    break;
    case RecheckMax: {
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            m_ampReadings[dev].clear();
            m_ampSerial->getFwdPwr(dev);
        }
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(WaitForMaxStable); });
    }
    break;
    case WaitForMaxStable: {
        const double tolerance = 0.01;
        bool allReady = true;
        double total = 0;
        int count = 0;
        QStringList targets = targetDevices();
        for (const QString &dev : targets) {
            int numReadings = m_ampReadings[dev].size();
            if (numReadings < 3)
                allReady = false;
            else {
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
            QStringList targets = targetDevices();
            for (const QString &dev : targets)
                m_ampSerial->getFwdPwr(dev);
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(WaitForMaxStable); });
            break;
        }
        double avgMax = total / count;
        m_finalStableMax = avgMax;
        m_delayTimer->singleShot(1000, this, [this]() { transitionToState(LogResults); });
    }
    break;
    case LogResults: {
        QFileInfo fileInfo(m_waveformFile);
        QString fileName = fileInfo.fileName();
        QString channelString = (m_channel == 0 ? "L1" : "L2");
        qDebug() << "Waveform" << fileName << "for channel" << channelString
                 << "is tuned to a minimum power of" << m_finalStableMin
                 << "dBm and a maximum power of" << m_finalStableMax << "dBm";
        QString logMsg = QString("Waveform %1 for channel %2 is tuned to a minimum power of %3 dBm and a maximum power of %4 dBm")
                             .arg(fileName)
                             .arg(channelString)
                             .arg(m_finalStableMin, 0, 'f', 1)
                             .arg(m_finalStableMax, 0, 'f', 1);
        if (m_logger)
            m_logger->debugAndLog(logMsg);
        if (m_isL1L2 && m_channel == 0) {
            // Finished tuning channel 0 for an L1_L2 file. Now switch to channel 1.
            m_channel = 1;
            m_currentGain = m_initialGain; // Reset channel 1's gain to the initial value.
            resetRollingAverages();
            m_pythonRunner->stopScript();
            QTimer::singleShot(1000, this, [this]() { transitionToState(SetInitialGain); });
        } else {
            m_pythonRunner->stopScript();
            emit tuningFinished();
        }
    }
    break;
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
            qDebug() << "Amp" << device << "is ready.";
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
            m_delayTimer->singleShot(500, this, [this](){
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
            m_delayTimer->singleShot(1000, this, [this]() { transitionToState(SetModeVVA_All); });
    }
}
