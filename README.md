# Pong

Ein Pong-Klon in C++17 mit [raylib](https://www.raylib.com/). Das ganze Spiel steckt in `main.cpp`.

## Features

- Einzelspieler gegen den Computer oder Zwei-Spieler-Modus
- Langsam animierter, kontrastarmer Plasma-Hintergrund
- Sound ohne Audiodateien (die Töne werden im Code erzeugt)
- Pause-Modus und Vollbild
- Highscore: längster Ballwechsel, wird in `highscore.txt` gespeichert
- Das Spiel gewinnt, wer zuerst 7 Punkte hat

## Steuerung

| Taste | Aktion |
|---|---|
| `1` / `2` | Im Menü: gegen Computer / Zwei Spieler |
| `W` / `S` | Linker Schläger |
| `Pfeil hoch` / `Pfeil runter` | Rechter Schläger (Zwei-Spieler-Modus) |
| `P` | Pause an/aus |
| `F11` / `Alt+Enter` | Vollbild an/aus |
| `Enter` | Nach Spielende zurück zum Menü |
| `Esc` | Vollbild verlassen, sonst Spiel beenden |

## Bauen unter Linux

Benötigt werden ein C++-Compiler, CMake (und Make oder Ninja). Ist raylib (>= 5.0) installiert, wird es verwendet. Sonst lädt CMake beim ersten Konfigurieren raylib 5.5 herunter (Internetverbindung nötig, dauert etwas).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/pong
```

## Windows-EXE unter Linux bauen (MXE)

Die Windows-EXE wird mit [MXE](https://mxe.cc/) (M cross environment) cross-kompiliert. Sie ist statisch gelinkt, braucht also keine zusätzlichen DLLs, und öffnet kein Konsolenfenster.

### Einmalig: MXE einrichten

MXE selbst braucht einige Build-Werkzeuge, siehe die [Anforderungen](https://mxe.cc/#requirements). Dann:

```
git clone https://github.com/mxe/mxe.git ~/mxe
cd ~/mxe
make cc cmake MXE_TARGETS=x86_64-w64-mingw32.static
```

Das baut den Cross-Compiler und CMake für das Ziel `x86_64-w64-mingw32.static` (64-Bit Windows, statische Bibliotheken). Das dauert eine Weile. raylib muss nicht installiert werden, es wird beim Konfigurieren automatisch geholt.

### Projekt bauen

```
export PATH=$HOME/mxe/usr/bin:$PATH
x86_64-w64-mingw32.static-cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
```

Das Ergebnis ist `build-win/pong.exe`. Diese Datei lässt sich auf einen Windows-Rechner kopieren und direkt starten.

Hinweise:

- Der Wrapper `x86_64-w64-mingw32.static-cmake` setzt Compiler und Toolchain von MXE. Ein normales `cmake` mit demselben Befehl würde die Linux-Version bauen.
- Für 32-Bit Windows entsprechend das Ziel `i686-w64-mingw32.static` verwenden.
- Der Ordner `build-win` darf nicht mit dem Linux-Ordner `build` verwechselt oder gemischt werden.

## Highscore

Der Rekord liegt als einfache Zahl in `highscore.txt` im Ordner der ausführbaren Datei (`build/` bzw. neben der `pong.exe`). Zum Zurücksetzen die Datei löschen.
