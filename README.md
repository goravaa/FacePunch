# FacePunch


## ⚡️ Quick Setup

### 1. **Download the BlazeFace Model**
- Download the ONNX face detection model from [Hugging Face](https://huggingface.co/garavv/blazeface-onnx/resolve/main/blaze.onnx)
- Place it in the following directory (create it if it doesn't exist):
    ```
    assets/models/blaze.onnx
    ```

### 2. **Download ONNX Runtime for Windows**
- Go to the [ONNX Runtime Releases page](https://github.com/microsoft/onnxruntime/releases)
- Download the version **I used for this project**:  
  [onnxruntime-win-x64-1.22.0.zip](https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-win-x64-1.22.0.zip)
- Extract the contents so that these files exist in your project:
    ```
    libs/onnxruntime/include/       # All .h header files from onnxruntime
    libs/onnxruntime/lib/           # All .lib, .dll files from onnxruntime
    ```


### 3. Install Qt 6.9.0 (Required for Development)

This project is built using the Qt 6 framework.  
You must have Qt 6.9.0 (with Qt Widgets, Qt Multimedia) installed to build and run the application from source.
 (Make sure you select MinGW 64-bit or MSVC according to your compiler.)
- [Download Qt 6.9.0 here](https://www.qt.io/download-qt-installer)
- During install, select the **MinGW 64-bit** kit (or **MSVC** if you use Visual Studio).
- Make sure your environment is set up so `cmake` can find Qt 6. (Usually this is handled automatically by the Qt Creator IDE.)
