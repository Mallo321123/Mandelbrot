#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib> // Für std::stoi und std::stod
#include <chrono>
#include "mandelbrot.hpp"

int main(int argc, char **argv)
{
    // Standardwerte für die Argumente
    int width = 8000;
    int height = 6000;
    double x_min = -2.0, x_max = 1.0;
    double y_min = -1.5, y_max = 1.5;
    int max_iter = 100;
    int chunk_size = 50;
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

    // Bildmatrix initialisieren
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

    // Mandelbrot berechnen
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, image);

    // Bild speichern
    std::cout << "Speichere Bild..." << std::endl;
    cv::imwrite(filename, image);
    std::cout << "Bild gespeichert als " << filename << "." << std::endl;


    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Bild genneriert in " << duration.count() << " Sekunden." << std::endl;

    return 0;
}
