# Building
Easy Audio Sync builds natively on Unix and Windows, and features a cross-platform CMake build system. The following external dependencies are required:

- Qt (see [below](#qt-version-requirements) for version requirements), specifically the following modules:
  - Core
  - GUI
  - Widgets
  - LinguistTools (build only, no runtime requirement)
- FFmpeg ≥5.1, specifically the following libraries:
  - libavformat
  - libavcodec
  - libswresample
  - libavfilter
  - libavutil
- TagLib ≥1.12
- spdlog
- fmt (spdlog is typically packaged with fmt as a dependency, so you likely won't need to explicitly install this)

The source code is written in C++20, and as such requires a relatively modern compiler to build:

- On Windows, use Visual Studio 2022
- On Linux, use GCC 11 or later
- On macOS, the latest available Xcode for your machine should work

### Qt Version Requirements
The Windows and macOS versions must use Qt6. The Linux version can use either Qt5 or Qt6, with Qt5 being the default. If you want to build with Qt6 instead, pass `-DQT_VERSION=6` to `cmake`. If Qt5 is used, the minimum supported version is 5.15.

## Unix
Before starting, make sure you have the development tools Git, CMake and pkg-config installed. Then, install the dependencies listed above per your package manager. On most systems, the Qt requirements can be satisfied by packages named `qt-base` and `qt-tools` or similar, with the later being a build-only requirement that can be uninstalled later.

### Building
Clone the repo and create a build directory:

```bash
git clone https://github.com/complexlogic/EasyAudioSync.git
cd EasyAudioSync
mkdir build && cd build
```

Generate the Makefile:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Build and test the program:

```bash
make
./easyaudiosync
```

Optionally, install to your system directories:

```bash
sudo make install
```

By default, this will install the program with a prefix of `/usr/local`. If you want a different prefix, re-run the CMake generation step with `-DCMAKE_INSTALL_PREFIX=prefix`.

## Windows
The Windows toolchain consists of Visual Studio and vcpkg in addition to Git and CMake. Before starting, make sure that Visual Studio is installed with C++ core desktop features and C++ CMake tools. The free Community Edition is sufficient.

Clone the repo and create a build directory:

```bash
git clone https://github.com/complexlogic/EasyAudioSync.git
cd EasyAudioSync
mkdir build
cd build
```

Build the dependencies and generate the Visual Studio project files (this will take a long time):

```bash
cmake .. -DVCPKG=ON
```

Build and test the program:

```bash
cmake --build . --config Release
.\Release\easyaudiosync.exe
```
