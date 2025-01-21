#pragma once
#include <iostream>
#include <string>
#include <mutex>
#include <iomanip>
#include <vector>
#include <map>

class ProgressManager {
private:
    static ProgressManager& getInstance() {
        static ProgressManager instance;
        return instance;
    }
    
    std::mutex mtx;
    std::map<int, std::pair<std::string, float>> progress_bars;
    int next_id = 0;

public:
    static int createProgressBar(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(getInstance().mtx);
        int id = getInstance().next_id++;
        getInstance().progress_bars[id] = {prefix, 0.0f};
        return id;
    }

    static void updateProgress(int id, float progress) {
        std::lock_guard<std::mutex> lock(getInstance().mtx);
        getInstance().progress_bars[id].second = progress;
        getInstance().redrawAll();
    }

private:
    void redrawAll() {
        // Lösche vorherige Ausgaben
        for (size_t i = 0; i < progress_bars.size(); ++i) {
            std::cout << "\033[A\033[2K";  // Cursor hoch und Zeile löschen
        }

        // Zeichne alle Fortschrittsbalken neu
        for (const auto& [id, bar] : progress_bars) {
            drawBar(bar.first, bar.second);
        }
    }

    void drawBar(const std::string& prefix, float progress) {
        const int width = 50;
        int filled = static_cast<int>(width * progress);

        std::cout << "\r" << prefix << " [";
        for (int i = 0; i < width; ++i) {
            if (i < filled) std::cout << "=";
            else if (i == filled) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) 
                 << (progress * 100.0) << "%" << std::endl;
    }
};

class ProgressBar {
private:
    std::mutex mtx;
    int total;
    std::atomic<int> current{0};
    std::string prefix;
    const int width = 50;

public:
    ProgressBar(int total, std::string prefix = "") 
        : total(total), prefix(prefix) {}

    void update(int value) {
        std::lock_guard<std::mutex> lock(mtx);
        current = std::min(value, total);
        display();
    }

    void increment() {
        int curr = ++current;
        if (curr % 10 == 0) { // Aktualisiere nur alle 10 Schritte
            std::lock_guard<std::mutex> lock(mtx);
            display();
        }
    }

private:
    void display() {
        float progress = static_cast<float>(current) / total;
        int filled = static_cast<int>(width * progress);

        std::cout << "\r" << prefix << " [";
        for (int i = 0; i < width; ++i) {
            if (i < filled) std::cout << "=";
            else if (i == filled) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) 
                 << (progress * 100.0) << "% "
                 << "(" << current << "/" << total << ")" << std::flush;
    }
};
