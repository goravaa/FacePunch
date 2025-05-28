#include "FaceDetector.hpp"
#include <QImage>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <stdexcept>  // For std::runtime_error
#include <filesystem> // For checking if model file exists
#include <cwchar>     // For wide character string conversion
#include <QDebug>     // For debug output

// Constructor: initializes ONNX session and stores config
FaceDetector::FaceDetector(const std::string& model_path,
                           int maxDetections_,
                           float confThresh_,
                           float iouThresh_)
    : env(ORT_LOGGING_LEVEL_WARNING, "face_detector"),
      maxDetections(maxDetections_),
      confThresh(confThresh_),
      iouThresh(iouThresh_)
{
    // Enable all graph optimizations (makes inference faster)
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Verify the model file exists (avoid silent fails)
    if (!std::filesystem::exists(model_path)) {
        throw std::runtime_error("Model file not found: " + model_path);
    }

    // Windows ONNX Runtime API expects wide string paths for Unicode support
    std::wstring wide_model_path(model_path.begin(), model_path.end());

    // Create the ONNX session for inference (loads the model into memory)
    session = std::make_unique<Ort::Session>(env, wide_model_path.c_str(), session_options);
}

std::vector<FaceDetection> FaceDetector::detect(const QImage& img) {
    std::vector<FaceDetection> results; // Output: List of detected faces

    // 1. Convert input image to RGB888, resize to 128x128, normalize to [0, 1]
    QImage rgb = img.convertToFormat(QImage::Format_RGB888).scaled(128, 128);

    // 2. Copy image data to a float vector (HWC format)
    std::vector<float> input_tensor_values(128 * 128 * 3);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            QColor color = rgb.pixelColor(x, y);
            input_tensor_values[(y * 128 + x) * 3 + 0] = color.red() / 255.0f;
            input_tensor_values[(y * 128 + x) * 3 + 1] = color.green() / 255.0f;
            input_tensor_values[(y * 128 + x) * 3 + 2] = color.blue() / 255.0f;
        }
    }

    // 3. Convert to NCHW format (C, H, W)
    std::vector<float> chw(1 * 3 * 128 * 128);
    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < 128; ++h) {
            for (int w = 0; w < 128; ++w) {
                chw[c * 128 * 128 + h * 128 + w] = input_tensor_values[(h * 128 + w) * 3 + c];
            }
        }
    }

    // 4. Prepare ONNX inputs (4 total)
    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_ptr    = session->GetInputNameAllocated(0, allocator);
    auto conf_name_ptr     = session->GetInputNameAllocated(1, allocator);
    auto maxdets_name_ptr  = session->GetInputNameAllocated(2, allocator);
    auto iou_name_ptr      = session->GetInputNameAllocated(3, allocator);

    const char* input_name     = input_name_ptr.get();
    const char* conf_name      = conf_name_ptr.get();
    const char* maxdets_name   = maxdets_name_ptr.get();
    const char* iou_name       = iou_name_ptr.get();

    std::array<int64_t, 4> input_shape = {1, 3, 128, 128};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, chw.data(), chw.size(), input_shape.data(), input_shape.size());

    // Use instance variables for config!
    float conf_thresh = confThresh;
    int64_t max_detections = maxDetections;
    float iou_thresh = iouThresh;

    std::array<int64_t, 1> single_dim = {1};
    Ort::Value conf_tensor     = Ort::Value::CreateTensor<float>(memory_info, &conf_thresh, 1, single_dim.data(), 1);
    Ort::Value maxdets_tensor  = Ort::Value::CreateTensor<int64_t>(memory_info, &max_detections, 1, single_dim.data(), 1);
    Ort::Value iou_tensor      = Ort::Value::CreateTensor<float>(memory_info, &iou_thresh, 1, single_dim.data(), 1);

    std::array<const char*, 4> input_names = {input_name, conf_name, maxdets_name, iou_name};
    std::array<Ort::Value, 4>  input_tensors = {
        std::move(input_tensor),
        std::move(conf_tensor),
        std::move(maxdets_tensor),
        std::move(iou_tensor)
    };

    std::vector<std::string> output_names_str = session->GetOutputNames();
    std::vector<const char*> output_names;
    for (const auto& name : output_names_str)
        output_names.push_back(name.c_str());

    // 5. Run the model!
    auto output_tensors = session->Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        input_tensors.data(),
        input_tensors.size(),
        output_names.data(),
        output_names.size()
    );

    // 6. Parse model output
    const float* boxes_data = output_tensors[0].GetTensorData<float>();
    const float* scores_data = (output_tensors.size() > 1) ? output_tensors[1].GetTensorData<float>() : nullptr;

    auto boxes_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t num_boxes = (boxes_shape.size() > 1) ? boxes_shape[1] : boxes_shape[0];
    if (boxes_shape.size() == 1)
        num_boxes = 1; // If only one detection

    int orig_w = img.width();
    int orig_h = img.height();

    for (size_t i = 0; i < num_boxes; ++i) {
        const float* det = boxes_data + i * 16;

        float y1 = det[0], x1 = det[1], y2 = det[2], x2 = det[3];
        float ley_x = det[4], ley_y = det[5];
        float rey_x = det[6], rey_y = det[7];
        float nose_x = det[8], nose_y = det[9];
        float mou_x = det[10], mou_y = det[11];
        float lea_x = det[12], lea_y = det[13];
        float rea_x = det[14], rea_y = det[15];

        float confidence = scores_data ? scores_data[i] : 1.0f;

        // Scale from normalized [0,1] to image coordinates
        float fx1 = x1 * orig_w;
        float fy1 = y1 * orig_h;
        float fx2 = x2 * orig_w;
        float fy2 = y2 * orig_h;

        if (fx2 - fx1 < 5 || fy2 - fy1 < 5)
            continue;

        // Landmarks, scaled to original image
        FaceDetection fd;
        fd.x1 = fx1;
        fd.y1 = fy1;
        fd.x2 = fx2;
        fd.y2 = fy2;
        fd.confidence = confidence;
        fd.left_eye_x = ley_x * orig_w;
        fd.left_eye_y = ley_y * orig_h;
        fd.right_eye_x = rey_x * orig_w;
        fd.right_eye_y = rey_y * orig_h;
        fd.nose_x = nose_x * orig_w;
        fd.nose_y = nose_y * orig_h;
        fd.mouth_x = mou_x * orig_w;
        fd.mouth_y = mou_y * orig_h;
        fd.left_cheek_x = lea_x * orig_w;
        fd.left_cheek_y = lea_y * orig_h;
        fd.right_cheek_x = rea_x * orig_w;
        fd.right_cheek_y = rea_y * orig_h;

        // Log everything
        qDebug() << "Face" << i << "box:" << fx1 << fy1 << fx2 << fy2 << "conf:" << confidence
                 << "ley:" << fd.left_eye_x << fd.left_eye_y
                 << "rey:" << fd.right_eye_x << fd.right_eye_y
                 << "nose:" << fd.nose_x << fd.nose_y
                 << "mouth:" << fd.mouth_x << fd.mouth_y
                 << "lea:" << fd.left_cheek_x << fd.left_cheek_y
                 << "rea:" << fd.right_cheek_x << fd.right_cheek_y;

        results.push_back(fd);
    }

    return results;
}
