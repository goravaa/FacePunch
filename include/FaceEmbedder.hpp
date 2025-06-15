// FaceEmbedder.hpp
#pragma once

#include <vector>
#include <string>
#include <onnxruntime_cxx_api.h>
#include <QImage>

// This class handles extracting face embeddings from a face image using ArcFace ONNX
class FaceEmbedder {
public:
    // Constructor: loads the ONNX model from given path
    FaceEmbedder(const std::string& modelPath);
    
    // Given a QImage (face, RGB), returns a 512-dim normalized embedding vector
    std::vector<float> getEmbedding(const QImage& face);

private:
    Ort::Env env;                    // ONNX environment
    Ort::SessionOptions sessionOpts; // Session options (optimizations)
    std::unique_ptr<Ort::Session> session; // The loaded ArcFace model

    // Helper: preprocess QImage to model input (1, 3, 112, 112) float32
    std::vector<float> preprocess(const QImage& img);
};
