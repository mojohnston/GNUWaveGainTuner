#include <QCoreApplication>
#include <QTextStream>
#include "pythoneditor.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream cin(stdin);
    QTextStream cout(stdout);

    // Prompt for file path.
    cout << "Enter the full path to the Python file you wish to edit: " << flush;
    QString filePath = cin.readLine().trimmed();
    if (filePath.isEmpty()) {
        cout << "Filename cannot be empty. Exiting." << "\n";
        return -1;
    }

    // Prompt for channel.
    cout << "Enter the channel to edit (L1 or L2): " << flush;
    QString channelInput = cin.readLine().trimmed();
    int channel = -1;
    if (channelInput.compare("L1", Qt::CaseInsensitive) == 0)
        channel = 0;
    else if (channelInput.compare("L2", Qt::CaseInsensitive) == 0)
        channel = 1;
    else {
        cout << "Invalid channel input. Exiting." << "\n";
        return -1;
    }

    // Prompt for gain value.
    cout << "Enter the gain value (integer between -10 and 60): " << flush;
    QString gainInput = cin.readLine().trimmed();
    bool ok = false;
    int gainValue = gainInput.toInt(&ok);
    if ((!ok) || (gainValue < -10) || (gainValue > 60)) {
        cout << "Invalid gain value. Exiting." << "\n";
        return -1;
    }

    // Call the editGainValue function.
    PythonEditor editor;
    if (editor.editGainValue(filePath, static_cast<double>(gainValue), channel)) {
        cout << "Successfully updated gain value in file: " << filePath << "\n";
    } else {
        cout << "Failed to update gain value in file: " << filePath << "\n";
    }

    return 0;
}
