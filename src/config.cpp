#include "config.h"
#include <cstdlib>
#include <stdexcept>

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

void AppConfig::loadFromEnv() {
    maxDetections = getIntEnv("MAX_DETECTIONS", maxDetections);
    if (maxDetections <= 0 || maxDetections > 1000) maxDetections = 25;

    confThresh = getFloatEnv("CONF_THRESH", confThresh);
    if (confThresh <= 0.0f || confThresh > 1.0f) confThresh = 0.5f;

    iouThresh = getFloatEnv("IOU_THRESH", iouThresh);
    if (iouThresh < 0.0f || iouThresh > 1.0f) iouThresh = 0.3f;

    const char* mp = std::getenv("MODEL_PATH");
    if (mp && mp[0]) modelPath = mp;
}
