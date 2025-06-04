// src/MainWindow.cpp
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
#include <QInputDialog>
#include <QMessageBox>
#include "FaceEmbedder.hpp"


MainWindow::MainWindow(const AppConfig &config, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      detector(config.modelPath, config.maxDetections, config.confThresh, config.iouThresh)
{
    embedder = new FaceEmbedder("assets/models/arc.onnx"); // adjust path
    faceIndex = new FaceIndex(512);                        // 512 for ArcFace
    faceIndex->loadFromDisk("face_db.csv");
    ui->setupUi(this);

    connect(ui->RegisterUserButton, &QPushButton::clicked, this, &MainWindow::onRegisterUser);

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

// Crops and returns a face image from detected bounding box (with some margin)
QImage MainWindow::cropFace(const QImage &img, const FaceDetection &fd)
{
    int x = std::max(0, int(fd.x1));
    int y = std::max(0, int(fd.y1));
    int w = std::min(img.width() - x, int(fd.x2 - fd.x1));
    int h = std::min(img.height() - y, int(fd.y2 - fd.y1));
    return img.copy(x, y, w, h).scaled(112, 112); // always scale to ArcFace size
}

void MainWindow::onRegisterUser()
{
    // 1. If no face in cache or no "Unknown", abort
    if (faceCache.empty() || std::none_of(faceCache.begin(), faceCache.end(),
        [](const CachedFace& c) { return c.name == "Unknown"; })) {
        QMessageBox::information(this, "Info", "No unknown face detected to register!");
        return;
    }

    // 2. Ask for the name
    bool ok;
    QString name = QInputDialog::getText(this, "Register User", "Enter Name for this face:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    // 3. Register the first "Unknown" face currently shown
    for (const CachedFace& cf : faceCache) {
        if (cf.name == "Unknown") {
            // Crop face region from the current frame as displayed (optionally with margin)
            QImage cropped = cropFace(lastFrame, FaceDetection{
                static_cast<float>(cf.box.left()),
                static_cast<float>(cf.box.top()),
                static_cast<float>(cf.box.right()),
                static_cast<float>(cf.box.bottom()),
                cf.left_eye_x, cf.left_eye_y,
                cf.right_eye_x, cf.right_eye_y,
                cf.nose_x, cf.nose_y,
                cf.mouth_x, cf.mouth_y,
                cf.left_cheek_x, cf.left_cheek_y,
                cf.right_cheek_x, cf.right_cheek_y,
                cf.conf
            });

            std::vector<float> emb = embedder->getEmbedding(cropped);
            faceIndex->add(name.trimmed().toStdString(), emb);
            faceIndex->saveToDisk("face_db.csv");
            QMessageBox::information(this, "Success", "User '" + name + "' registered successfully!");
            return;
        }
    }

    // Should not reach here, but fallback
    QMessageBox::information(this, "Info", "No unknown face detected to register!");
}

void MainWindow::onVideoFrame(const QVideoFrame &frame)
{
    QImage image = frame.toImage();
    if (image.isNull())
        return;

    frameCount++;

    // 1. Age/expire cache
    for (auto it = faceCache.begin(); it != faceCache.end(); ) {
        if (--(it->ttl) <= 0) it = faceCache.erase(it);
        else ++it;
    }

    // 2. Only run ONNX heavy pipeline every frameSkip-th frame or if cache empty
    bool doDetect = (frameCount % frameSkip == 0) || faceCache.empty();

    if (doDetect) {
        faceCache.clear();
        std::vector<FaceDetection> faces = detector.detect(image);
        for (const auto &f : faces) {
            QImage cropped = cropFace(image, f);
            std::vector<float> emb = embedder->getEmbedding(cropped);
            auto match = faceIndex->search(emb, 0.85f);

            CachedFace cf;
            cf.box = QRect(QPoint(int(f.x1), int(f.y1)), QPoint(int(f.x2), int(f.y2)));
            cf.name = match.first.empty() ? "Unknown" : match.first;
            cf.conf = f.confidence;
            cf.similarity = match.second;
            
            // Cache landmarks if you want landmark overlay every frame
            cf.left_eye_x = f.left_eye_x; cf.left_eye_y = f.left_eye_y;
            cf.right_eye_x = f.right_eye_x; cf.right_eye_y = f.right_eye_y;
            cf.nose_x = f.nose_x; cf.nose_y = f.nose_y;
            cf.mouth_x = f.mouth_x; cf.mouth_y = f.mouth_y;
            cf.left_cheek_x = f.left_cheek_x; cf.left_cheek_y = f.left_cheek_y;
            cf.right_cheek_x = f.right_cheek_x; cf.right_cheek_y = f.right_cheek_y;
            cf.ttl = cacheTTL;
            faceCache.push_back(cf);
        }
    }

    // 3. Draw overlays from cache on every frame (full speed)
    QImage display = image;
    QPainter painter(&display);

    for (const auto &cf : faceCache) {
        // Draw face box
        painter.setPen(QPen(Qt::green, 5));
        painter.drawRect(cf.box);

        // Draw name
        painter.setPen(QPen(Qt::yellow, 2));
        painter.setFont(QFont("Arial", 16, QFont::Bold));
        painter.drawText(cf.box.topLeft() - QPoint(0, 24), QString::fromStdString(cf.name));

        // Draw confidence or similarity
        painter.setPen(QPen(Qt::green, 2));
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        if (cf.name != "Unknown") {
            painter.drawText(cf.box.topLeft() - QPoint(0, 8), QString("Sim: %1").arg(cf.similarity, 0, 'f', 2));
        } else {
            painter.drawText(cf.box.topLeft() - QPoint(0, 8), QString("Conf: %1").arg(cf.conf, 0, 'f', 2));
        }

        // Draw landmarks
        struct LM { float x, y; QColor color; };
        std::vector<LM> landmarks = {
            {cf.left_eye_x, cf.left_eye_y, QColor("red")},
            {cf.right_eye_x, cf.right_eye_y, QColor("red")},
            {cf.nose_x, cf.nose_y, QColor("blue")},
            {cf.mouth_x, cf.mouth_y, QColor("magenta")},
            {cf.left_cheek_x, cf.left_cheek_y, QColor("orange")},
            {cf.right_cheek_x, cf.right_cheek_y, QColor("orange")},
        };
        for (const auto &lm : landmarks) {
            painter.setBrush(lm.color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(lm.x, lm.y), 6, 6);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::green, 5));
    }

    if (faceCache.empty()) {
        painter.setPen(QPen(Qt::red, 2));
        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(display.rect(), Qt::AlignCenter, "No Face Detected");
    }
    painter.end();

    ui->CameraLabel->setPixmap(QPixmap::fromImage(display));

    // Show Register button if any "Unknown" in cache
    bool showRegister = !faceCache.empty() && std::any_of(faceCache.begin(), faceCache.end(),
                                [](const CachedFace& cf){ return cf.name == "Unknown"; });
    ui->RegisterUserButton->setVisible(showRegister);

   
    recentResults.clear();
    for (const auto &cf : faceCache)
        recentResults.push_back(cf.name);
}
