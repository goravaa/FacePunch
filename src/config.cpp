//src/config.cpp
#include "config.h"
#include <cstdlib>
#include <stdexcept>
#include <QSettings>
#include <QString> // For QSettings string conversions

// Helper to read string from QSettings or fallback
static std::string getStringSetting(QSettings& settings, const QString& key, const std::string& fallback) {
    if (settings.contains(key)) {
        return settings.value(key).toString().toStdString();
    }
    return fallback;
}

// Helper to read int from QSettings or fallback
static int getIntSetting(QSettings& settings, const QString& key, int fallback) {
    if (settings.contains(key)) {
        bool ok;
        int val = settings.value(key).toInt(&ok);
        if (ok) return val;
    }
    return fallback;
}

// Helper to read float from QSettings or fallback
static float getFloatSetting(QSettings& settings, const QString& key, float fallback) {
    if (settings.contains(key)) {
        bool ok;
        float val = settings.value(key).toFloat(&ok);
        if (ok) return val;
    }
    return fallback;
}


static float getFloatEnv(const char* name, float fallback) {
    const char* val = std::getenv(name);
    if (val) {
        try { return std::stof(val); } catch (...) {}
    }
    return fallback;
}

static int getIntEnv(const char* name, int fallback) {
    const char* val = std::getenv(name);
    if (val) {
        try { return std::stoi(val); } catch (...) {}
    }
    return fallback;
}

void AppConfig::loadInitialConfig() {
    // Default values are already set by member initializers in AppConfig struct

    // 1. Load from QSettings
    QSettings settings("MyCompany", "FacePunchApp");

    maxDetections = getIntSetting(settings, "maxDetections", maxDetections);
    confThresh = getFloatSetting(settings, "confThresh", confThresh);
    iouThresh = getFloatSetting(settings, "iouThresh", iouThresh);
    modelPath = getStringSetting(settings, "modelPath", modelPath);
    arcfaceModelPath = getStringSetting(settings, "arcfaceModelPath", arcfaceModelPath);
    faceDatabasePath = getStringSetting(settings, "faceDatabasePath", faceDatabasePath);
    similarityThreshold = getFloatSetting(settings, "similarityThreshold", similarityThreshold);
    maxFaceIndexSize = getIntSetting(settings, "maxFaceIndexSize", maxFaceIndexSize);
    attendanceLogPath = getStringSetting(settings, "attendanceLogPath", attendanceLogPath);

    // 2. Override with Environment Variables if set
    const char* env_val_str; // For string types

    env_val_str = std::getenv("MAX_DETECTIONS");
    if (env_val_str) maxDetections = getIntEnv("MAX_DETECTIONS", maxDetections);

    env_val_str = std::getenv("CONF_THRESH");
    if (env_val_str) confThresh = getFloatEnv("CONF_THRESH", confThresh);

    env_val_str = std::getenv("IOU_THRESH");
    if (env_val_str) iouThresh = getFloatEnv("IOU_THRESH", iouThresh);

    env_val_str = std::getenv("MODEL_PATH");
    if (env_val_str && env_val_str[0]) modelPath = env_val_str;

    env_val_str = std::getenv("ARCFACE_MODEL_PATH");
    if (env_val_str && env_val_str[0]) arcfaceModelPath = env_val_str;

    env_val_str = std::getenv("FACE_DATABASE_PATH");
    if (env_val_str && env_val_str[0]) faceDatabasePath = env_val_str;

    env_val_str = std::getenv("SIMILARITY_THRESHOLD");
    if (env_val_str) similarityThreshold = getFloatEnv("SIMILARITY_THRESHOLD", similarityThreshold);

    env_val_str = std::getenv("MAX_FACE_INDEX_SIZE");
    if (env_val_str) maxFaceIndexSize = getIntEnv("MAX_FACE_INDEX_SIZE", maxFaceIndexSize);

    env_val_str = std::getenv("ATTENDANCE_LOG_PATH");
    if (env_val_str && env_val_str[0]) attendanceLogPath = env_val_str;

    // 3. Validate (and apply hardcoded defaults if validation fails)
    // This validation logic is similar to what was at the end of the old loadFromEnv
    if (maxDetections <= 0 || maxDetections > 1000) maxDetections = 25; // Default from original struct
    if (confThresh <= 0.0f || confThresh > 1.0f) confThresh = 0.5f; // Default from original struct
    if (iouThresh < 0.0f || iouThresh > 1.0f) iouThresh = 0.3f; // Default from original struct
    // No specific validation for paths here, assuming they are correct or empty
    if (similarityThreshold < 0.0f || similarityThreshold > 1.0f) similarityThreshold = 0.85f; // Default
    if (maxFaceIndexSize < 100 || maxFaceIndexSize > 1000000) maxFaceIndexSize = 10000; // Default
}
