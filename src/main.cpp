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

void chunk_intervall(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int intervall, std::string out_path, bool silent, int offset)
{
    std::cout << "Berechne Chunks in Intervallen von " << intervall << std::endl;
    std::cout << "Speichere Chunks in: " << out_path << std::endl;

    fs::create_directory(out_path);
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, intervall, out_path, silent, offset);
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
    int offset = 0;

    // Überprüfen, ob Argumente übergeben wurden
    // Kleine Helferfunktionen, um Duplikate zu vermeiden
    auto nextIntArg = [&](int &i)
    {
        if (++i >= argc)
        {
            std::cerr << "Fehler: fehlender numerischer Wert für " << argv[i - 1] << std::endl;
            std::exit(1);
        }
        return std::stoi(argv[i]);
    };

    auto nextDoubleArg = [&](int &i)
    {
        if (++i >= argc)
        {
            std::cerr << "Fehler: fehlender numerischer Wert für " << argv[i - 1] << std::endl;
            std::exit(1);
        }
        return std::stod(argv[i]);
    };

    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if ((arg == "--width" || arg == "-w"))
                width = nextIntArg(i);
            else if ((arg == "--height" || arg == "-h"))
                height = nextIntArg(i);
            else if (arg == "--x_min")
                x_min = nextDoubleArg(i);
            else if (arg == "--x_max")
                x_max = nextDoubleArg(i);
            else if (arg == "--y_min")
                y_min = nextDoubleArg(i);
            else if (arg == "--y_max")
                y_max = nextDoubleArg(i);
            else if (arg == "--max_iter")
                max_iter = nextIntArg(i);
            else if (arg == "--chunk_size")
                chunk_size = nextIntArg(i);
            else if (arg == "--num_workers")
                num_workers = nextIntArg(i);
            else if ((arg == "--filename"))
            {
                if (++i >= argc)
                {
                    std::cerr << "Fehler: fehlender Wert für --filename" << std::endl;
                    std::exit(1);
                }
                filename = argv[i];
            }
            else if (arg == "--chunk_start")
                chunk_start = nextIntArg(i);
            else if (arg == "--chunk_end")
                chunk_end = nextIntArg(i);
            else if ((arg == "--out_path" || arg == "-o"))
            {
                if (++i >= argc)
                {
                    std::cerr << "Fehler: fehlender Wert für --out_path" << std::endl;
                    std::exit(1);
                }
                out_path = argv[i];
            }
            else if ((arg == "--silent" || arg == "-s"))
                silent = true;
            else if (arg == "--intervall")
                intervall = nextIntArg(i);
            else if (arg == "--offset")
                offset = nextIntArg(i);
            else if (arg == "--help")
            {
                std::cout << "Verwendung: " << argv[0] << " [OPTIONEN]\n"
                          << "  --help, -h         Zeige diese Hilfe an\n"
                          << "  --silent, -s       Keine Fortschrittsausgabe\n"
                          << "  --width, -w N      Breite (Standard: 8000)\n"
                          << "  --height, -h N     Höhe (Standard: 6000)\n"
                          << "  --x_min N          Start des x-Bereichs (Standard: -2.0)\n"
                          << "  --x_max N          Ende des x-Bereichs (Standard: 1.0)\n"
                          << "  --y_min N          Start des y-Bereichs (Standard: -1.5)\n"
                          << "  --y_max N          Ende des y-Bereichs (Standard: 1.5)\n"
                          << "  --max_iter N       Iterationen (Standard: 100)\n"
                          << "  --chunk_size N     Chunk-Größe (Standard: 100)\n"
                          << "  --num_workers N    Worker (Standard: 3)\n"
                          << "  --filename STR     Ausgabedatei (Standard: mandelbrot.png)\n"
                          << "  --intervall N      Chunk-Intervall (Standard: aus)\n"
                          << "  --offset N         Chunk-Offset (Standard: 0)\n"
                          << "  --chunk_start N    Startindex (Standard: aus)\n"
                          << "  --chunk_end N      Endindex (Standard: aus)\n"
                          << "  --out_path, -o STR Speicherpfad für Chunks (Standard: chunks)\n";
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
            std::cout << "Offset: " << offset << std::endl;

            chunk_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, intervall, out_path, silent, offset);
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