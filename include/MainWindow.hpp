// include/MainWindow.hpp
#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QImage>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QTransform> // Added for QTransform
#include <cmath>      // Added for std::atan2, std::sqrt
#include "FaceDetector.hpp"
#include "config.h"
#include "FaceEmbedder.hpp"
#include "FaceIndex.hpp"
#include <memory>
#include <QAction> // Added for QAction
#include <QMenuBar> // Added for menuBar()
#include <QDateTime> // For attendance logging
#include <QFile>     // For attendance logging
#include <QTextStream> // For attendance logging

// Forward declare SettingsDialog
class SettingsDialog;

// Forward declare Qt UI classes for User Management Tab
class QTabWidget;
class QTableWidget;
class QPushButton;
class QVBoxLayout;
class QWidget;


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
    std::unique_ptr<FaceEmbedder> embedder = nullptr;       // For embedding
    std::unique_ptr<FaceIndex> faceIndex = nullptr;         // For storing/searching
    std::vector<std::string> recentResults; // To track per-frame results
    

    void updateRecognitionUI(const std::vector<FaceDetection> &faces, QImage &image);

private slots:
    void onVideoFrame(const QVideoFrame &frame); // <-- For Qt 6 video frame callback
    void onRegisterUser();     
    QImage cropFace(const QImage &img, const FaceDetection &fd);
    QImage alignFace(const QImage &sourceImage, const FaceDetection &detectedFace); // Added for face alignment
    void openSettingsDialog(); // Slot to open settings dialog
    void populateUserTable(); // Slot to populate the user table
    void onDeleteUserClicked(); // Slot for delete user button
    void onEditUserNameClicked(); // Slot for edit user name button
    void populateAttendanceTable(); // Slot to populate the attendance table


private:
    Ui::MainWindow *ui;
    QMenu *fileMenu; // Added for File menu
    QAction *settingsAction; // Added for Settings action
    QTimer *timer;
    std::unique_ptr<FaceDetector> detector; // Changed to unique_ptr
    QImage lastFrame;
    AppConfig m_appConfig; // Added AppConfig member

    // For attendance log debouncing
    std::unordered_map<size_t, QDateTime> m_lastLogTimestamps;
    int m_attendanceLogDebounceSecs = 10; // Default, consider making this part of AppConfig later

    // UI elements for User Management Tab
    QTabWidget *mainTabWidget;
    QWidget *liveViewTab; // To hold the camera feed
    QWidget *userManagementTab;
    QVBoxLayout *userManagementLayout;
    QTableWidget *userTableWidget;
    QPushButton *refreshUserListButton;
    QPushButton *deleteUserButton; // Button to delete selected user
    QPushButton *editUserNameButton; // Button to edit selected user's name

    // UI elements for Attendance Log Tab
    QWidget *attendanceLogTab;
    QVBoxLayout *attendanceLogLayout;
    QTableWidget *attendanceTableWidget;
    QPushButton *refreshLogButton;

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
     size_t userId = 0; // Added for attendance logging
};

int frameCount = 0;
const int frameSkip = 3;  // ONNX every 3rd frame
const int cacheTTL  = 3;  // how many frames to keep a detection
std::vector<CachedFace> faceCache;

};
