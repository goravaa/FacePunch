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
        // Calculate tight bounding box from landmarks
        float min_x = std::min({f.left_eye_x, f.right_eye_x, f.nose_x, f.mouth_x, f.left_cheek_x, f.right_cheek_x});
        float max_x = std::max({f.left_eye_x, f.right_eye_x, f.nose_x, f.mouth_x, f.left_cheek_x, f.right_cheek_x});
        float min_y = std::min({f.left_eye_y, f.right_eye_y, f.nose_y, f.mouth_y, f.left_cheek_y, f.right_cheek_y});
        float max_y = std::max({f.left_eye_y, f.right_eye_y, f.nose_y, f.mouth_y, f.left_cheek_y, f.right_cheek_y});

        // Optional: add padding for comfort
        float padding = 20;
        min_x = std::max(0.0f, min_x - padding);
        max_x = std::min(float(image.width() - 1), max_x + padding);
        min_y = std::max(0.0f, min_y - padding);
        max_y = std::min(float(image.height() - 1), max_y + padding);

        float box_width = max_x - min_x;
        float box_height = max_y - min_y;

        // Add padding
        float expand_top = box_height * 0.40f;    // 35% above min_y
        float expand_bottom = box_height * 0.45f; // 40% below max_y
           // 10% left and right

        float tight_x1 = std::max(0.0f, min_x);
        float tight_x2 = std::min(float(display.width()), max_x);
        float tight_y1 = std::max(0.0f, min_y - expand_top);
        float tight_y2 = std::min(float(display.height()), max_y + expand_bottom);

        QRect tightRect(QPoint(static_cast<int>(tight_x1), static_cast<int>(tight_y1)),
                        QPoint(static_cast<int>(tight_x2), static_cast<int>(tight_y2)));

        painter.setPen(QPen(Qt::green, 5));
        painter.drawRect(tightRect);

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
