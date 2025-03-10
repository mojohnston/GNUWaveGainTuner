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
    // Check that newGain is within allowed range
    if ((newGain < -10.0) || (newGain > 60.0)) {
        qWarning() << "Gain value out of allowed range:" << newGain;
        return false;
    }

    // Open file for reading
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return false;
    }

    // Read all lines
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // Regex to find lines with '.set_gain(<float>, <digit>)'
    static const QRegularExpression re("\\.set_gain\\(\\s*([-+]?\\d*\\.?\\d+)\\s*,\\s*([01])\\s*\\)");

    // Update the value of the gain for the specified channel
    bool updated = false;
    for (int i = 0; i < lines.size(); i++) {
        QRegularExpressionMatch match = re.match(lines[i]);
        if (match.hasMatch() && match.captured(2).toInt() == channel) {
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

    // Open file for writing (truncate mode) and update the file
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
