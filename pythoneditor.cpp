#include "pythoneditor.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

PythonEditor::PythonEditor(QObject *parent)
    : QObject(parent)
{
}

bool PythonEditor::editGainValue(const QString &filePath, int newGain, int channel)
{
    // Ensure channel is either 0 or 1.
    if (channel != 0 && channel != 1) {
        qWarning() << "Invalid channel specified:" << channel;
        return false;
    }

    // Check that newGain is within allowed range.
    if ((newGain < -10) || (newGain > 60)) {
        qWarning() << "Gain value out of allowed range:" << newGain;
        return false;
    }

    // Open file for reading.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return false;
    }

    // Read all lines.
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // Regex to find lines with .set_gain(<int>, <int>).
    // Capture group 1: an integer that should be between -10 and 60
    // Capture group 2: 0 or 1 only
    static const QRegularExpression re("\\.set_gain\\(\\s*([-+]?\\d+)\\s*,\\s*([01])\\s*\\)");

    bool updated = false;
    // Iterate over each line; update the first occurrence that matches the specified channel.
    for (int i = 0; i < lines.size(); i++) {
        QRegularExpressionMatch match = re.match(lines[i]);
        if (match.hasMatch() && match.captured(2).toInt() == channel) {
            // Build replacement using newGain (as int) and preserving the channel.
            QString replacement = QString(".set_gain(%1, %2)")
                .arg(newGain)
                .arg(match.captured(2));
            lines[i].replace(match.capturedStart(0), match.capturedLength(0), replacement);
            updated = true;
            break;
        }
    }

    if (!updated) {
        qWarning() << "No matching .set_gain function found for channel" << channel << "in" << filePath;
        return false;
    }

    // Open file for writing (truncate mode) and update the file.
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    QTextStream out(&file);
    for (const QString &line : qAsConst(lines)) {
        out << line << "\n";
    }
    file.close();

    return true;
}
