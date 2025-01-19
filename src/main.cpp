#include <iostream>
#include <opencv2/opencv.hpp>
#include "mandelbrot.hpp"

int main() {
    int width = 8000;
    int height = 6000;
    double x_min = -2.0, x_max = 1.0;
    double y_min = -1.5, y_max = 1.5;
    int max_iter = 100;
    int chunk_size = 50;
    int num_workers = 15;

    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;

    // Bildmatrix initialisieren
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

    generate_mandelbrot(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, image);

    std::cout << "Speichere Bild..." << std::endl;
    cv::imwrite("mandelbrot_large_parallel.png", image);
    std::cout << "Bild gespeichert als 'mandelbrot_large_parallel.png'." << std::endl;

    return 0;
}