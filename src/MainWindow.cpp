#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QPainter>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include "config.h"

// Constructor: Sets up camera, media pipeline, timer, and face detector
MainWindow::MainWindow(const AppConfig &config, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      detector(config.modelPath, config.maxDetections, config.confThresh, config.iouThresh) // Pass config to detector
{
    ui->setupUi(this);

    // Camera and media session setup (Qt 6 style)
    camera = new QCamera(this);
    captureSession = new QMediaCaptureSession(this);
    videoSink = new QVideoSink(this);

    captureSession->setCamera(camera);
    captureSession->setVideoSink(videoSink);

    connect(videoSink, &QVideoSink::videoFrameChanged, this, &MainWindow::onVideoFrame);

    camera->start();

    timer = new QTimer(this);
    timer->start(33); // For possible periodic updates if needed
}

MainWindow::~MainWindow()
{
    delete ui;
    delete camera;
    delete captureSession;
    delete videoSink;
    delete timer;
}

// Called on every new video frame from the camera
void MainWindow::onVideoFrame(const QVideoFrame &frame)
{
    // 1. Convert QVideoFrame to QImage
    QImage image = frame.toImage();
    if (image.isNull())
        return;

    // 2. Detect faces in the image
    std::vector<FaceDetection> faces = detector.detect(image);

    // 3. Make a copy of the image to draw on
    QImage display = image;
    QPainter painter(&display);

    // 4. Draw all detections (boxes, confidence, landmarks)
    for (const auto &f : faces)
    {
        // --- Draw face box (green) ---
        painter.setPen(QPen(Qt::green, 5));
        QRect faceRect(QPoint(static_cast<int>(f.x1), static_cast<int>(f.y1)),
                       QPoint(static_cast<int>(f.x2), static_cast<int>(f.y2)));
        painter.drawRect(faceRect);

        // --- Draw confidence ---
        painter.setPen(QPen(Qt::green, 2));
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        // Draw confidence at top-left of box (adjust -20 if needed for overlap)
        QString confText = QString("Conf: %1").arg(f.confidence, 0, 'f', 2);
        painter.drawText(static_cast<int>(f.x1), static_cast<int>(f.y1) - 8, confText);

        // --- Draw landmarks ---
        // Color order: left eye (red), right eye (red), nose (blue), mouth (magenta), left cheek (orange), right cheek (orange)
        struct LM
        {
            float x, y;
            QColor color;
        };
        std::vector<LM> landmarks = {
            {f.left_eye_x, f.left_eye_y, QColor("red")},
            {f.right_eye_x, f.right_eye_y, QColor("red")},
            {f.nose_x, f.nose_y, QColor("blue")},
            {f.mouth_x, f.mouth_y, QColor("magenta")},
            {f.left_cheek_x, f.left_cheek_y, QColor("orange")},
            {f.right_cheek_x, f.right_cheek_y, QColor("orange")},
        };

        for (const auto &lm : landmarks)
        {
            painter.setBrush(lm.color);
            painter.setPen(Qt::NoPen);                      // No border on landmark
            painter.drawEllipse(QPointF(lm.x, lm.y), 6, 6); // (cx, cy), radius 6px
        }
        painter.setBrush(Qt::NoBrush);      // Reset brush
        painter.setPen(QPen(Qt::green, 5)); // Reset pen
    }

    // 5. If no faces found, overlay message
    if (faces.empty())
    {
        painter.setPen(QPen(Qt::red, 2));
        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(display.rect(), Qt::AlignCenter, "No Face Detected");
    }
    painter.end();

    // 6. Show the result in the QLabel on the UI
    ui->CameraLabel->setPixmap(QPixmap::fromImage(display));
}
