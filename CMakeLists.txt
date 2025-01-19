cmake_minimum_required(VERSION 3.10)
project(MandelbrotProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(mandelbrot
    src/main.cpp
    src/mandelbrot.cpp
)

target_include_directories(mandelbrot PRIVATE include)

find_package(OpenCV REQUIRED)
target_link_libraries(mandelbrot ${OpenCV_LIBS})

add_executable(test_mandelbrot tests/test_mandelbrot.cpp src/mandelbrot.cpp)
target_include_directories(test_mandelbrot PRIVATE include)
target_link_libraries(test_mandelbrot ${OpenCV_LIBS})