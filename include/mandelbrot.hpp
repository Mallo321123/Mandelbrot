#ifndef MANDELBROT_HPP
#define MANDELBROT_HPP

#include <opencv2/opencv.hpp>

int mandelbrot(std::complex<double> c, int max_iter);
void compute_chunk(int y_start, int y_end, int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, cv::Mat &image, int chunk_idx, int num_chunks, bool silent);
void generate_mandelbrot_chunked(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, std::string temp_dir, bool silent);
void generate_mandelbrot_limited(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, int chunk_start, int chunk_end, std::string out_dir, bool silent);
void generate_mandelbrot_intervall(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int intervall, std::string out_path, bool silent);

#endif // MANDELBROT_HPP
