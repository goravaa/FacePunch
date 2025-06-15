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
#include "SettingsDialog.hpp" // Include SettingsDialog
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget> // Already included via QMainWindow but good for clarity
#include <QHeaderView> // For QTableWidget column sizing


#include <QMediaDevices> // For QMediaDevices

MainWindow::MainWindow(const AppConfig &config, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_appConfig(config)
{
    ui->setupUi(this); // Creates ui->centralwidget, ui->CameraLabel, ui->RegisterUserButton etc.

    // Main Tab Widget setup
    mainTabWidget = new QTabWidget(this);

    // Live View Tab (first tab)
    liveViewTab = new QWidget(this);
    QVBoxLayout *liveViewLayout = new QVBoxLayout(liveViewTab);
    liveViewLayout->addWidget(ui->CameraLabel); // Move CameraLabel here
    // Assuming RegisterUserButton is contextually part of live view
    if (ui->RegisterUserButton) { // ui->RegisterUserButton might not exist if UI file changes
        liveViewLayout->addWidget(ui->RegisterUserButton);
    }
    mainTabWidget->addTab(liveViewTab, tr("Live View"));

    // User Management Tab (second tab)
    userManagementTab = new QWidget(this);
    userManagementLayout = new QVBoxLayout(userManagementTab);

    userTableWidget = new QTableWidget(this);
    userTableWidget->setColumnCount(2);
    userTableWidget->setHorizontalHeaderLabels({"User ID", "Name"});
    userTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    userTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    userTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    userTableWidget->verticalHeader()->setVisible(false); // Hide vertical row numbers
    userTableWidget->horizontalHeader()->setStretchLastSection(true); // Name column fills space

    refreshUserListButton = new QPushButton(tr("Refresh List"), this);
    connect(refreshUserListButton, &QPushButton::clicked, this, &MainWindow::populateUserTable);

    deleteUserButton = new QPushButton(tr("Delete Selected User"), this);
    connect(deleteUserButton, &QPushButton::clicked, this, &MainWindow::onDeleteUserClicked);

    // Layout for buttons
    QHBoxLayout *userMgmtButtonLayout = new QHBoxLayout();
    userMgmtButtonLayout->addWidget(refreshUserListButton);
    userMgmtButtonLayout->addWidget(deleteUserButton);
    editUserNameButton = new QPushButton(tr("Edit Selected Name"), this);
    connect(editUserNameButton, &QPushButton::clicked, this, &MainWindow::onEditUserNameClicked);
    userMgmtButtonLayout->addWidget(editUserNameButton);
    userMgmtButtonLayout->addStretch(); // Add spacer to push buttons to one side if desired

    userManagementLayout->addLayout(userMgmtButtonLayout); // Add button layout
    userManagementLayout->addWidget(userTableWidget);
    mainTabWidget->addTab(userManagementTab, tr("User Management"));

    // Set the tab widget as the central widget of MainWindow
    // If ui->centralwidget exists and has a layout, add mainTabWidget to that layout.
    // For simplicity, if ui->centralwidget is just a plain QWidget, we replace it.
    // However, QMainWindow usually expects setCentralWidget.
    // The ui file likely has ui->centralwidget. Let's set mainTabWidget as the central widget.
    // Any other widgets from the .ui file not explicitly moved will be orphaned if not in a menu/toolbar.
    // This assumes CameraLabel and RegisterUserButton were the main content of centralwidget.
    // setCentralWidget(mainTabWidget); // Will be set after all tabs are added

    // Attendance Log Tab (third tab)
    attendanceLogTab = new QWidget(this);
    attendanceLogLayout = new QVBoxLayout(attendanceLogTab);

    attendanceTableWidget = new QTableWidget(this);
    attendanceTableWidget->setColumnCount(3);
    attendanceTableWidget->setHorizontalHeaderLabels({"Timestamp", "User ID", "User Name"});
    attendanceTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    attendanceTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    attendanceTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    attendanceTableWidget->verticalHeader()->setVisible(false);
    attendanceTableWidget->horizontalHeader()->setStretchLastSection(true); // User Name column fills space
    // Allow sorting by clicking headers for attendance log
    attendanceTableWidget->setSortingEnabled(true);


    refreshLogButton = new QPushButton(tr("Refresh Log"), this);
    connect(refreshLogButton, &QPushButton::clicked, this, &MainWindow::populateAttendanceTable);

    attendanceLogLayout->addWidget(refreshLogButton);
    attendanceLogLayout->addWidget(attendanceTableWidget);
    mainTabWidget->addTab(attendanceLogTab, tr("Attendance Log"));

    setCentralWidget(mainTabWidget); // Set central widget after all tabs are configured


    try {
        // Detector initialization
        detector = std::make_unique<FaceDetector>(m_appConfig.modelPath, m_appConfig.maxDetections, m_appConfig.confThresh, m_appConfig.iouThresh);

        // Embedder initialization
        embedder = std::make_unique<FaceEmbedder>(m_appConfig.arcfaceModelPath);

        // FaceIndex initialization and loading
        faceIndex = std::make_unique<FaceIndex>(512, m_appConfig.maxFaceIndexSize);
        faceIndex->loadFromDisk(m_appConfig.faceDatabasePath);

    } catch (const Ort::Exception& ort_err) {
        QMessageBox::critical(this, "Critical Error", QString("ONNX Runtime Error: %1\nApplication will now exit.").arg(ort_err.what()));
        QTimer::singleShot(0, this, &QWidget::close); // Close after message box
        return;
    } catch (const std::runtime_error& err) {
        QMessageBox::critical(this, "Critical Error", QString("Initialization Error: %1\nApplication will now exit.").arg(err.what()));
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Critical Error", QString("An unexpected error occurred during initialization: %1\nApplication will now exit.").arg(ex.what()));
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    // Connect signals/slots and start camera AFTER successful initialization
    connect(ui->RegisterUserButton, &QPushButton::clicked, this, &MainWindow::onRegisterUser);

    // File Menu for Settings
    fileMenu = menuBar()->addMenu(tr("&File"));
    settingsAction = new QAction(tr("&Settings..."), this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    fileMenu->addAction(settingsAction);

    // Camera setup
    camera = new QCamera(this);
    captureSession = new QMediaCaptureSession(this);
    videoSink = new QVideoSink(this);

    captureSession->setCamera(camera);
    captureSession->setVideoSink(videoSink);

    connect(videoSink, &QVideoSink::videoFrameChanged, this, &MainWindow::onVideoFrame);

    if (QMediaDevices::defaultVideoInput().isNull()) {
         QMessageBox::warning(this, "Camera Error", "No default camera found. Please ensure a camera is connected and configured.");
    } else {
        camera->start();
        if (camera->error() != QCamera::NoError) {
            QMessageBox::warning(this, "Camera Error", "Could not start camera: " + camera->errorString());
        }
    }

    timer = new QTimer(this);
    timer->start(33);

    populateUserTable(); // Initial population
}

MainWindow::~MainWindow()
{
    delete ui;
    delete camera;
    delete captureSession;
    delete videoSink;
    delete timer;
}

QImage MainWindow::alignFace(const QImage &sourceImage, const FaceDetection &detectedFace)
{
    // Target landmark positions in the 112x112 aligned image
    const double lx_tgt = 30.0;
    const double ly_tgt = 48.0;
    const double rx_tgt = 82.0;
    // const double ry_tgt = 48.0; // Not needed as ly_tgt == ry_tgt

    // Source landmark positions
    double lx_src = detectedFace.left_eye_x;
    double ly_src = detectedFace.left_eye_y;
    double rx_src = detectedFace.right_eye_x;
    double ry_src = detectedFace.right_eye_y;

    // Calculate source eye center and vector
    double mid_x_src = (lx_src + rx_src) / 2.0;
    double mid_y_src = (ly_src + ry_src) / 2.0;
    double dx_src = rx_src - lx_src;
    double dy_src = ry_src - ly_src;

    // Angle of the source eyes vector
    double angle_src = std::atan2(dy_src, dx_src);
    // Target angle is 0 (horizontal)
    double rotation_angle_rad = -angle_src; // Rotate by negative angle to make it horizontal
    double rotation_angle_deg = rotation_angle_rad * 180.0 / M_PI;

    // Scale factor
    double dist_src = std::sqrt(dx_src * dx_src + dy_src * dy_src);
    const double dist_tgt = rx_tgt - lx_tgt; // 52.0

    // Avoid division by zero or very small distance
    if (dist_src < 1e-6) {
        // Fallback to simple crop if eyes are too close (bad detection)
        return cropFace(sourceImage, detectedFace);
    }
    double scale = dist_tgt / dist_src;

    // Transformation:
    // 1. Translate source midpoint to origin
    // 2. Rotate
    // 3. Scale
    // 4. Translate origin to target midpoint (56, 48)
    QTransform transform;
    transform.translate(mid_x_src, mid_y_src); // Prepare for rotation around source midpoint
    transform.rotateRadians(rotation_angle_rad);
    transform.scale(scale, scale);
    transform.translate(-mid_x_src, -mid_y_src); // Translate source midpoint (now scaled and rotated) to origin

    // Now, map the source midpoint (which should become target midpoint) to the target midpoint
    // The current transform makes (mid_x_src, mid_y_src) behave like the origin for subsequent transforms
    // We want mid_x_src, mid_y_src to map to 56,48 in the *final coordinate system of the output image*
    // The QImage::transformed() function creates a new image. We need to crop from it.
    // The transform should map source coordinates to target *canvas* coordinates.

    QTransform final_transform;
    // Step 1: Translate source eye midpoint to origin
    final_transform.translate(-mid_x_src, -mid_y_src);
    // Step 2: Rotate around origin so that eye line becomes horizontal
    final_transform.rotateRadians(rotation_angle_rad);
    // Step 3: Scale around origin so that eye distance becomes target distance
    final_transform.scale(scale, scale);
    // Step 4: Translate the (now aligned) source eye midpoint to the target eye midpoint
    const double mid_x_tgt = (lx_tgt + rx_tgt) / 2.0; // 56.0
    const double mid_y_tgt = (ly_tgt + ly_tgt) / 2.0; // 48.0
    final_transform.translate(mid_x_tgt, mid_y_tgt);

    // Warp the entire source image
    QImage warpedImage = sourceImage.transformed(final_transform, Qt::SmoothTransformation);

    // Create the 112x112 output image
    // The transformation `final_transform` maps a point in the original image
    // to a point in the `warpedImage`. The point (mid_x_src, mid_y_src) in original
    // is mapped to (mid_x_tgt, mid_y_tgt) in `warpedImage`.
    // We need to crop a 112x112 window from `warpedImage` centered at (mid_x_tgt, mid_y_tgt),
    // but the target landmarks are already defined for a 112x112 image.
    // So, we just need to take a 112x112 crop starting from (0,0) of an image
    // that is already aligned to have the landmarks in their target positions.

    // The `final_transform` maps original image points to a new coordinate system.
    // If we draw the original image with this transform, the point (mid_x_src, mid_y_src)
    // will land at (mid_x_tgt, mid_y_tgt).
    // We want the output image of 112x112 such that the eyes are at (30,48) and (82,48).

    QImage aligned_image(112, 112, sourceImage.format());
    aligned_image.fill(Qt::black); // Fill with black background

    QPainter painter(&aligned_image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // The transform should be set on the painter to draw the sourceImage
    // such that the source landmarks map to the target landmark positions *within the 112x112 canvas*.

    // Reset transform for clarity
    QTransform tf;
    // Translate so source eye midpoint is at origin
    tf.translate(-lx_src, -ly_src); // Let's try aligning left eye to origin first
                                     // Then rotate, scale, then translate to lx_tgt, ly_tgt

    // Simpler: compute transform that maps source points to target points
    // For eyes: (lx_src, ly_src) -> (lx_tgt, ly_tgt) and (rx_src, ry_src) -> (rx_tgt, ry_tgt)
    // QTransform can do this with QTransform::quadToQuad or from three points.
    // OpenCV's getAffineTransform uses 3 points. Here we have 2 pairs, need a 3rd.
    // Let's use the midpoint and orientation/scale method.

    // Transform to map source image region to the 112x112 canvas
    QTransform final_tf;
    final_tf.translate(lx_tgt, ly_tgt); // Target: left eye will be at (lx_tgt, ly_tgt)
    final_tf.rotateRadians(-angle_src);    // Rotate source so eyes are horizontal
    final_tf.scale(scale, scale);        // Scale so eye distance matches target
    final_tf.translate(-lx_src, -ly_src);  // Source: move left eye to origin

    painter.setTransform(final_tf);
    painter.drawImage(QPoint(0,0), sourceImage); // Draw source image transformed onto the 112x112 canvas
    painter.end();

    return aligned_image;
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
    // 1. Collect "Unknown" faces
    std::vector<CachedFace> unknown_faces;
    for (const auto& cf : faceCache) {
        if (cf.name == "Unknown") {
            unknown_faces.push_back(cf);
        }
    }

    // 2. Check count and proceed
    if (unknown_faces.empty()) {
        QMessageBox::information(this, "Info", "No unknown face detected to register. Please ensure an unknown person is in view.");
        return;
    }

    if (unknown_faces.size() > 1) {
        QMessageBox::warning(this, "Registration Ambiguity", "Multiple unknown faces detected. Please ensure only ONE unknown person is clearly in view and try again.");
        return;
    }

    // Exactly one unknown face
    const CachedFace& face_to_register = unknown_faces[0];

    // 3. Ask for the name
    bool ok;
    QString name = QInputDialog::getText(this, "Register User", "Enter Name for this face:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // 4. Reconstruct FaceDetection and align
    FaceDetection fd_to_register {
        static_cast<float>(face_to_register.box.left()),
        static_cast<float>(face_to_register.box.top()),
        static_cast<float>(face_to_register.box.right()),
        static_cast<float>(face_to_register.box.bottom()),
        face_to_register.left_eye_x, face_to_register.left_eye_y,
        face_to_register.right_eye_x, face_to_register.right_eye_y,
        face_to_register.nose_x, face_to_register.nose_y,
        face_to_register.mouth_x, face_to_register.mouth_y,
        face_to_register.left_cheek_x, face_to_register.left_cheek_y,
        face_to_register.right_cheek_x, face_to_register.right_cheek_y,
        face_to_register.conf
    };

    QImage aligned_image = alignFace(lastFrame, fd_to_register);

    if (aligned_image.isNull()) {
        QMessageBox::critical(this, "Error", "Failed to align face for registration. Please try again.");
        return;
    }

    // 5. Get embedding, add to index, save
    std::vector<float> emb = embedder->getEmbedding(aligned_image);
    faceIndex->add(name.trimmed().toStdString(), emb);
    faceIndex->saveToDisk(m_appConfig.faceDatabasePath);
    QMessageBox::information(this, "Success", "User '" + name.trimmed() + "' registered successfully!");
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

    if (detector && doDetect) { // Check if detector is initialized
        faceCache.clear();
        std::vector<FaceDetection> faces = detector->detect(image); // Use ->
        for (const auto &f : faces) {
            QImage aligned_face = alignFace(image, f);
            if (aligned_face.isNull()) continue; // Skip if alignment failed

            std::vector<float> emb = embedder->getEmbedding(aligned_face);
            FaceIndex::SearchResult search_result = faceIndex->search(emb, m_appConfig.similarityThreshold);

            CachedFace cf;
            cf.box = QRect(QPoint(int(f.x1), int(f.y1)), QPoint(int(f.x2), int(f.y2)));
            cf.name = search_result.found ? search_result.name : "Unknown";
            cf.conf = f.confidence; // Original detection confidence
            cf.similarity = search_result.similarity;
            cf.userId = search_result.id; // Store user ID in cache

            // Log attendance if a known user is found
            if (search_result.found && search_result.id != 0) {
                QDateTime current_time = QDateTime::currentDateTime();
                bool should_log = true;
                if (m_lastLogTimestamps.count(search_result.id)) {
                    if (m_lastLogTimestamps[search_result.id].secsTo(current_time) < m_attendanceLogDebounceSecs) {
                        should_log = false;
                    }
                }

                if (should_log) {
                    m_lastLogTimestamps[search_result.id] = current_time; // Update last log time

                    QFile logFile(QString::fromStdString(m_appConfig.attendanceLogPath));
                    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
                        QTextStream stream(&logFile);
                        // Check if file is new/empty to write headers
                        if (logFile.pos() == 0) {
                            stream << "Timestamp,UserID,UserName\n";
                        }
                        stream << current_time.toString(Qt::ISODate) << ","
                               << search_result.id << ","
                               << QString::fromStdString(search_result.name) << "\n";
                        logFile.close();
                    } else {
                        qWarning() << "Could not open attendance log file for writing:" << QString::fromStdString(m_appConfig.attendanceLogPath);
                    }
                }
            }
            
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

void MainWindow::openSettingsDialog()
{
    SettingsDialog dialog(m_appConfig, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Apply settings that can be changed at runtime by re-initializing components
        // This is a simplified approach. A more granular update might be preferred in a complex app.
        try {
            // Re-initialize FaceDetector
            detector = std::make_unique<FaceDetector>(m_appConfig.modelPath, m_appConfig.maxDetections, m_appConfig.confThresh, m_appConfig.iouThresh);

            // Re-initialize FaceEmbedder
            embedder = std::make_unique<FaceEmbedder>(m_appConfig.arcfaceModelPath);

            // Re-initialize FaceIndex (dimension 512 is hardcoded for ArcFace)
            faceIndex = std::make_unique<FaceIndex>(512, m_appConfig.maxFaceIndexSize);
            faceIndex->loadFromDisk(m_appConfig.faceDatabasePath);

            QMessageBox::information(this, "Settings Applied", "Settings have been applied. Critical components were re-initialized.");

        } catch (const Ort::Exception& ort_err) {
            QMessageBox::critical(this, "Error Applying Settings", QString("ONNX Runtime Error: %1\nPlease check model paths or restart.").arg(ort_err.what()));
        } catch (const std::runtime_error& err) {
            QMessageBox::critical(this, "Error Applying Settings", QString("Initialization Error: %1\nPlease check configuration or restart.").arg(err.what()));
        } catch (const std::exception& ex) {
            QMessageBox::critical(this, "Error Applying Settings", QString("An unexpected error occurred: %1\nPlease restart.").arg(ex.what()));
        }
        // Note: The FaceIndex is re-initialized within the try block if settings are accepted.
        // The duplicated lines that were here have been removed.

        // Optionally, inform the user that some settings may require a restart if not handled live.
        // QMessageBox::information(this, "Settings Changed", "Some settings have been updated. Active components have been re-initialized.");
    }
}

void MainWindow::populateUserTable()
{
    if (!faceIndex) return; // Ensure faceIndex is initialized

    userTableWidget->setRowCount(0); // Clear existing rows

    const auto& id_map = faceIndex->getIdToNameMap();
    userTableWidget->setSortingEnabled(false); // Disable sorting during population for speed

    for (const auto& pair : id_map) {
        int row = userTableWidget->rowCount();
        userTableWidget->insertRow(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(pair.first));
        QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(pair.second));

        // Items should be non-editable if table is NoEditTriggers, but explicit is fine
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

        userTableWidget->setItem(row, 0, idItem);
        userTableWidget->setItem(row, 1, nameItem);
    }
    userTableWidget->resizeColumnsToContents(); // Adjust column widths based on content
    userTableWidget->setSortingEnabled(true); // Re-enable sorting
}

void MainWindow::onDeleteUserClicked()
{
    if (!faceIndex) {
        QMessageBox::critical(this, "Error", "Face index not available.");
        return;
    }

    QList<QTableWidgetItem*> selectedItems = userTableWidget->selectedItems();
    if (userTableWidget->selectionModel()->selectedRows().isEmpty()) {
        QMessageBox::information(this, "Delete User", "Please select a user from the list to delete.");
        return;
    }

    int selectedRow = userTableWidget->selectionModel()->selectedRows().first().row();
    QTableWidgetItem *idItem = userTableWidget->item(selectedRow, 0);
    QTableWidgetItem *nameItem = userTableWidget->item(selectedRow, 1); // For user-friendly message

    if (!idItem || !nameItem) {
        QMessageBox::warning(this, "Delete User", "Could not retrieve user details from selection.");
        return; // Should not happen with proper selection
    }

    QString userName = nameItem->text();
    bool ok_id;
    size_t userId = idItem->text().toULongLong(&ok_id);

    if (!ok_id) {
        QMessageBox::warning(this, "Delete User", QString("Invalid User ID format for '%1'.").arg(userName));
        return;
    }

    auto reply = QMessageBox::question(this, "Confirm Delete",
                                       QString("Are you sure you want to delete user '%1' (ID: %2)? This action cannot be undone.")
                                       .arg(userName).arg(userId),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        bool deleted = faceIndex->deleteUser(userId);
        if (deleted) {
            faceIndex->saveToDisk(m_appConfig.faceDatabasePath); // Persist the change
            populateUserTable(); // Refresh the table
            QMessageBox::information(this, "Delete User", QString("User '%1' (ID: %2) deleted successfully.").arg(userName).arg(userId));
        } else {
            // This might happen if the ID was in the table but somehow not in FaceIndex's map anymore
            // or if FaceIndex::deleteUser itself had an issue (e.g., HNSW internal problem if not caught there)
            QMessageBox::warning(this, "Delete User", QString("Failed to delete user '%1' (ID: %2). User may have already been removed or an error occurred.").arg(userName).arg(userId));
            populateUserTable(); // Refresh table even on failure to ensure consistency
        }
    }
}

void MainWindow::onEditUserNameClicked()
{
    if (!faceIndex) {
        QMessageBox::critical(this, "Error", "Face index not available.");
        return;
    }

    if (userTableWidget->selectionModel()->selectedRows().isEmpty()) {
        QMessageBox::information(this, "Edit User Name", "Please select a user from the list to edit.");
        return;
    }

    int selectedRow = userTableWidget->selectionModel()->selectedRows().first().row();
    QTableWidgetItem *idItem = userTableWidget->item(selectedRow, 0);
    QTableWidgetItem *nameItem = userTableWidget->item(selectedRow, 1);

    if (!idItem || !nameItem) {
        QMessageBox::warning(this, "Edit User Name", "Could not retrieve user details from selection.");
        return;
    }

    QString currentName = nameItem->text();
    bool ok_id;
    size_t userId = idItem->text().toULongLong(&ok_id);

    if (!ok_id) {
        QMessageBox::warning(this, "Edit User Name", QString("Invalid User ID format for '%1'.").arg(currentName));
        return;
    }

    bool ok_input;
    QString newName = QInputDialog::getText(this, "Edit User Name",
                                          QString("Enter new name for %1 (ID: %2):").arg(currentName).arg(userId),
                                          QLineEdit::Normal, currentName, &ok_input);

    if (ok_input && !newName.trimmed().isEmpty()) {
        if (newName.trimmed() == currentName) {
            QMessageBox::information(this, "Edit User Name", "Name not changed.");
            return;
        }

        bool updated = faceIndex->updateUserName(userId, newName.trimmed().toStdString());
        if (updated) {
            faceIndex->saveToDisk(m_appConfig.faceDatabasePath); // Persist the change
            populateUserTable(); // Refresh the table
            QMessageBox::information(this, "Edit User Name", QString("User name for ID %1 updated from '%2' to '%3'.").arg(userId).arg(currentName).arg(newName.trimmed()));
        } else {
            // This might happen if the ID was in table but removed from FaceIndex map concurrently
            QMessageBox::warning(this, "Edit User Name", QString("Failed to update name for user ID %1. User may no longer exist.").arg(userId));
            populateUserTable(); // Refresh table to ensure consistency
        }
    } else if (ok_input && newName.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Edit User Name", "User name cannot be empty.");
    }
    // If !ok_input (user pressed Cancel), do nothing.
}

void MainWindow::populateAttendanceTable()
{
    attendanceTableWidget->setRowCount(0); // Clear existing rows
    attendanceTableWidget->setSortingEnabled(false); // Disable sorting during population

    QFile logFile(QString::fromStdString(m_appConfig.attendanceLogPath));
    if (!logFile.exists()) {
        qWarning() << "Attendance log file not found:" << QString::fromStdString(m_appConfig.attendanceLogPath);
        attendanceTableWidget->setSortingEnabled(true);
        return;
    }

    if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open attendance log file for reading:" << QString::fromStdString(m_appConfig.attendanceLogPath);
        attendanceTableWidget->setSortingEnabled(true);
        return;
    }

    QTextStream in(&logFile);
    bool isFirstLine = true;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (isFirstLine) { // Skip header
            isFirstLine = false;
            // Optionally validate header: if (line != "Timestamp,UserID,UserName") { /* warn */ }
            continue;
        }

        QStringList columns = line.split(',');
        if (columns.size() == 3) {
            int row = attendanceTableWidget->rowCount();
            attendanceTableWidget->insertRow(row);
            attendanceTableWidget->setItem(row, 0, new QTableWidgetItem(columns[0])); // Timestamp
            attendanceTableWidget->setItem(row, 1, new QTableWidgetItem(columns[1])); // User ID
            attendanceTableWidget->setItem(row, 2, new QTableWidgetItem(columns[2])); // User Name
        } else if (!line.trimmed().isEmpty()) {
            qWarning() << "Skipping malformed line in attendance log:" << line;
        }
    }
    logFile.close();

    attendanceTableWidget->resizeColumnsToContents();
    attendanceTableWidget->setSortingEnabled(true); // Re-enable sorting
}
