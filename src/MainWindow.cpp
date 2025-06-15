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
    const double ry_tgt = 48.0;

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

    // Calculate distance between eyes
    double dist_src = std::sqrt(dx_src * dx_src + dy_src * dy_src);
    const double dist_tgt = rx_tgt - lx_tgt; // 52.0

    // Validate eye distance
    if (dist_src < 1e-6) {
        qWarning() << "Eye distance too small, falling back to simple crop";
        return cropFace(sourceImage, detectedFace);
    }

    // Calculate scale and angle
    double scale = dist_tgt / dist_src;
    double angle_src = std::atan2(dy_src, dx_src);
    double rotation_angle_rad = -angle_src;

    // Create transform
    QTransform transform;
    transform.translate(mid_x_src, mid_y_src);
    transform.rotateRadians(rotation_angle_rad);
    transform.scale(scale, scale);
    transform.translate(-mid_x_src, -mid_y_src);

    // Create output image
    QImage aligned_image(112, 112, sourceImage.format());
    aligned_image.fill(Qt::black);

    // Draw transformed image
    QPainter painter(&aligned_image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setTransform(transform);
    
    // Calculate source rectangle to draw
    double face_width = detectedFace.x2 - detectedFace.x1;
    double face_height = detectedFace.y2 - detectedFace.y1;
    double margin = std::max(face_width, face_height) * 0.5; // 50% margin
    
    QRectF source_rect(
        detectedFace.x1 - margin,
        detectedFace.y1 - margin,
        face_width + 2 * margin,
        face_height + 2 * margin
    );
    
    painter.drawImage(QRectF(0, 0, 112, 112), sourceImage, source_rect);
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
    // Take a snapshot of the current frame
    if (lastFrame.isNull()) {
        QMessageBox::critical(this, "Error", "No frame available for registration. Please ensure the camera is working.");
        return;
    }

    // Run detection on the current frame
    std::vector<FaceDetection> faces = detector->detect(lastFrame);
    
    // Filter for faces with good confidence
    std::vector<FaceDetection> good_faces;
    for (const auto& f : faces) {
        if (f.confidence >= m_appConfig.confThresh) {
            good_faces.push_back(f);
        }
    }

    if (good_faces.empty()) {
        QMessageBox::warning(this, "Registration Error", "No face detected in the current frame. Please ensure a face is clearly visible.");
        return;
    }

    if (good_faces.size() > 1) {
        QMessageBox::warning(this, "Registration Error", "Multiple faces detected. Please ensure only one face is in view.");
        return;
    }

    // We have exactly one good face
    const FaceDetection& face_to_register = good_faces[0];

    // Ask for the name
    bool ok;
    QString name = QInputDialog::getText(this, "Register User", "Enter Name for this face:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // Try to align the face
    QImage aligned_image = alignFace(lastFrame, face_to_register);
    if (aligned_image.isNull()) {
        QMessageBox::critical(this, "Error", "Failed to align face for registration. Please try again.");
        return;
    }

    // Get embedding and add to index
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
    lastFrame = image;
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
            SearchResult search_result = faceIndex->search(emb, m_appConfig.similarityThreshold);

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
