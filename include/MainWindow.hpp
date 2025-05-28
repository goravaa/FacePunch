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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig& config, QWidget *parent = nullptr);
    ~MainWindow(); 

private slots:
    void onVideoFrame(const QVideoFrame &frame); // <-- For Qt 6 video frame callback

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    FaceDetector detector;
    QImage lastFrame;

    // --------- ADD THESE FOR CAMERA ----------
    QCamera* camera;
    QMediaCaptureSession* captureSession;
    QVideoSink* videoSink;
};
