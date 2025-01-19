#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <png.h>
#include "mandelbrot.hpp"

namespace fs = std::filesystem;

void write_image_chunked(const std::string &filename, int width, int height, int chunk_size, const std::string &temp_dir)
{
    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp)
    {
        std::cerr << "Fehler: Konnte Datei nicht öffnen." << std::endl;
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);

    if (setjmp(png_jmpbuf(png)))
    {
        std::cerr << "Fehler beim Schreiben." << std::endl;
        fclose(fp);
        png_destroy_write_struct(&png, &info);
        return;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (int chunk_idx = 0; chunk_idx < (height + chunk_size - 1) / chunk_size; ++chunk_idx)
    {
        std::string chunk_filename = temp_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
        cv::Mat chunk = cv::imread(chunk_filename);

        if (chunk.empty())
        {
            std::cerr << "Fehler: Konnte Chunk nicht laden: " << chunk_filename << std::endl;
            continue;
        }

        // Konvertiere das Chunk-Bild von BGR nach RGB
        cv::Mat chunk_rgb;
        cv::cvtColor(chunk, chunk_rgb, cv::COLOR_RGB2BGR);

        // Schreibe die Zeilen dieses Chunks in die PNG-Datei
        for (int y = 0; y < chunk_rgb.rows; ++y)
        {
            png_bytep row = chunk_rgb.ptr<png_byte>(y);
            png_write_row(png, row);
        }
    }

    png_write_end(png, nullptr);
    fclose(fp);
    png_destroy_write_struct(&png, &info);
}

int main(int argc, char **argv)
{
    // Standardwerte für die Argumente
    int width = 8000;
    int height = 6000;
    double x_min = -2.0, x_max = 1.0;
    double y_min = -1.5, y_max = 1.5;
    int max_iter = 100;
    int chunk_size = 100;
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

    std::string temp_dir = "temp_chunks";
    fs::create_directory(temp_dir);

    // Mandelbrot berechnen
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_chunked(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, temp_dir);

    std::cout << "Füge Chunks zusammen und speichere Bild..." << std::endl;
    write_image_chunked(filename, width, height, chunk_size, temp_dir);

    fs::remove_all(temp_dir);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Das Programm hat " << duration.count() << " Sekunden benötigt." << std::endl;

    return 0;
}