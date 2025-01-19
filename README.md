# Mandelbrot-Projekt

Dieses Projekt berechnet die Mandelbrot-Menge und speichert das Ergebnis als Bild.

## Abhängigkeiten
- C++17 oder höher
- OpenCV

## Build-Anleitung
1. Installiere OpenCV (z. B. mit `sudo apt install libopencv-dev`).
2. Führe folgende Befehle aus:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

## Ausführen
- Mandelbrot-Berechnung:
  ```bash
  ./mandelbrot
  ```
- Tests:
  ```bash
  ./test_mandelbrot
  ```