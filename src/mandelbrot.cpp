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

inline int mandelbrot(double cr, double ci, int max_iter) {
    /*
    This function basicly does this:

    double zr = 0.0, zi = 0.0;
    for (int n = 0; n < max_iter; ++n)
    {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0)
        {
            return n;
        }
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }
    
    */

    int result;
    const double four = 4.0;
    __asm__ volatile(
        "movsd %1, %%xmm0\n\t"          // cr -> xmm0
        "movsd %2, %%xmm1\n\t"          // ci -> xmm1
        "xorpd %%xmm2, %%xmm2\n\t"      // zr = 0
        "xorpd %%xmm3, %%xmm3\n\t"      // zi = 0
        "movsd %3, %%xmm7\n\t"          // Grenzwert 4.0
        "xor %%ecx, %%ecx\n\t"          // Iterator n = 0
        
        "1:\n\t"                        // Schleifenstart
        "movapd %%xmm2, %%xmm4\n\t"     // zr -> xmm4
        "movapd %%xmm3, %%xmm5\n\t"     // zi -> xmm5
        "mulsd %%xmm4, %%xmm4\n\t"      // zr^2
        "mulsd %%xmm5, %%xmm5\n\t"      // zi^2
        "movapd %%xmm4, %%xmm6\n\t"
        "addsd %%xmm5, %%xmm6\n\t"      // zr^2 + zi^2
        "comisd %%xmm7, %%xmm6\n\t"     // vergleiche mit 4.0
        "ja 2f\n\t"                     // wenn > 4.0, springe zu 2
        
        "movapd %%xmm2, %%xmm6\n\t"
        "mulsd %%xmm3, %%xmm6\n\t"      // zr * zi
        "addsd %%xmm6, %%xmm6\n\t"      // 2 * zr * zi
        "addsd %%xmm1, %%xmm6\n\t"      // + ci
        "subsd %%xmm5, %%xmm4\n\t"      // zr^2 - zi^2
        "addsd %%xmm0, %%xmm4\n\t"      // + cr
        "movapd %%xmm4, %%xmm2\n\t"     // neues zr
        "movapd %%xmm6, %%xmm3\n\t"     // neues zi
        
        "inc %%ecx\n\t"
        "cmp %4, %%ecx\n\t"
        "jl 1b\n\t"
        
        "2:\n\t"
        "mov %%ecx, %0\n\t"             // Ergebnis speichern
        : "=r" (result)
        : "m" (cr), "m" (ci), "m" (four), "r" (max_iter)
        : "ecx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
    );
    return result;
}

void compute_chunk(int y_start, int y_end, int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, cv::Mat &image, int chunk_idx, int num_chunks, bool silent)
{
    int total_rows = y_end - y_start;
    int processed_rows = 0;
    auto last_update = std::chrono::steady_clock::now();

    for (int y = y_start; y < y_end; ++y)
    {
        uchar* rowPtr = image.ptr<uchar>(y - y_start);
        double imagY = y_min + (double(y) / height) * (y_max - y_min);
        for (int x = 0; x < width; ++x)
        {
            double realX = x_min + (double(x) / width) * (x_max - x_min);
            int value = mandelbrot(realX, imagY, max_iter);
            double normalized = (double)value / max_iter;

            // "Hot"-Colormap
            rowPtr[x * 3 + 2] = static_cast<uchar>(255 * std::min(1.0, normalized * 3.0));                  // Rot
            rowPtr[x * 3 + 1] = static_cast<uchar>(255 * std::min(1.0, std::max(0.0, normalized - 0.33) * 3.0)); // Grün
            rowPtr[x * 3 + 0] = static_cast<uchar>(255 * std::min(1.0, std::max(0.0, normalized - 0.66) * 3.0)); // Blau
        }

        processed_rows++;
        auto now = std::chrono::steady_clock::now();
        if (!silent && now - last_update > std::chrono::milliseconds(500)) {
            std::lock_guard<std::mutex> lock(progress_mutex);
            std::cout << "\rChunk " << chunk_idx << "/" << num_chunks 
                     << " - Zeilen: " << processed_rows << "/" << total_rows 
                     << " (" << (processed_rows * 100 / total_rows) << "%)"
                     << " | Gesamt: " << completed_chunks << "/" << num_chunks 
                     << " Chunks" << std::flush;
            last_update = now;
        }
    }

    {
        std::lock_guard<std::mutex> lock(progress_mutex);
        completed_chunks++;
        if (!silent)
        {
            std::cout << "\rChunk " << chunk_idx << " abgeschlossen. "
                     << "Gesamt: " << completed_chunks << "/" << num_chunks 
                     << " Chunks (" << (completed_chunks * 100 / num_chunks) << "%)" 
                     << std::endl;
        }
    }
}

