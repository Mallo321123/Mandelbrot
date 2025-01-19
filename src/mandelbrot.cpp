#include "mandelbrot.hpp"
#include <complex>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <filesystem>


namespace fs = std::filesystem;
std::mutex file_mutex;

std::mutex progress_mutex;
std::atomic<int> completed_chunks(0);

int mandelbrot(std::complex<double> c, int max_iter)
{
    std::complex<double> z = 0;
    for (int n = 0; n < max_iter; ++n)
    {
        if (std::abs(z) > 2.0)
        {
            return n;
        }
        z = z * z + c;
    }
    return max_iter;
}

void compute_chunk(int y_start, int y_end, int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, cv::Mat &image, int chunk_idx, int num_chunks)
{
    for (int y = y_start; y < y_end; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            double real = x_min + (x / (double)width) * (x_max - x_min);
            double imag = y_min + (y / (double)height) * (y_max - y_min);
            std::complex<double> c(real, imag);
            int value = mandelbrot(c, max_iter);

            // Normalisieren
            double normalized = (double)value / max_iter;

            // "Hot"-Colormap - Umrechnung der Farbwerte
            cv::Vec3b color;
            // Rotkanal (höherer Wert für größere Werte)
            color[2] = static_cast<uchar>(255 * std::min(1.0, normalized * 3.0));
            // Grüner Kanal (mittelmäßig bei höheren Werten)
            color[1] = static_cast<uchar>(255 * std::min(1.0, std::max(0.0, normalized - 0.33) * 3.0));
            // Blaukanal (gering bei höheren Werten)
            color[0] = static_cast<uchar>(255 * std::min(1.0, std::max(0.0, normalized - 0.66) * 3.0));

            // Relativer y-Index für die Bildmatrix
            image.at<cv::Vec3b>(y - y_start, x) = color;
        }
    }

    {
        std::lock_guard<std::mutex> lock(progress_mutex);
        completed_chunks++;
        std::cout << "Fortschritt: " << completed_chunks << "/" << num_chunks << " Chunks abgeschlossen." << std::endl;
    }
}


void generate_mandelbrot_chunked(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, std::string temp_dir)
{
    namespace fs = std::filesystem;
    if (!fs::exists(temp_dir)) {
        fs::create_directory(temp_dir);
    }

    std::vector<std::thread> threads;
    std::mutex file_mutex;
    int num_chunks = (height + chunk_size - 1) / chunk_size;

    std::cout << "Anzahl der Chunks: " << num_chunks << std::endl;

    for (int chunk_idx = 0; chunk_idx < num_chunks; ++chunk_idx)
    {
        int y_start = chunk_idx * chunk_size;
        int y_end = std::min((chunk_idx + 1) * chunk_size, height);

        threads.emplace_back([=, &file_mutex]()
                             {
                                 try {
                                     cv::Mat image(y_end - y_start, width, CV_8UC3, cv::Scalar(0, 0, 0));

                                     compute_chunk(y_start, y_end, width, height, x_min, x_max, y_min, y_max, max_iter, image, chunk_idx, num_chunks);

                                     {
                                         std::lock_guard<std::mutex> lock(file_mutex);
                                         std::string filename = temp_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
                                         cv::imwrite(filename, image);
                                     }
                                 } catch (const std::exception &e) {
                                     std::cerr << "Fehler im Thread " << chunk_idx << ": " << e.what() << std::endl;
                                 } catch (...) {
                                     std::cerr << "Unbekannter Fehler im Thread " << chunk_idx << std::endl;
                                 }
                             });

        if (threads.size() == num_workers || chunk_idx == num_chunks - 1) {
            for (auto &t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            threads.clear();
        }
    }
}