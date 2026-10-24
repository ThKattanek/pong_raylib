#!/bin/sh
# Baut build/Pong-x86_64.AppImage. Voraussetzung: appimagetool im PATH (oder per APPIMAGETOOL gesetzt).
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
rm -rf build/AppDir
mkdir -p build/AppDir/usr/bin
cp build/pong build/AppDir/usr/bin/pong
cp packaging/pong.desktop packaging/pong.png build/AppDir/
ln -s usr/bin/pong build/AppDir/AppRun
ARCH=x86_64 "${APPIMAGETOOL:-appimagetool}" --appimage-extract-and-run build/AppDir build/Pong-x86_64.AppImage
