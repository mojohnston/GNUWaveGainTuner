#include <QCoreApplication>
#include <QTextStream>
#include "waveformtuner.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream cin(stdin);
    QTextStream cout(stdout);

    // Prompt for waveform file location.
    cout << "Enter the waveform file location to be tuned (e.g. /home/labuser/signals-mamba/L1_BBN20M.py): " << Qt::flush;
    QString waveformFile = cin.readLine().trimmed();
    if (waveformFile.isEmpty()) {
        cout << "Waveform file cannot be empty. Exiting." << "\n";
        return -1;
    }

    // Prompt for amplifier model.
    cout << "Are you tuning for an x300 or N321? " << Qt::flush;
    QString ampModel = cin.readLine().trimmed();
    if (ampModel.compare("x300", Qt::CaseInsensitive) != 0 &&
        ampModel.compare("N321", Qt::CaseInsensitive) != 0)
    {
        cout << "Invalid amplifier model. Exiting." << "\n";
        return -1;
    }

    // Prompt for minimum power.
    cout << "Enter the minimum power: " << Qt::flush;
    QString minPowerStr = cin.readLine().trimmed();
    bool ok = false;
    double minPower = minPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid minimum power. Exiting." << "\n";
        return -1;
    }

    // Prompt for maximum power.
    cout << "Enter the maximum power: " << Qt::flush;
    QString maxPowerStr = cin.readLine().trimmed();
    double maxPower = maxPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid maximum power. Exiting." << "\n";
        return -1;
    }

    // Prompt for which is critical.
    cout << "Which is critical - HIGH or LOW? " << Qt::flush;
    QString critical = cin.readLine().trimmed();
    if (critical.compare("HIGH", Qt::CaseInsensitive) != 0 &&
        critical.compare("LOW", Qt::CaseInsensitive) != 0)
    {
        cout << "Invalid critical value. Exiting." << "\n";
        return -1;
    }

    // Create the waveform tuner object.
    WaveformTuner tuner;

    // Connect tuning finished signal.
    QObject::connect(&tuner, &WaveformTuner::tuningFinished, &tuner, [&](){
        cout << "Tuning complete!!!" << "\n";
        app.quit();
    });

    // Connect tuning failed signal.
    QObject::connect(&tuner, &WaveformTuner::tuningFailed, &tuner, [&](const QString &reason){
        cout << "Tuning failed: " << reason << "\n";
        app.quit();
    });

    // Start tuning with the user-provided parameters.
    tuner.startTuning(waveformFile, ampModel, minPower, maxPower, critical);

    return app.exec();
}
