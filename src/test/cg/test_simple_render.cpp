/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cg/render.h>
#include "binary/writer.h"
#include "math/matrix.h"
#include <memory>
#include "bmp.hpp"
#include <fstream>

int main() {
    auto heap = std::make_unique<futils::math::Matrix<512, 512, RGB>>();
    futils::cg::render::test::renderer.render([&](size_t x, size_t y, auto tv, auto bright, auto color) {
        y = 511 - y;
        if (bright.is_nan()) {
            (*heap)[y][x] = RGB{
                .b = color[2],
                .g = color[1],
                .r = color[0],
            };
        }
        else {
            (*heap)[y][x] = RGB{
                .b = std::uint8_t(color[2] * bright.to_float()),
                .g = std::uint8_t(color[1] * bright.to_float()),
                .r = std::uint8_t(color[0] * bright.to_float()),
            };
        }
    });
    BMPHeader hdr;
    hdr.file_header.bfType = BMP_MAGIC_NUMBER;
    hdr.info_header.biSize = 40;
    hdr.info_header.biPlanes = 1;
    hdr.info_header.biBitCount = 24;
    hdr.info_header.biCompression = BMPCompression::RGB;
    hdr.info_header.biWidth = 512;
    hdr.info_header.biHeight = 512;
    hdr.file_header.bfOffBits = hdr.fixed_header_size;
    hdr.file_header.bfSize = hdr.file_header.bfOffBits + 3 * 512 * 512;
    size_t i = 0;
    futils::byte header[hdr.fixed_header_size];
    futils::binary::writer w{header};
    hdr.encode(w);
    std::ofstream output("ignore/render.bmp", std::ios::binary);
    output.write((char*)header, sizeof(header));
    output.write((char*)heap.get(), 3 * 512 * 512);
}
