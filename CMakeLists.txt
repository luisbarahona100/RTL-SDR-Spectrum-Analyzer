cmake_minimum_required(VERSION 3.16)
project(RTLSpectrum VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED librtlsdr)
# fftw3f (single precision) — pkg-config may call it fftw3f or not exist
# Use find_library as fallback for robustness on Ubuntu 22.04
find_library(FFTW3F_LIB NAMES fftw3f REQUIRED)
find_path(FFTW3F_INCLUDE NAMES fftw3.h REQUIRED)
set(FFTW3_LIBRARIES ${FFTW3F_LIB})
set(FFTW3_INCLUDE_DIRS ${FFTW3F_INCLUDE})

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/SpectrumWidget.cpp
    src/WaterfallWidget.cpp
    src/RTLWorker.cpp
    src/DemodWFM.cpp
    src/AudioOutput.cpp
)

set(HEADERS
    src/MainWindow.h
    src/SpectrumWidget.h
    src/WaterfallWidget.h
    src/RTLWorker.h
    src/DemodWFM.h
    src/AudioOutput.h
)

add_executable(RTLSpectrum ${SOURCES} ${HEADERS})

target_include_directories(RTLSpectrum PRIVATE
    src/
    ${RTLSDR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(RTLSpectrum PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${RTLSDR_LIBRARIES}
    ${FFTW3_LIBRARIES}
    pthread
    asound
)

target_compile_options(RTLSpectrum PRIVATE
    ${RTLSDR_CFLAGS_OTHER}
    ${FFTW3_CFLAGS_OTHER}
    -O3
    -march=native
)
