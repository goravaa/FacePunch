// src/main.cpp
#include <QApplication>
#include "MainWindow.hpp"
#include "config.h"

int main(int argc, char *argv[]) {
    AppConfig config;
    config.loadInitialConfig();

    QApplication app(argc, argv);
    MainWindow w(config); // Pass config to MainWindow
    w.show();
    return app.exec();
}
