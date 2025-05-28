cmake_minimum_required(VERSION 3.16)
project(FacePunch)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 Widgets and Multimedia
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

# Include directories
include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/libs/onnxruntime/include)

# Add executable
add_executable(FacePunch
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.hpp
    src/MainWindow.ui
    include/FaceDetector.hpp
    src/FaceDetector.cpp
    include/config.h 
    src/config.cpp
)

# Link Qt6 Widgets and Multimedia
target_link_libraries(FacePunch
    PRIVATE
      Qt6::Widgets
      Qt6::Multimedia
      Qt6::MultimediaWidgets
)

# Link ONNX Runtime library
target_link_libraries(FacePunch PRIVATE ${CMAKE_SOURCE_DIR}/libs/onnxruntime/lib/onnxruntime.lib)

# Ensure the library directory is in the linker path
target_link_directories(FacePunch PRIVATE ${CMAKE_SOURCE_DIR}/libs/onnxruntime/lib)
