#include <QCoreApplication>
#include <QTextStream>
#include <QDir>
#include <QStringList>
#include <functional>
#include <memory>
#include "waveformtuner.h"

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
    if (categoryChoice == "1") {
        selectedFiles = l1Files;
    } else if (categoryChoice == "2") {
        selectedFiles = l2Files;
    } else if (categoryChoice == "3") {
        selectedFiles = l1l2Files;
    } else {
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
    cout << "Enter the minimum power: " << Qt::flush;
    QString minPowerStr = cin.readLine().trimmed();
    bool ok = false;
    double minPower = minPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid minimum power. Exiting.\n";
        return -1;
    }

    // Prompt for maximum power.
    cout << "Enter the maximum power: " << Qt::flush;
    QString maxPowerStr = cin.readLine().trimmed();
    double maxPower = maxPowerStr.toDouble(&ok);
    if (!ok) {
        cout << "Invalid maximum power. Exiting.\n";
        return -1;
    }

    // Prompt for critical level.
    cout << "Which is critical - HIGH or LOW? " << Qt::flush;
    QString critical = cin.readLine().trimmed();
    if (critical.compare("HIGH", Qt::CaseInsensitive) != 0 &&
        critical.compare("LOW", Qt::CaseInsensitive) != 0) {
        cout << "Invalid critical value. Exiting.\n";
        return -1;
    }

    int totalFiles = selectedFiles.size();
    // Use a shared_ptr for currentIndex so it stays alive for the lambda.
    auto currentIndex = std::make_shared<int>(0);
    // Capture pointers to app and cout so they’re copied into the lambda.
    auto pApp = &app;
    auto pcout = &cout;

    // Define a lambda to process files sequentially.
    std::function<void()> processNext = [=]() mutable {
        if (*currentIndex >= totalFiles) {
            (*pcout) << "All files processed. Exiting.\n";
            pApp->quit();
            return;
        }
        QString file = selectedFiles.at(*currentIndex);
        (*pcout) << "Processing file (" << (*currentIndex + 1) << "/" << totalFiles << "): " << file << "\n";
        // Create a new WaveformTuner for this file.
        WaveformTuner *tuner = new WaveformTuner(pApp);
        QObject::connect(tuner, &WaveformTuner::tuningFinished, pApp,
                         [=]() mutable {
                             (*pcout) << "Tuning complete for file: " << file << "\n";
                             tuner->deleteLater();
                             (*currentIndex)++;
                             processNext();
                         });
        QObject::connect(tuner, &WaveformTuner::tuningFailed, pApp,
                         [=](const QString &reason) mutable {
                             (*pcout) << "Tuning failed for file: " << file << " Reason: " << reason << "\n";
                             tuner->deleteLater();
                             (*currentIndex)++;
                             processNext();
                         });
        tuner->startTuning(file, ampModel, minPower, maxPower, critical);
    };

    processNext();
    return app.exec();
}
