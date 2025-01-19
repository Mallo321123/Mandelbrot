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

void chunk_limited(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int chunk_start, int chunk_end, std::string out_path, bool silent)
{
    std::cout << "Berechne nur Chunks von " << chunk_start << " bis " << chunk_end << std::endl;
    std::cout << "Speichere Chunks in: " << out_path << std::endl;

    fs::create_directory(out_path);
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_limited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, chunk_start, chunk_end, out_path, silent);
}

void chunk_unlimited(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, std::string temp_dir, std::string filename, bool silent)
{
    std::cout << "Dateiname: " << filename << std::endl;

    fs::create_directory(temp_dir);

    // Mandelbrot berechnen
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_chunked(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, temp_dir, silent);

    std::cout << "Füge Chunks zusammen und speichere Bild..." << std::endl;
    write_image_chunked(filename, width, height, chunk_size, temp_dir);

    fs::remove_all(temp_dir);
}

void chunk_intervall(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int intervall, std::string out_path, bool silent)
{
    std::cout << "Berechne Chunks in Intervallen von " << intervall << std::endl;
    std::cout << "Speichere Chunks in: " << out_path << std::endl;

    fs::create_directory(out_path);
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, intervall, out_path, silent);
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
    int num_workers = 3;
    std::string filename = "mandelbrot.png";
    int chunk_start = -1;
    int chunk_end = -1;
    std::string out_path = "chunks";
    bool silent = false;
    int intervall = -1;

    // Überprüfen, ob Argumente übergeben wurden
    if (argc > 1)
    {
        // Argumente einlesen
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if ((arg == "--width" || arg == "-w") && i + 1 < argc)
            {
                width = std::stoi(argv[++i]);
            }
            else if ((arg == "--height" || arg == "-h") && i + 1 < argc)
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
            else if ((arg == "--num_workers" || arg == "-w") && i + 1 < argc)
            {
                num_workers = std::stoi(argv[++i]);
            }
            else if (arg == "--filename" && i + 1 < argc)
            {
                filename = std::string(argv[++i]);
            }
            else if (arg == "--chunk_start" && i + 1 < argc)
            {
                chunk_start = std::stoi(argv[++i]);
            }
            else if (arg == "--chunk_end" && i + 1 < argc)
            {
                chunk_end = std::stoi(argv[++i]);
            }
            else if ((arg == "--out_path" || arg == "-o") && i + 1 < argc)
            {
                out_path = std::string(argv[++i]);
            }
            else if (arg == "--silent" || arg == "-s")
            {
                silent = true;
            }
            else if (arg == "--intervall" && i + 1 < argc)
            {
                intervall = std::stoi(argv[++i]);
            }
            else if (arg == "--help")
            {
                std::cout << "Verwendung: " << argv[0] << " [OPTIONEN]" << std::endl;
                std::cout << "Optionen:" << std::endl;
                std::cout << "  --help, -h         Zeige diese Hilfe an" << std::endl;
                std::cout << "  --silent, -s       Keine Vortschritts Ausgabe auf der Konsole" << std::endl;
                std::cout << "  Bild Optionen:" << std::endl;
                std::cout << "    --width N, -w N           Breite des Bildes (Standard: 8000) (Es sollte das seitenverhältniss eingehalten werden)" << std::endl;
                std::cout << "    --height N, -h N          Höhe des Bildes (Standard: 6000)" << std::endl;
                std::cout << "    --x_min N                 Minimum des x-Bereichs (Standard: -2.0)" << std::endl;
                std::cout << "    --x_max N                 Maximum des x-Bereichs (Standard: 1.0)" << std::endl;
                std::cout << "    --y_min N                 Minimum des y-Bereichs (Standard: -1.5)" << std::endl;
                std::cout << "    --y_max N                 Maximum des y-Bereichs (Standard: 1.5)" << std::endl;
                std::cout << "    --max_iter N              Maximale Anzahl an Iterationen (Standard: 100)" << std::endl;
                std::cout << "  Compute optionen:" << std::endl;
                std::cout << "    --chunk_size N            Größe der Chunks (Standard: 100)" << std::endl;
                std::cout << "    --num_workers N, -w N     Anzahl der Worker (Standard: 3)" << std::endl;
                std::cout << "    --filename STR, -f STR    Dateiname des Bildes (Standard: mandelbrot.png)" << std::endl;
                std::cout << "  Sonstige Optionen:" << std::endl;
                std::cout << "    --intervall N             Intervall der Chunks (Standard: Off)" << std::endl;
                std::cout << "    --chunk_start N           Startindex des Chunks (Standard: Off)" << std::endl;
                std::cout << "    --chunk_end N             Endindex des Chunks (Standard: Off)" << std::endl;
                std::cout << "    --out_path STR, -o STR    Pfad zum Speichern der Chunks (Standard: chunks)" << std::endl;
                return 0;
            }
            else
            {
                std::cerr << "Unbekanntes Argument: " << arg << std::endl;
                return 1;
            }
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "Bildgröße: " << width << "x" << height << std::endl;
    std::cout << "Mandelbrot-Bereich: [" << x_min << ", " << x_max << "], [" << y_min << ", " << y_max << "]" << std::endl;
    std::cout << "Maximale Iterationen: " << max_iter << std::endl;
    std::cout << "Chunk-Größe: " << chunk_size << std::endl;
    std::cout << "Anzahl der Worker: " << num_workers << std::endl;

    if (chunk_start != -1 && chunk_end != -1)
    {
        chunk_limited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, chunk_start, chunk_end, out_path, silent);
    }
    else if (chunk_start != -1 || chunk_end != -1)
    {
        std::cerr << "Fehler: chunk_start und chunk_end müssen beide gesetzt sein." << std::endl;
        return 1;
    }
    else if (intervall != -1)
    {
        if (intervall <= 0)
        {
            std::cerr << "Fehler: Das Intervall muss größer als 0 sein." << std::endl;
            return 1;
        }
        else
        {
            std::cout << "Intervall: " << intervall << std::endl;
            chunk_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, intervall, out_path, silent);
        }
    }
    else
    {
        chunk_unlimited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, "temp_chunks", filename, silent);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Das Programm hat " << duration.count() << " Sekunden benötigt." << std::endl;

    return 0;
}