# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Pong clone in C++17 using raylib. The whole game lives in a single file, `main.cpp`. There are no tests, no linter and no asset files. UI text is in German.

## Build and run

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/pong
```

Windows EXE (cross-compiled on Linux with MXE in `~/mxe`, static triplet): `PATH=$HOME/mxe/usr/bin:$PATH x86_64-w64-mingw32.static-cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release && cmake --build build-win -j` produces `build-win/pong.exe`. It links statically and is built as a GUI app (no console window).

Linux AppImage: `APPIMAGETOOL=/path/to/appimagetool ./packaging/build-appimage.sh` produces `build/Pong-x86_64.AppImage` (desktop file and icon are in `packaging/`).

`CMakeLists.txt` first tries `find_package(raylib 5.0)`. If raylib is not installed, it downloads raylib 5.5 via FetchContent, so the first configure needs network access and takes a while. The tag is 5.5 on purpose: raylib 5.0 fails to configure with current CMake (`cmake_minimum_required` < 3.5 was removed).

## Architecture (`main.cpp`)

- Everything runs in one loop in `main()`, driven by a `State` enum (`Menu`, `Playing`, `Paused`, `GameOver`). Update logic and drawing are in the same loop iteration.
- Game constants are `constexpr` values at the top of the file (window size, paddle and ball speeds, `kWinScore`).
- Ball bounce off paddles: the outgoing angle (up to ±60°) depends on where the ball hits the paddle. Each hit speeds the ball up by 6 %, capped at `kBallMax`.
- The AI controls the right paddle in single-player mode. It tracks the ball's y position at 80 % of the player speed and only moves while the ball is travelling toward it.
- All drawing goes into an 800x600 `RenderTexture` (`canvas`), which is then scaled with letterboxing to the actual window. Game logic always uses the fixed 800x600 coordinates. Fullscreen (F11 / Alt+Enter) is true fullscreen via `ToggleFullscreenMode()`: the window is first resized to the monitor size, because raylib's `ToggleFullscreen()` requests the current window size as resolution. Borderless windowed mode was tried and does not cover KDE panels. `SetExitKey(KEY_NULL)` is set so that ESC first leaves fullscreen and only then quits.
- The background is a plasma effect. `UpdatePlasma()` fills an 80x60 pixel buffer every frame, uploads it with `UpdateTexture`, and it is drawn stretched with bilinear filtering. The colors are deliberately dark and low-contrast so the white game objects stay readable. Keep it that way when changing the palette.
- The highscore is the longest rally (consecutive paddle hits without a point), in both game modes. It is a plain integer in a text file, loaded at startup and written when a point is scored (if a new record was set) and on exit. On Windows it is `highscore.txt` next to the EXE; on Linux it is `~/.local/share/pong/highscore.txt` (`$XDG_DATA_HOME`), because the application directory is read-only inside an AppImage.
- Sound is generated procedurally. `MakeTone()` builds a square-wave `Wave` in memory and converts it with `LoadSoundFromWave`. Do not add audio files. All `Sound` objects are created at the start of `main()` and unloaded before `CloseAudioDevice()`.
