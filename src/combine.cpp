#include <future>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <png.h>
#include <thread> // Für sleep_for
#include "progress.hpp"

class ThreadPool
{
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

public:
    explicit ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this]
                                 {
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { 
                            return stop || !tasks.empty(); 
                        });
                        if(stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                } });
    }

    template <class F>
    auto enqueue(F &&f) -> std::future<typename std::invoke_result<F>::type>
    {
        using return_type = typename std::invoke_result<F>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace([task]()
                          { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }
};

void write_image_chunked(const std::string &filename, int width, int height, int chunk_size, const std::string &temp_dir, int threads)
{
    const int total_chunks = (height + chunk_size - 1) / chunk_size;
    const int chunks_per_temp = 10;

    struct TempFileInfo
    {
        std::string filename;
        int start_chunk;
        int num_chunks;
        int start_row;
    };
    std::vector<TempFileInfo> temp_files;
    std::mutex temp_files_mutex, cout_mutex;
    std::atomic<int> processed_chunks{0};

    auto log = [&](const std::string &message)
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message << std::flush;
    };

    log("Starte Verarbeitung...\n");
    log("Gesamtanzahl Chunks: " + std::to_string(total_chunks) + "\n");

    ProgressBar chunkProgress(total_chunks, "Verarbeite Chunks");

    auto process_chunk_range = [&](int start_chunk, int end_chunk, int temp_index)
    {
        std::string temp_filename = temp_dir + "/temp_" + std::to_string(temp_index) + ".tmp";
        int start_row = start_chunk * chunk_size;
        int total_rows = 0;

        FILE *temp_fp = fopen(temp_filename.c_str(), "wb");
        if (!temp_fp)
        {
            log("Fehler: Konnte temporäre Datei nicht erstellen: " + temp_filename + "\n");
            return;
        }

        for (int i = start_chunk; i < end_chunk && i < total_chunks; ++i)
        {
            std::string chunk_file = temp_dir + "/chunk_" + std::to_string(i) + ".png";
            cv::Mat chunk = cv::imread(chunk_file, cv::IMREAD_COLOR);

            if (!chunk.empty())
            {
                fwrite(chunk.data, 1, chunk.step * chunk.rows, temp_fp);
                total_rows += chunk.rows;
                chunk.release();
                processed_chunks++;
                chunkProgress.update(processed_chunks);
            }
        }

        fclose(temp_fp);
        {
            std::lock_guard<std::mutex> lock(temp_files_mutex);
            temp_files.push_back({temp_filename,
                                  start_chunk,
                                  end_chunk - start_chunk,
                                  start_row});
        }
    };

    // Parallele Verarbeitung
    {
        ThreadPool pool(threads);
        std::vector<std::future<void>> futures;

        for (int i = 0; i < total_chunks; i += chunks_per_temp)
        {
            futures.push_back(pool.enqueue([=, &process_chunk_range]()
                                           { process_chunk_range(
                                                 i,
                                                 std::min(i + chunks_per_temp, total_chunks),
                                                 i / chunks_per_temp); }));
        }

        for (auto &future : futures)
        {
            future.wait();
        }
    }

    // Sortiere nach Startzeile
    std::sort(temp_files.begin(), temp_files.end(),
              [](const TempFileInfo &a, const TempFileInfo &b)
              {
                  return a.start_row < b.start_row;
              });

    log("\nErstelle finale PNG-Datei...\n");

    // PNG Initialisierung
    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp)
    {
        log("Fehler: Konnte finale Datei nicht öffnen.\n");
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);

    if (!png || !info || setjmp(png_jmpbuf(png)))
    {
        log("Fehler bei PNG-Initialisierung.\n");
        if (png)
            png_destroy_write_struct(&png, info ? &info : nullptr);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_bgr(png);
    png_write_info(png, info);

    // Schreibe Daten
    std::vector<uint8_t> row_buffer(width * 3);
    int total_rows = 0;

    ProgressBar writeProgress(height, "Schreibe Datei");

    for (const auto &temp_info : temp_files)
    {
        FILE *temp_fp = fopen(temp_info.filename.c_str(), "rb");
        if (!temp_fp)
            continue;

        for (int i = 0; i < temp_info.num_chunks && total_rows < height; ++i)
        {
            int rows_in_chunk = std::min(chunk_size, height - total_rows);
            for (int row = 0; row < rows_in_chunk; ++row)
            {
                if (fread(row_buffer.data(), 1, width * 3, temp_fp) == width * 3)
                {
                    png_write_row(png, row_buffer.data());
                    total_rows++;
                    writeProgress.increment();
                }
            }
        }

        fclose(temp_fp);
        std::remove(temp_info.filename.c_str());
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    log("\nVerarbeitung abgeschlossen. " + std::to_string(total_rows) +
        " von " + std::to_string(height) + " Zeilen geschrieben.\n");
}