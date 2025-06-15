//inlclude/config.h
#pragma once
#include <string>

struct AppConfig {
    int maxDetections = 25;
    float confThresh = 0.5f;
    float iouThresh = 0.3f;
    std::string modelPath = "assets/models/blaze.onnx";
    std::string arcfaceModelPath = "assets/models/arc.onnx";
    std::string faceDatabasePath = "face_db.csv";
    float similarityThreshold = 0.85f;
    int maxFaceIndexSize = 10000;
    std::string attendanceLogPath = "attendance_log.csv";

    // Loads config from QSettings, then environment, with defaults and validation
    void loadInitialConfig();
};
