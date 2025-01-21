#!/bin/bash

dateipfad="$1"

# Prüfe Eingabeparameter
if [ -z "$dateipfad" ]; then
    echo "Verwendung: $0 <bildpfad>"
    exit 1
fi

# Prüfe ob Datei existiert
if [ ! -f "$dateipfad" ]; then
    echo "Die Datei $dateipfad existiert nicht."
    exit 1
fi

# Prüfe ob vips installiert ist
if ! command -v vips &> /dev/null; then
    echo "vips ist nicht installiert. Bitte installieren Sie es zuerst."
    exit 1
fi

# Stoppe und entferne existierenden Container
if docker ps -a | grep -q deepzoom-viewer; then
    echo "Stoppe und entferne alten Container..."
    docker stop deepzoom-viewer
    docker rm deepzoom-viewer
fi

echo "Baue deepzoom Bild..."
rm -rf viewer/deepzoom
mkdir -p viewer/deepzoom

if ! vips dzsave "$dateipfad" viewer/deepzoom/; then
    echo "Fehler beim Erstellen des DeepZoom-Bildes!"
    exit 1
fi

echo "Starte webserver..."
docker build -t deepzoom-viewer viewer/
docker run -d -p 8080:80 --name deepzoom-viewer deepzoom-viewer

echo "Viewer ist unter http://localhost:8080 verfügbar"