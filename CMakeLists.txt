cmake_minimum_required(VERSION 3.10)
project(MandelbrotProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)



add_executable(mandelbrot
    src/main.cpp
    src/mandelbrot.cpp
    src/combine.cpp
)

target_include_directories(mandelbrot PRIVATE include)

find_package(OpenCV REQUIRED)
target_link_libraries(mandelbrot ${OpenCV_LIBS})
find_package(PNG REQUIRED)
target_link_libraries(mandelbrot PNG::PNG)

target_compile_options(mandelbrot PRIVATE -msse2)
