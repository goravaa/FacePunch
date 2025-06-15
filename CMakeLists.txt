cmake_minimum_required(VERSION 3.16)
project(FacePunch)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 Widgets and Multimedia
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

# Include directories

include_directories(${CMAKE_SOURCE_DIR}/libs/onnxruntime/include)
include_directories(${CMAKE_SOURCE_DIR}/include/hnswlib)

# Add executable (ONLY .cpp/.ui files, NOT headers!)
add_executable(FacePunch
    src/main.cpp
    src/MainWindow.cpp
    src/FaceDetector.cpp
    src/config.cpp
    src/MainWindow.ui
    src/FaceEmbedder.cpp
    src/FaceIndex.cpp
    include/MainWindow.hpp
)

# Link Qt6 Widgets and Multimedia
target_link_libraries(FacePunch
    PRIVATE
      Qt6::Widgets
      Qt6::Multimedia
      Qt6::MultimediaWidgets
)

target_include_directories(FacePunch PRIVATE include/)

# Link ONNX Runtime library
target_link_libraries(FacePunch PRIVATE ${CMAKE_SOURCE_DIR}/libs/onnxruntime/lib/onnxruntime.lib)

# Ensure the library directory is in the linker path
target_link_directories(FacePunch PRIVATE ${CMAKE_SOURCE_DIR}/libs/onnxruntime/lib)

add_custom_command(TARGET FacePunch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:FacePunch>/assets"
)

# Copy ONNX Runtime DLL after build (Windows only)
if(WIN32)
    add_custom_command(TARGET FacePunch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/libs/onnxruntime/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:FacePunch>"
    )
endif()
