#include <future>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <png.h>
#include <thread> // Für sleep_for

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

void write_image_chunked(const std::string &filename, int width, int height, int chunk_size, const std::string &temp_dir)
{
    const int total_chunks = (height + chunk_size - 1) / chunk_size;
    const int max_threads = std::min(2u, std::thread::hardware_concurrency());
    const int chunks_per_temp = 10; // Kleinere Gruppen für weniger Speicherverbrauch

    std::vector<std::string> temp_files;
    std::mutex temp_files_mutex, cout_mutex;
    std::atomic<int> processed_chunks{0};
    std::atomic<int> written_temp_files{0};

    auto log = [&](const std::string &message)
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message << std::flush;
    };

    log("Starte Verarbeitung...\n");
    log("Gesamtanzahl Chunks: " + std::to_string(total_chunks) + "\n");

    auto process_chunk_range = [&](int start, int end, int temp_index)
    {
        std::string temp_filename = temp_dir + "/temp_" + std::to_string(temp_index) + ".tmp";

        FILE *temp_fp = fopen(temp_filename.c_str(), "wb");
        if (!temp_fp)
        {
            log("Fehler: Konnte temporäre Datei nicht erstellen: " + temp_filename + "\n");
            return;
        }

        std::vector<uint8_t> row_buffer(width * 3);
        for (int i = start; i < end && i < total_chunks; ++i)
        {
            std::string chunk_file = temp_dir + "/chunk_" + std::to_string(i) + ".png";
            cv::Mat chunk = cv::imread(chunk_file, cv::IMREAD_COLOR);

            if (!chunk.empty())
            {
                for (int y = 0; y < chunk.rows; ++y)
                {
                    memcpy(row_buffer.data(), chunk.ptr(y), width * 3);
                    fwrite(row_buffer.data(), 1, width * 3, temp_fp);
                }
                chunk.release(); // Sofortiges Freigeben des Speichers
                processed_chunks++;

                if (processed_chunks % 5 == 0)
                {
                    log("\rVerarbeitet: " + std::to_string(processed_chunks) + "/" +
                        std::to_string(total_chunks) + " (" +
                        std::to_string(processed_chunks * 100 / total_chunks) + "%)\n");
                }
            }
            else
            {
                log("Warnung: Konnte Chunk nicht laden: " + std::to_string(i) + "\n");
            }
        }

        fclose(temp_fp);
        {
            std::lock_guard<std::mutex> lock(temp_files_mutex);
            temp_files.push_back(temp_filename);
            written_temp_files++;
        }
    };

    // Parallele Verarbeitung
    {
        ThreadPool pool(max_threads);
        std::vector<std::future<void>> futures;
        futures.reserve((total_chunks + chunks_per_temp - 1) / chunks_per_temp);

        log("Starte " + std::to_string(max_threads) + " Worker-Threads...\n");

        for (int i = 0; i < total_chunks; i += chunks_per_temp)
        {
            futures.push_back(pool.enqueue([=, &process_chunk_range]()
                                           { process_chunk_range(i, std::min(i + chunks_per_temp, total_chunks), i / chunks_per_temp); }));
        }

        for (auto &future : futures)
        {
            future.wait();
        }
    }

    log("\nErstelle finale PNG-Datei...\n");

    // PNG erstellen
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
        std::cerr << "Fehler bei PNG-Initialisierung." << std::endl;
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

    log("Füge temporäre Dateien zusammen...\n");
    std::vector<uint8_t> row_buffer(width * 3);
    int total_rows = 0;

    for (const auto &temp_file : temp_files)
    {
        FILE *temp_fp = fopen(temp_file.c_str(), "rb");
        if (temp_fp)
        {
            int rows_in_file = 0;
            while (fread(row_buffer.data(), 1, width * 3, temp_fp) == width * 3)
            {
                png_write_row(png, row_buffer.data());
                rows_in_file++;
                total_rows++;

                if (total_rows % 1000 == 0)
                {
                    log("\rZeilen geschrieben: " + std::to_string(total_rows) + "/" +
                        std::to_string(height) + " (" +
                        std::to_string(total_rows * 100 / height) + "%)\n");
                }
            }
            fclose(temp_fp);
            std::remove(temp_file.c_str());
        }
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    log("\nVerarbeitung erfolgreich abgeschlossen.\n");
    log("Gesamtanzahl verarbeiteter Zeilen: " + std::to_string(total_rows) + "\n");
}