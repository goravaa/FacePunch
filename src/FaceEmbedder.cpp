// FaceEmbedder.cpp

#include "FaceEmbedder.hpp"
#include <algorithm>
#include <cmath>

// Constructor: loads ONNX model and sets up inference session
FaceEmbedder::FaceEmbedder(const std::string& modelPath)
    : env(ORT_LOGGING_LEVEL_WARNING, "arcface_embedder")
{
    sessionOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    std::wstring wModelPath(modelPath.begin(), modelPath.end());
    session = std::make_unique<Ort::Session>(env, wModelPath.c_str(), sessionOpts);
}

// Preprocess image for ArcFace: resize to 112x112, RGB, float32, normalize
// Output is NHWC: [1, 112, 112, 3] (as your ONNX model expects)
std::vector<float> FaceEmbedder::preprocess(const QImage& img)
{
    // 1. Resize to 112x112 and convert to RGB
    QImage rgb = img.convertToFormat(QImage::Format_RGB888).scaled(112, 112);

    // 2. Prepare buffer for NHWC: (batch=1, height, width, channel)
    std::vector<float> data(1 * 112 * 112 * 3);

    // 3. Fill buffer in NHWC order (row, col, channel)
    // Explanation:
    // For each pixel, channels are stored together: [R, G, B], [R, G, B], ...
    for (int y = 0; y < 112; ++y) {
        for (int x = 0; x < 112; ++x) {
            QColor c = rgb.pixelColor(x, y);
            int idx = (y * 112 + x) * 3; // NHWC: [batch=0, y, x, channel]
            data[idx + 0] = (c.red()   - 127.5f) / 128.0f;   // Red
            data[idx + 1] = (c.green() - 127.5f) / 128.0f;   // Green
            data[idx + 2] = (c.blue()  - 127.5f) / 128.0f;   // Blue
        }
    }
    return data;
}


// Main function: run ONNX inference and return normalized 512-d embedding
std::vector<float> FaceEmbedder::getEmbedding(const QImage& face)
{
    std::vector<float> input = preprocess(face);

    // Prepare tensor: shape (1, 3, 112, 112)
    std::array<int64_t, 4> inputShape = {1, 112, 112, 3}; // NHWC

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, input.data(), input.size(), inputShape.data(), inputShape.size()
    );

    // Get input and output names
    Ort::AllocatorWithDefaultOptions allocator;
    auto inputName = session->GetInputNameAllocated(0, allocator);
    auto outputName = session->GetOutputNameAllocated(0, allocator);

    // Run inference
    std::array<const char*, 1> inputNames = {inputName.get()};
    std::array<const char*, 1> outputNames = {outputName.get()};

    auto outputTensors = session->Run(
        Ort::RunOptions{nullptr},
        inputNames.data(), &inputTensor, 1,
        outputNames.data(), 1
    );

    // Get output data (512-d vector)
    float* outputData = outputTensors[0].GetTensorMutableData<float>();

    std::vector<float> embedding(outputData, outputData + 512);

    // Normalize to unit length (so dot = cosine)
    float norm = 0.0f;
    for (float v : embedding) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0) {
        for (float& v : embedding) v /= norm;
    }
    return embedding;
}
