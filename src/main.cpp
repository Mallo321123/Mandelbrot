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
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    // Schreibe BGR-Daten direkt als RGB
    png_set_bgr(png);

    png_write_info(png, info);

    int total_chunks = (height + chunk_size - 1) / chunk_size;
    for (int chunk_idx = 0; chunk_idx < total_chunks; ++chunk_idx)
    {
        std::string chunk_filename = temp_dir + "/chunk_" + std::to_string(chunk_idx) + ".png";
        cv::Mat chunkBGR = cv::imread(chunk_filename, cv::IMREAD_COLOR);

        if (chunkBGR.empty())
        {
            std::cerr << "Fehler: Konnte Chunk nicht laden: " << chunk_filename << std::endl;
            continue;
        }

        for (int y = 0; y < chunkBGR.rows; ++y)
        {
            png_bytep row = chunkBGR.ptr<png_byte>(y);
            png_write_row(png, row);
        }
    }

    png_write_end(png, info);
    fclose(fp);
    png_destroy_write_struct(&png, &info);
}

void chunk_limited(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int chunk_start, int chunk_end, std::string chunk_path, bool silent)
{
    std::cout << "Berechne nur Chunks von " << chunk_start << " bis " << chunk_end << std::endl;
    std::cout << "Speichere Chunks in: " << chunk_path << std::endl;

    fs::create_directory(chunk_path);
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_limited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, chunk_start, chunk_end, chunk_path, silent);
}

void chunk_unlimited(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, std::string temp_dir, std::string filename, bool silent, bool delete_cache)
{
    std::cout << "Dateiname: " << filename << std::endl;

    fs::create_directory(temp_dir);

    // Mandelbrot berechnen
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_chunked(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, temp_dir, silent);

    std::cout << "Füge Chunks zusammen und speichere Bild..." << std::endl;
    write_image_chunked(filename, width, height, chunk_size, temp_dir);

    if (delete_cache)
    {
        std::cout << "Lösche temporäre Chunks..." << std::endl;
        fs::remove_all(temp_dir);
    }
}

void chunk_intervall(int width, int height, int x_min, int x_max, int y_min, int y_max, int max_iter, int chunk_size, int num_workers, int intervall, std::string chunk_path, bool silent, int offset)
{
    std::cout << "Berechne Chunks in Intervallen von " << intervall << std::endl;
    std::cout << "Speichere Chunks in: " << chunk_path << std::endl;

    fs::create_directory(chunk_path);
    std::cout << "Generiere Mandelbrot-Menge..." << std::endl;
    generate_mandelbrot_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers, intervall, chunk_path, silent, offset);
}

int main(int argc, char **argv)
{
    auto printParams = [&](int w, int h, double xmin, double xmax, double ymin, double ymax, int mi, int cs, int nw)
    {
        std::cout << "Bildgröße: " << w << "x" << h << std::endl;
        std::cout << "Mandelbrot-Bereich: [" << xmin << ", " << xmax << "], [" << ymin << ", " << ymax << "]" << std::endl;
        std::cout << "Maximale Iterationen: " << mi << std::endl;
        std::cout << "Chunk-Größe: " << cs << std::endl;
        std::cout << "Anzahl der Worker: " << nw << std::endl;
    };

    int width = 8000, height = 6000, max_iter = 100, chunk_size = 100, num_workers = 3;
    double x_min = -2.0, x_max = 1.0, y_min = -1.5, y_max = 1.5;
    std::string filename = "mandelbrot.png", chunk_path = "chunks";
    int chunk_start = -1, chunk_end = -1, intervall = -1, offset = 0;
    bool silent = false, fusion = false, delete_cache = false;

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
        else if ((arg == "--workers") || (arg == "-j"))
            num_workers = nextIntArg(i);
        else if (arg == "--filename")
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
        else if ((arg == "--chunk_path" || arg == "-o"))
        {
            if (++i >= argc)
            {
                std::cerr << "Fehler: fehlender Wert für --chunk_path" << std::endl;
                std::exit(1);
            }
            chunk_path = argv[i];
        }
        else if ((arg == "--silent" || arg == "-s"))
            silent = true;
        else if (arg == "--fusion")
            fusion = true;
        else if ((arg == "--delete") || (arg == "-t"))
        {
            delete_cache = true;
            std::cout << "Lösche temporäre Chunks nach dem Zusammenfügen..." << std::endl;
        }
        else if (arg == "--intervall")
            intervall = nextIntArg(i);
        else if (arg == "--offset")
            offset = nextIntArg(i);
        else if (arg == "--help")
        {
            std::cout
                << "Verwendung: " << argv[0] << " [OPTIONEN]\n"
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
                << "  --workers N, -j N  Worker (Standard: 3)\n"
                << "  --filename STR     Ausgabedatei (Standard: mandelbrot.png)\n"
                << "  --intervall N      Chunk-Intervall (Standard: aus)\n"
                << "  --offset N         Chunk-Offset (Standard: 0)\n"
                << "  --chunk_start N    Startindex (Standard: aus)\n"
                << "  --chunk_end N      Endindex (Standard: aus)\n"
                << "  --fusion           Füge Chunks zusammen und speichere Bild\n"
                << "  --delete, -t       Lösche temporäre Chunks nach dem Zusammenfügen\n"
                << "  --chunk_path, -o STR Speicherpfad (Standard: chunks)\n";
            return 0;
        }
        else
        {
            std::cerr << "Unbekanntes Argument: " << arg << std::endl;
            return 1;
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    if (chunk_start != -1 && chunk_end != -1)
    {
        printParams(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers);
        chunk_limited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size,
                      num_workers, chunk_start, chunk_end, chunk_path, silent);
    }
    else if (chunk_start != -1 || chunk_end != -1)
    {
        printParams(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers);
        std::cerr << "Fehler: chunk_start und chunk_end müssen beide gesetzt sein." << std::endl;
        return 1;
    }
    else if (intervall != -1)
    {
        if (intervall <= 0)
        {
            printParams(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers);
            std::cerr << "Fehler: Das Intervall muss größer als 0 sein." << std::endl;
            return 1;
        }
        else
        {
            std::cout << "Intervall: " << intervall << std::endl;
            std::cout << "Offset: " << offset << std::endl;
            chunk_intervall(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size,
                            num_workers, intervall, chunk_path, silent, offset);
        }
    }
    else if (fusion)
    {
        std::cout << "Bildgröße: " << width << "x" << height << std::endl;
        std::cout << "Chunk-Größe: " << chunk_size << std::endl;
        std::cout << "Füge Chunks zusammen und speichere Bild..." << std::endl;
        write_image_chunked(filename, width, height, chunk_size, chunk_path);
    }
    else
    {
        printParams(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers);
        chunk_unlimited(width, height, x_min, x_max, y_min, y_max, max_iter, chunk_size, num_workers,
                        chunk_path, filename, silent, delete_cache);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "Das Programm hat "
              << std::chrono::duration<double>(end_time - start_time).count()
              << " Sekunden benötigt." << std::endl;

    return 0;
}
