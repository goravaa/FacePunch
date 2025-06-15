#include "SettingsDialog.hpp"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSettings>
#include <QPushButton> // Required for QDialogButtonBox standard buttons

SettingsDialog::SettingsDialog(AppConfig& config, QWidget *parent)
    : QDialog(parent), currentConfig(config) {
    setWindowTitle("Application Settings");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    maxDetectionsSpinBox = new QSpinBox(this);
    maxDetectionsSpinBox->setRange(1, 1000);
    formLayout->addRow(tr("Max Detections:"), maxDetectionsSpinBox);

    confThreshDoubleSpinBox = new QDoubleSpinBox(this);
    confThreshDoubleSpinBox->setRange(0.0, 1.0);
    confThreshDoubleSpinBox->setSingleStep(0.01);
    confThreshDoubleSpinBox->setDecimals(2);
    formLayout->addRow(tr("Detector Confidence Threshold:"), confThreshDoubleSpinBox);

    iouThreshDoubleSpinBox = new QDoubleSpinBox(this);
    iouThreshDoubleSpinBox->setRange(0.0, 1.0);
    iouThreshDoubleSpinBox->setSingleStep(0.01);
    iouThreshDoubleSpinBox->setDecimals(2);
    formLayout->addRow(tr("Detector IOU Threshold:"), iouThreshDoubleSpinBox);

    similarityThresholdDoubleSpinBox = new QDoubleSpinBox(this);
    similarityThresholdDoubleSpinBox->setRange(0.0, 1.0);
    similarityThresholdDoubleSpinBox->setSingleStep(0.01);
    similarityThresholdDoubleSpinBox->setDecimals(2);
    formLayout->addRow(tr("Similarity Threshold:"), similarityThresholdDoubleSpinBox);

    maxFaceIndexSizeSpinBox = new QSpinBox(this);
    maxFaceIndexSizeSpinBox->setRange(100, 1000000); // Consistent with config.cpp validation
    maxFaceIndexSizeSpinBox->setSingleStep(100);
    formLayout->addRow(tr("Max Face Index Size:"), maxFaceIndexSizeSpinBox);

    mainLayout->addLayout(formLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    mainLayout->addWidget(buttonBox);

    loadSettings();
}

SettingsDialog::~SettingsDialog() {
    // Widgets are children of this dialog, Qt will handle their deletion.
}

void SettingsDialog::loadSettings() {
    maxDetectionsSpinBox->setValue(currentConfig.maxDetections);
    confThreshDoubleSpinBox->setValue(currentConfig.confThresh);
    iouThreshDoubleSpinBox->setValue(currentConfig.iouThresh);
    similarityThresholdDoubleSpinBox->setValue(currentConfig.similarityThreshold);
    maxFaceIndexSizeSpinBox->setValue(currentConfig.maxFaceIndexSize);
    // Model paths are not typically edited in such a dialog, so they are skipped here.
}

void SettingsDialog::saveSettings() {
    // Update AppConfig object
    currentConfig.maxDetections = maxDetectionsSpinBox->value();
    currentConfig.confThresh = static_cast<float>(confThreshDoubleSpinBox->value());
    currentConfig.iouThresh = static_cast<float>(iouThreshDoubleSpinBox->value());
    currentConfig.similarityThreshold = static_cast<float>(similarityThresholdDoubleSpinBox->value());
    currentConfig.maxFaceIndexSize = maxFaceIndexSizeSpinBox->value();

    // Save to QSettings
    QSettings settings("MyCompany", "FacePunchApp"); // Company and App name
    settings.setValue("maxDetections", currentConfig.maxDetections);
    settings.setValue("confThresh", currentConfig.confThresh);
    settings.setValue("iouThresh", currentConfig.iouThresh);
    settings.setValue("modelPath", QString::fromStdString(currentConfig.modelPath));
    settings.setValue("arcfaceModelPath", QString::fromStdString(currentConfig.arcfaceModelPath));
    settings.setValue("faceDatabasePath", QString::fromStdString(currentConfig.faceDatabasePath));
    settings.setValue("similarityThreshold", currentConfig.similarityThreshold);
    settings.setValue("maxFaceIndexSize", currentConfig.maxFaceIndexSize);
    settings.setValue("attendanceLogPath", QString::fromStdString(currentConfig.attendanceLogPath));
}

void SettingsDialog::accept() {
    saveSettings();
    QDialog::accept();
}
