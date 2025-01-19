#ifndef MANDELBROT_HPP
#define MANDELBROT_HPP

#include <opencv2/opencv.hpp>

int mandelbrot(std::complex<double> c, int max_iter);
//void compute_chunk(int y_start, int y_end, int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, cv::Mat& image);
void generate_mandelbrot(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, const std::string& output_dir);

#endif // MANDELBROT_HPP