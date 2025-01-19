#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <png.h>
#include "mandelbrot.hpp"

namespace fs = std::filesystem;

int main(int argc, char **argv)
{
    // Standardwerte für die Argumente
    int width = 80000;
    int height = 60000;
    double x_min = -2.0, x_max = 1.0;
    double y_min = -1.5, y_max = 1.5;
    int max_iter = 100;
    int chunk_size = 100;
    int num_workers = 15;
    std::string filename = "mandelbrot.png";

    // Überprüfen, ob Argumente übergeben wurden
    if (argc > 1)
    {
        // Argumente einlesen
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--width" && i + 1 < argc)
            {
                width = std::stoi(argv[++i]);
            }
            else if (arg == "--height" && i + 1 < argc)
            {
                height = std::stoi(argv[++i]);
            }
            else if (arg == "--x_min" && i + 1 < argc)
            {
                x_min = std::stod(argv[++i]);
            }
            else if (arg == "--x_max" && i + 1 < argc)
            {
                x_max = std::stod(argv[++i]);
            }
            else if (arg == "--y_min" && i + 1 < argc)
            {
                y_min = std::stod(argv[++i]);
            }
            else if (arg == "--y_max" && i + 1 < argc)
            {
                y_max = std::stod(argv[++i]);
            }
            else if (arg == "--max_iter" && i + 1 < argc)
            {
                max_iter = std::stoi(argv[++i]);
            }
            else if (arg == "--chunk_size" && i + 1 < argc)
            {
                chunk_size = std::stoi(argv[++i]);
            }
            else if (arg == "--num_workers" && i + 1 < argc)
            {
                num_workers = std::stoi(argv[++i]);
            }
            else if (arg == "--filename" && i + 1 < argc)
            {
                filename = std::string(argv[++i]);
            }
            else
            {
                std::cerr << "Unbekanntes Argument: " << arg << std::endl;
                return 1;
            }
        }
    }

    std::cout << "Bildgröße: " << width << "x" << height << std::endl;
    std::cout << "Mandelbrot-Bereich: [" << x_min << ", " << x_max << "], [" << y_min << ", " << y_max << "]" << std::endl;
    std::cout << "Maximale Iterationen: " << max_iter << std::endl;
    std::cout << "Chunk-Größe: " << chunk_size << std::endl;
    std::cout << "Anzahl der Worker: " << num_workers << std::endl;
    std::cout << "Dateiname: " << filename << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string temp_dir = "temp_chunks";
    fs::create_directory(temp_dir);

    // Mandelbrot berechnen
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_chunked(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, temp_dir);

    std::cout << "Füge Chunks zusammen..." << std::endl;
    // Erstelle das finale Bild
    cv::Mat final_image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

    // Berechne die Anzahl der Chunks
    int num_chunks = (height + chunk_size - 1) / chunk_size;

    for (int chunk_idx = 0; chunk_idx < num_chunks; ++chunk_idx)
    {
        // Lade den Chunk
        std::string chunk_filename = temp_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
        cv::Mat chunk = cv::imread(chunk_filename);

        if (chunk.empty())
        {
            std::cerr << "Fehler: Konnte Chunk nicht laden: " << chunk_filename << std::endl;
            continue;
        }

        // Berechne die y-Bereiche des Chunks
        int y_start = chunk_idx * chunk_size;
        int y_end = std::min(y_start + chunk.rows, height);

        // Füge den Chunk in das Zielbild ein
        chunk.copyTo(final_image(cv::Rect(0, y_start, width, y_end - y_start)));
    }

    // Temporäre Dateien löschen
    fs::remove_all(temp_dir);

    // Timer stoppen
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    // Bild speichern
    std::cout << "Speichere Bild..." << std::endl;
    cv::imwrite("mandelbrot_large_parallel.png", final_image);
    std::cout << "Bild gespeichert als 'mandelbrot_large_parallel.png'." << std::endl;

    // Laufzeit ausgeben
    std::cout << "Das Programm hat " << duration.count() << " Sekunden benötigt." << std::endl;

    return 0;
}