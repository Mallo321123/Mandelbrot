#include "mandelbrot.hpp"
#include <cassert>
#include <iostream>

void test_mandelbrot() {
    std::complex<double> c1(-2.0, 1.0);
    std::complex<double> c2(0.0, 0.0);
    std::complex<double> c3(0.3, 0.5);

    assert(mandelbrot(c1, 100) == 0); // Sofortiges Entkommen
    assert(mandelbrot(c2, 100) == 100); // Maximaler Iterationswert
    assert(mandelbrot(c3, 100) > 0); // Innerhalb der Grenze

    std::cout << "Alle Tests bestanden!" << std::endl;
}

int main() {
    test_mandelbrot();
    return 0;
}