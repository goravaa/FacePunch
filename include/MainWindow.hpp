// include/MainWindow.hpp
#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QImage>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include "FaceDetector.hpp"
#include "config.h"
#include "FaceEmbedder.hpp"
#include "FaceIndex.hpp"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig &config, QWidget *parent = nullptr);
    ~MainWindow();
    FaceEmbedder *embedder = nullptr;       // For embedding
    FaceIndex *faceIndex = nullptr;         // For storing/searching
    std::vector<std::string> recentResults; // To track per-frame results
    

    void updateRecognitionUI(const std::vector<FaceDetection> &faces, QImage &image);

private slots:
    void onVideoFrame(const QVideoFrame &frame); // <-- For Qt 6 video frame callback
    void onRegisterUser();     
    QImage cropFace(const QImage &img, const FaceDetection &fd);


private:
    Ui::MainWindow *ui;
    QTimer *timer;
    FaceDetector detector;
    QImage lastFrame;

    // --------- ADD THESE FOR CAMERA ----------
    QCamera *camera;
    QMediaCaptureSession *captureSession;
    QVideoSink *videoSink;

 struct CachedFace {
    QRect box;
    std::string name;
    float conf;
    // You can add landmark points here if you want landmark overlay cached too
    float left_eye_x, left_eye_y;
    float right_eye_x, right_eye_y;
    float nose_x, nose_y;
    float mouth_x, mouth_y;
    float left_cheek_x, left_cheek_y;
    float right_cheek_x, right_cheek_y;
    int ttl; // frames left
    float similarity;
};

int frameCount = 0;
const int frameSkip = 3;  // ONNX every 3rd frame
const int cacheTTL  = 3;  // how many frames to keep a detection
std::vector<CachedFace> faceCache;

};
