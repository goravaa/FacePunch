//include/FaceDetector.hpp
#pragma once

#include <QImage>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include <memory> // For smart pointers

// Struct for one detected face, including box and all landmarks
struct FaceDetection {
    float x1, y1, x2, y2;
    float confidence;
    float left_eye_x, left_eye_y;
    float right_eye_x, right_eye_y;
    float nose_x, nose_y;
    float mouth_x, mouth_y;
    float left_cheek_x, left_cheek_y;
    float right_cheek_x, right_cheek_y;
};

// FaceDetector now takes config values (best practice!)
class FaceDetector {
public:
    FaceDetector(const std::string& model_path,
                 int maxDetections,
                 float confThresh,
                 float iouThresh);

    std::vector<FaceDetection> detect(const QImage& img);

private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions session_options;

    int maxDetections;
    float confThresh;
    float iouThresh;
};
