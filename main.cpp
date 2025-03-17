#include <QCoreApplication>
#include <QTextStream>
#include <QDir>
#include <QStringList>
#include <QTimer>
#include <functional>
#include "waveformtuner.h"

// Helper function to process files sequentially.
void processNextFile(const QStringList &selectedFiles,
                     int index,
                     QCoreApplication *app,
                     QTextStream *out,
                     const QString &ampModel,
                     double minPower,
                     double maxPower,
                     const QString &critical)
{
    if (index >= selectedFiles.size()) {
        *out << "All files processed. Exiting.\n";
        app->quit();
        return;
    }

    QString file = selectedFiles.at(index);
    *out << "Processing file (" << (index + 1) << "/" << selectedFiles.size() << "): " << file << "\n";

    // Create a new WaveformTuner for this file.
    WaveformTuner *tuner = new WaveformTuner(app);

    QObject::connect(tuner, &WaveformTuner::tuningFinished, app, [=]() {
        *out << "Tuning complete for file: " << file << "\n";
        tuner->deleteLater();
        QTimer::singleShot(3000, app, [=]() {
            processNextFile(selectedFiles, index + 1, app, out, ampModel, minPower, maxPower, critical);
        });
    });

    QObject::connect(tuner, &WaveformTuner::tuningFailed, app, [=](const QString &reason) {
        *out << "Tuning failed for file: " << file << " Reason: " << reason << "\n";
        tuner->deleteLater();
        QTimer::singleShot(3000, app, [=]() {
            processNextFile(selectedFiles, index + 1, app, out, ampModel, minPower, maxPower, critical);
        });
    });

    tuner->startTuning(file, ampModel, minPower, maxPower, critical);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream cin(stdin);
    QTextStream cout(stdout);

    // Prompt for the directory containing waveform files.
    cout << "Enter the directory containing waveform files: " << Qt::flush;
    QString directory = cin.readLine().trimmed();
    if (directory.isEmpty()) {
        cout << "Directory cannot be empty. Exiting.\n";
        return -1;
    }

    QDir dir(directory);
    if (!dir.exists()) {
        cout << "Directory does not exist. Exiting.\n";
        return -1;
    }

    // List all .py files in the directory.
    QStringList pyFiles = dir.entryList(QStringList() << "*.py", QDir::Files);
    if (pyFiles.isEmpty()) {
        cout << "No .py files found in the directory. Exiting.\n";
        return -1;
    }

    // Partition files into three lists.
    QStringList l1Files, l2Files, l1l2Files;
    for (const QString &file : pyFiles) {
        if (file.startsWith("L1_")) {
            if (file.startsWith("L1_L2_"))
                l1l2Files.append(dir.absoluteFilePath(file));
            else
                l1Files.append(dir.absoluteFilePath(file));
        } else if (file.startsWith("L2_")) {
            l2Files.append(dir.absoluteFilePath(file));
        }
    }

    // Show counts and prompt the user for a category.
    cout << "Files found:\n";
    cout << "L1 (excluding L1_L2): " << l1Files.size() << " files\n";
    cout << "L2: " << l2Files.size() << " files\n";
    cout << "L1_L2: " << l1l2Files.size() << " files\n";
    cout << "Enter 1 to tune L1, 2 to tune L2, or 3 to tune L1_L2: " << Qt::flush;
    QString categoryChoice = cin.readLine().trimmed();

    QStringList selectedFiles;
    if (categoryChoice == "1")
        selectedFiles = l1Files;
    else if (categoryChoice == "2")
        selectedFiles = l2Files;
    else if (categoryChoice == "3")
        selectedFiles = l1l2Files;
    else {
        cout << "Invalid selection. Exiting.\n";
        return -1;
    }

    // Prompt for amplifier model.
    cout << "Are you tuning for an x300 or N321? " << Qt::flush;
    QString ampModel = cin.readLine().trimmed();
    if (ampModel.compare("x300", Qt::CaseInsensitive) != 0 &&
        ampModel.compare("N321", Qt::CaseInsensitive) != 0) {
        cout << "Invalid amplifier model. Exiting.\n";
        return -1;
    }

    // Prompt for minimum power.
    cout << "Enter the target minimum power: " << Qt::flush;
    QString minPowerStr = cin.readLine().trimmed();
    bool ok = false;
    double minPower = minPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid minimum power. Exiting.\n";
        return -1;
    }

    // Prompt for maximum power.
    cout << "Enter the target maximum power: " << Qt::flush;
    QString maxPowerStr = cin.readLine().trimmed();
    double maxPower = maxPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid maximum power. Exiting.\n";
        return -1;
    }

    // Prompt for critical level.
    cout << "Which is most important - HIGH or LOW? " << Qt::flush;
    QString critical = cin.readLine().trimmed();
    if (critical.compare("HIGH", Qt::CaseInsensitive) != 0 &&
        critical.compare("LOW", Qt::CaseInsensitive) != 0) {
        cout << "Invalid critical value. Exiting.\n";
        return -1;
    }

    // Process each selected file sequentially.
    processNextFile(selectedFiles, 0, &app, &cout, ampModel, minPower, maxPower, critical);
    return app.exec();
}
