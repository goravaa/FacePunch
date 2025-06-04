//inlclude/config.h
#pragma once
#include <string>

struct AppConfig {
    int maxDetections = 25;
    float confThresh = 0.5f;
    float iouThresh = 0.3f;
    std::string modelPath = "assets/models/blaze.onnx";

    // Loads config from environment, with defaults and validation
    void loadFromEnv();
};