void generate_mandelbrot_chunked(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, std::string temp_dir, bool silent)
{
    namespace fs = std::filesystem;
    if (!fs::exists(temp_dir))
    {
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

                                     compute_chunk(y_start, y_end, width, height, x_min, x_max, y_min, y_max, max_iter, image, chunk_idx, num_chunks, silent);

                                     {
                                         std::lock_guard<std::mutex> lock(file_mutex);
                                         std::string filename = temp_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
                                         cv::imwrite(filename, image);
                                     }
                                 } catch (const std::exception &e) {
                                     std::cerr << "Fehler im Thread " << chunk_idx << ": " << e.what() << std::endl;
                                 } catch (...) {
                                     std::cerr << "Unbekannter Fehler im Thread " << chunk_idx << std::endl;
                                 } });

        if (threads.size() == num_workers || chunk_idx == num_chunks - 1)
        {
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }
            threads.clear();
        }
    }
}

void generate_mandelbrot_limited(int width, int height, double x_min, double x_max, double y_min, double y_max, int max_iter, int chunk_size, int num_workers, int chunk_start, int chunk_end, std::string out_dir, bool silent)
{
    namespace fs = std::filesystem;
    if (!fs::exists(out_dir))
    {
        fs::create_directory(out_dir);
    }

    std::vector<std::thread> threads;
    std::mutex file_mutex;
    int num_chunks = (height + chunk_size - 1) / chunk_size;
    int num_active_chunks = chunk_end - chunk_start + 1;

    std::cout << "Anzahl der Chunks: " << num_active_chunks << std::endl;

    for (int chunk_idx = 0; chunk_idx < num_chunks; ++chunk_idx)
    {
        int y_start = chunk_idx * chunk_size;
        int y_end = std::min((chunk_idx + 1) * chunk_size, height);

        if (chunk_idx >= chunk_start && chunk_idx < chunk_end)
        {
            threads.emplace_back([=, &file_mutex]()
                                 {
                                     try {
                                         cv::Mat image(y_end - y_start, width, CV_8UC3, cv::Scalar(0, 0, 0));

                                         compute_chunk(y_start, y_end, width, height, x_min, x_max, y_min, y_max, max_iter, image, chunk_idx, num_active_chunks, silent);

                                         {
                                             std::lock_guard<std::mutex> lock(file_mutex);
                                             std::string filename = out_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
                                             cv::imwrite(filename, image);
                                         }
                                     } catch (const std::exception &e) {
                                         std::cerr << "Fehler im Thread " << chunk_idx << ": " << e.what() << std::endl;
                                     } catch (...) {
                                         std::cerr << "Unbekannter Fehler im Thread " << chunk_idx << std::endl;
                                     } });
        }

        if (threads.size() == num_workers || chunk_idx == num_chunks - 1)
        {
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }
            threads.clear();
        }
    }
}

void generate_mandelbrot_intervall(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int intervall, std::string out_path, bool silent, int offset)
{
    namespace fs = std::filesystem;
    if (!fs::exists(out_path))
    {
        fs::create_directory(out_path);
    }

    std::vector<std::thread> threads;
    std::mutex file_mutex;
    int num_chunks = (height + chunk_size - 1) / chunk_size;
    int num_active_chunks = (num_chunks + intervall - 1) / intervall;

    std::cout << "Anzahl der Chunks: " << num_active_chunks << std::endl;

    for (int chunk_idx = 0; chunk_idx < num_chunks; ++chunk_idx)
    {
        int y_start = chunk_idx * chunk_size;
        int y_end = std::min((chunk_idx + 1) * chunk_size, height);

        if ((chunk_idx + offset) % intervall == 0)
        {
            threads.emplace_back([=, &file_mutex]()
                                 {
                                     try {
                                         cv::Mat image(y_end - y_start, width, CV_8UC3, cv::Scalar(0, 0, 0));

                                         compute_chunk(y_start, y_end, width, height, x_min, x_max, y_min, y_max, max_iter, image, chunk_idx, num_active_chunks, silent);

                                         {
                                             std::lock_guard<std::mutex> lock(file_mutex);
                                             std::string filename = out_path + "/chunk_" + std::to_string(chunk_idx) + ".png";
                                             cv::imwrite(filename, image);
                                         }
                                     } catch (const std::exception &e) {
                                         std::cerr << "Fehler im Thread " << chunk_idx << ": " << e.what() << std::endl;
                                     } catch (...) {
                                         std::cerr << "Unbekannter Fehler im Thread " << chunk_idx << std::endl;
                                     } });
        }

        if (threads.size() == num_workers || chunk_idx == num_chunks - 1)
        {
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }
            threads.clear();
        }
    }
}