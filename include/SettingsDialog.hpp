#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include "config.h" // For AppConfig

// Forward declaration for Ui namespace (not strictly needed as we are not using a .ui file)
// namespace Ui { class SettingsDialog; }

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(AppConfig& config, QWidget *parent = nullptr);
    ~SettingsDialog(); // Add destructor prototype

private slots:
    void accept() override; // To save settings

private:
    // Ui::SettingsDialog *ui; // Not using .ui file for this subtask
    AppConfig& currentConfig; // Reference to the main AppConfig

    // Pointers to widgets
    QSpinBox* maxDetectionsSpinBox;
    QDoubleSpinBox* confThreshDoubleSpinBox;
    QDoubleSpinBox* iouThreshDoubleSpinBox;
    QDoubleSpinBox* similarityThresholdDoubleSpinBox;
    QSpinBox* maxFaceIndexSizeSpinBox;

    QDialogButtonBox* buttonBox;

    void loadSettings(); // Load AppConfig into dialog widgets
    void saveSettings(); // Save dialog widget values to AppConfig & QSettings
};
