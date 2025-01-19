#ifndef MANDELBROT_HPP
#define MANDELBROT_HPP

#include <opencv2/opencv.hpp>

// Berechnung der Mandelbrot-Menge für einen Punkt
int mandelbrot(std::complex<double> c, int max_iter);

// Berechnung eines Chunk-Bereichs
void compute_chunk(int y_start, int y_end, int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, cv::Mat& image, int chunk_idx);

// Generiere die Mandelbrot-Menge und speichere die Bilddaten
void generate_mandelbrot(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, cv::Mat& image);

#endif // MANDELBROT_HPP
