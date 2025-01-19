#include <iostream>
#include <opencv2/opencv.hpp>
#include "mandelbrot.hpp"

int main() {
    int width = 8000;  // Beispielgröße verringert
    int height = 6000;
    double x_min = -2.0, x_max = 1.0;
    double y_min = -1.5, y_max = 1.5;
    int max_iter = 100;
    int chunk_size = 100;
    int num_workers = 15;

    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;

    // Speicherort für die Chunks
    std::string output_dir = "chunks";
    std::system(("mkdir -p " + output_dir).c_str());

    // Berechnung und Speicherung der Chunks
    generate_mandelbrot(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, output_dir);

    std::cout << "Bild generiert. Chunks sind im Verzeichnis '" << output_dir << "' gespeichert." << std::endl;

    return 0;
}
