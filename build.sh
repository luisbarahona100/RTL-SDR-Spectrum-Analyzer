#!/bin/bash
# =============================================================
# RTL-SDR Spectrum Analyzer — Build script for Ubuntu 22.04
# =============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║         RTL-SDR Spectrum Analyzer — Build Script         ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# --- Install dependencies ---
echo "[1/4] Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
    librtlsdr-dev \
    libfftw3-dev \
    libfftw3-single3 \
    libasound2-dev \
    qt6-base-dev \
    qt6-base-private-dev \
    libqt6opengl6-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev

echo "[2/4] Dependencies installed ✓"

# --- Configure with CMake ---
echo "[3/4] Configuring with CMake..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
/usr/bin/cmake "$SCRIPT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "[4/4] Building..."
make -j$(nproc)

echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║               ✅  Build successful!                      ║"
echo "║                                                          ║"
echo "║   Run: ./build/RTLSpectrum                              ║"
echo "║                                                          ║"
echo "║   Tips:                                                  ║"
echo "║   • Make sure RTL-SDR is plugged into USB               ║"
echo "║   • If permission denied: sudo adduser $USER plugdev     ║"
echo "║     then log out and back in                            ║"
echo "║   • Or run with: sudo ./build/RTLSpectrum               ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
