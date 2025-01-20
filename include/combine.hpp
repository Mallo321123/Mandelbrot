#ifndef COMBINE_HPP
#define COMBINE_HPP

#include <string>

void write_image_chunked(const std::string &filename, int width, int height, int chunk_size, const std::string &temp_dir);

#endif // COMBINE_HPP