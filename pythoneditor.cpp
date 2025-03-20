#include "pythoneditor.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <limits.h>
#include <QCoreApplication>
#include <QSettings>

PythonEditor::PythonEditor(QObject *parent)
    : QObject(parent)
{
}

bool PythonEditor::editGainValue(const QString &filePath, int newGain, int targetChannel)
{
    // Read the allowed gain range from waveTuneConfig.ini.
    // The .ini file is assumed to be in the same directory as the application.
    QString configFile = QCoreApplication::applicationDirPath() + "/waveTuneConfig.ini";
    QSettings settings(configFile, QSettings::IniFormat);
    int minGain = settings.value("Gain/Min", -10).toInt();
    int maxGain = settings.value("Gain/Max", 60).toInt();

    // Validate channel.
    if (targetChannel != 0 && targetChannel != 1) {
        qWarning() << "Invalid channel specified:" << targetChannel;
        return false;
    }
    // Validate gain using values from config file.
    if ((newGain < minGain) || (newGain > maxGain)) {
        qWarning() << "Gain value out of allowed range:" << newGain
                   << "(Allowed range:" << minGain << "to" << maxGain << ")";
        return false;
    }

    // Read all lines from file.
    QFile fileObj(filePath);
    if (!fileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return false;
    }
    QStringList lines;
    QTextStream in(&fileObj);
    while (!in.atEnd())
        lines.append(in.readLine());
    fileObj.close();

    // Regular expression to capture:
    //   Group 1: SDR instance name (between "self." and ".set_gain")
    //   Group 2: the gain value (first argument)
    //   Group 3: the channel parameter (second argument)
    QRegularExpression re("self\\.([A-Za-z0-9_]+)\\.set_gain\\(\\s*([-+]?\\d+)\\s*,\\s*(\\d+)\\s*\\)");

    // Structure to store candidate information.
    struct Candidate {
        int lineIndex;
        QString instanceName;
        int channelParam; // second argument in set_gain()
        int tokenCount;   // number of underscore-separated tokens in instanceName
        int lastToken;    // last token from instanceName (if convertible to int), else -1
    };

    QList<Candidate> candidates;
    // Scan each line for candidates.
    for (int i = 0; i < lines.size(); ++i) {
        QRegularExpressionMatch match = re.match(lines[i]);
        if (match.hasMatch()) {
            Candidate c;
            c.lineIndex = i;
            c.instanceName = match.captured(1);
            c.channelParam = match.captured(3).toInt();
            QStringList tokens = c.instanceName.split("_");
            c.tokenCount = tokens.size();
            // Try to interpret the last token as an int.
            bool ok = false;
            c.lastToken = tokens.last().toInt(&ok);
            if (!ok)
                c.lastToken = -1;
            candidates.append(c);
        }
    }
    if (candidates.isEmpty()) {
        qWarning() << "No .set_gain lines found in" << filePath;
        return false;
    }

    Candidate chosen;
    bool candidateChosen = false;

    // Step 1: Check if the channel parameters differ among candidates.
    bool diffChannels = false;
    for (const Candidate &c : candidates) {
        if (c.channelParam != candidates.first().channelParam) {
            diffChannels = true;
            break;
        }
    }
    if (diffChannels) {
        // Simply pick the candidate with channelParam equal to targetChannel.
        for (const Candidate &c : candidates) {
            if (c.channelParam == targetChannel) {
                chosen = c;
                candidateChosen = true;
                break;
            }
        }
    }
    else {
        // All candidates have the same channel parameter.
        // Step 2: Compare token counts.
        int minToken = INT_MAX, maxToken = -1;
        for (const Candidate &c : candidates) {
            if (c.tokenCount < minToken)
                minToken = c.tokenCount;
            if (c.tokenCount > maxToken)
                maxToken = c.tokenCount;
        }
        if (minToken != maxToken) {
            // If targetChannel is 0 (L1), choose candidate with minimum token count.
            // If targetChannel is 1 (L2), choose candidate with maximum token count.
            for (const Candidate &c : candidates) {
                if (targetChannel == 0 && c.tokenCount == minToken) {
                    chosen = c;
                    candidateChosen = true;
                    break;
                } else if (targetChannel == 1 && c.tokenCount == maxToken) {
                    chosen = c;
                    candidateChosen = true;
                    break;
                }
            }
        }
        else {
            // Token counts are the same; Step 3: Check last token of the instance name.
            bool foundByLastToken = false;
            for (const Candidate &c : candidates) {
                if (c.lastToken == targetChannel) {
                    chosen = c;
                    foundByLastToken = true;
                    candidateChosen = true;
                    break;
                }
            }
            if (!foundByLastToken) {
                // Step 4: Fall back to order.
                if (candidates.size() >= 2) {
                    chosen = (targetChannel == 0) ? candidates.first() : candidates.at(1);
                    candidateChosen = true;
                } else {
                    chosen = candidates.first();
                    candidateChosen = true;
                }
            }
        }
    }

    if (!candidateChosen) {
        qWarning() << "Failed to determine which .set_gain line to update.";
        return false;
    }

    // Update the chosen candidate's line by replacing the gain value.
    int lineToUpdate = chosen.lineIndex;
    // Regex to capture the portion before the gain value and after, so that we only replace the gain.
    QRegularExpression gainRe("(.set_gain\\(\\s*)[-+]?\\d+(\\s*,\\s*\\d+\\s*\\))");
    QString newLine = lines[lineToUpdate];
    newLine.replace(gainRe, QString("\\1%1\\2").arg(newGain));
    lines[lineToUpdate] = newLine;

    // Write the updated content back to the file.
    if (!fileObj.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    QTextStream out(&fileObj);
    for (const QString &line : lines)
        out << line << "\n";
    fileObj.close();

    return true;
}
