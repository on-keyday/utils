/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/byte.h>
#include <view/iovec.h>
#include <optional>
#include <binary/writer.h>
#include <unicode/utf/retreat.h>

namespace futils::console {

    enum class WindowTileType : byte {
        BACKGROUND,
        CHAR,
        TOP_LEFT,
        BOTTOM_LEFT,
        TOP_RIGHT,
        BOTTOM_RIGHT,
        HORIZONTAL,
        VERTICAL,
        TOP,
        LEFT,
        CROSS,
        RIGHT,
        BOTTOM,
        VERTICAL_RIGHT,
        VERTICAL_LEFT,
        HORIZONTAL_BOTTOM,
        HORIZONTAL_TOP,
        TOP_LEFT_DOUBLE,
        BOTTOM_LEFT_DOUBLE,
        TOP_RIGHT_DOUBLE,
        BOTTOM_RIGHT_DOUBLE,
        SLASH,
        BACKSLASH,
        CROSS_SLASH,
        CROSS_BACKSLASH,
        TOP_LEFT_DOUBLE_SLASH,
        BOTTOM_LEFT_DOUBLE_SLASH,
        TOP_RIGHT_DOUBLE_SLASH,
        BOTTOM_RIGHT_DOUBLE_SLASH,
        HORIZONTAL_DOUBLE,
        VERTICAL_DOUBLE,
        TOP_DOUBLE,
        LEFT_DOUBLE,
        CROSS_DOUBLE,
        RIGHT_DOUBLE,
        BOTTOM_DOUBLE,
    };

    constexpr const char* window_grid[] = {
        "┌",  // 1
        "└",  // 2
        "┐",  // 3
        "┘",  // 4
        "─",  // 5
        "┬",  // 6
        "│",  // 7
        "├",  // 8
        "┼",  // 9
        "┤",  // 10
        "┴",  // 11
        "╷",  // 12
        "╵",  // 13
        "╶",  // 14
        "╴",  // 15
        "╭",  // 16
        "╰",  // 17
        "╯",  // 18
        "╮",  // 19
        "╲",  // 20
        "╱",  // 21
        "╳",  // 22
        "╋",  // 23
        "╔",  // 24
        "╚",  // 25
        "╗",  // 26
        "╝",  // 27
        "═",  // 28
        "╦",  // 29
        "║",  // 30
        "╠",  // 31
        "╬",  // 32
        "╣",  // 33
        "╩",  // 34
    };

    // clang-format off
    constexpr byte basic_window[] = 
    {
        2,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,4,
        8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,
        8,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,8,
        8,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,8,
        8,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,8,
        8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,
        3,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,
    };
    // clang-format on

    constexpr bool make_layout_data(view::wvec data, size_t x, size_t y, size_t padding = 0) {
        if (data.size() != y * x) {
            return false;
        }
        for (size_t i = 0; i < y; i++) {
            for (size_t j = 0; j < x; j++) {
                auto index = i * x + j;
                if (i == 0 && j == 0) {
                    data[index] = 2;  // top left
                }
                else if (i == y - 1 && j == 0) {
                    data[index] = 3;  // bottom left
                }
                else if (i == 0 && j == x - 1) {
                    data[index] = 4;  // top right
                }
                else if (i == y - 1 && j == x - 1) {
                    data[index] = 5;  // bottom right
                }
                else if (i == 0 || i == y - 1) {
                    data[index] = 6;  // horizontal
                }
                else if (j == 0 || j == x - 1) {
                    data[index] = 8;  // vertical
                }
                else if (j < padding || (x - j - 1) < padding || i < padding || (y - i - 1) < padding) {
                    data[index] = 0;
                }
                else {
                    data[index] = 1;
                }
            }
        }
        return true;
    }

    template <size_t x, size_t y>
    constexpr auto make_layout_data(size_t padding = 0) {
        struct Array {
            byte data_[x * y]{};
            constexpr auto operator[](size_t i) const {
                return data_[i];
            }
            constexpr auto& operator[](size_t i) {
                return data_[i];
            }

            constexpr auto size() const {
                return x * y;
            }

            constexpr auto begin() {
                return data_;
            }

            constexpr auto end() {
                return data_ + size();
            }

            constexpr auto cbegin() const {
                return data_;
            }

            constexpr auto cend() const {
                return data_ + size();
            }

            constexpr auto data() {
                return data_;
            }

            constexpr auto data() const {
                return data_;
            }
        };
        Array data{};
        make_layout_data(data, x, y, padding);
        return data;
    }

    struct Window {
       private:
        size_t horizontal;
        size_t vertical;
        view::rvec layout_data;

        constexpr Window(size_t horizontal, size_t vertical, view::rvec layout_data)
            : horizontal{horizontal}, vertical{vertical}, layout_data{layout_data} {}

       public:
        constexpr auto dx() const {
            return horizontal;
        }
        constexpr auto dy() const {
            return vertical;
        }
        static constexpr std::optional<Window> create(view::rvec layout, size_t x, size_t y) {
            if (layout.size() != x * y) {
                return std::nullopt;
            }
            return Window{x, y, layout};
        }

        // text must be ascii or zenkaku
        constexpr bool draw(binary::writer& w, view::rvec text, size_t shift_right = 0, size_t shift_top = 0) const {
            auto seq = make_cpy_seq(text);
            auto write_char = [&](size_t index, size_t& j, size_t last) {
                utf::MiniBuffer<byte> minbuf{};
                auto pre = seq.rptr;
                utf::convert_one(seq, minbuf, false, false);
                if (seq.rptr == pre) {
                    return w.write(' ', 1);
                }
                if (minbuf.size() != 1) {
                    if (j == last) {
                        seq.rptr = pre;
                        return w.write(' ', 1);
                    }
                    if (layout_data[index + 1] != 1) {  // not char space
                        seq.rptr = pre;
                        return w.write(' ', 1);
                    }
                    j++;
                }
                else if (minbuf[0] == '\n') {
                    if (j == last) {
                        return true;
                    }
                    if (!w.write(' ', last - j)) {
                        return false;
                    }
                    j = last;
                    return true;
                }
                if (!w.write(minbuf)) {
                    return false;
                }
                return true;
            };
            auto write_tile = [&](WindowTileType type) {
                auto tile = window_grid[static_cast<byte>(type) - 2];
                return w.write(view::rcvec(tile, futils::strlen(tile)));
            };
            if (!w.write('\n', shift_top)) {
                return false;
            }
            for (size_t i = 0; i < vertical; i++) {
                for (size_t k = 0; k < shift_right; k++) {
                    if (!w.write(' ', 1)) {
                        return false;
                    }
                }
                for (size_t j = 0; j < horizontal; j++) {
                    auto index = i * horizontal + j;
                    auto tile = layout_data[index];
                    if (tile == 0) {
                        if (!w.write(' ', 1)) {
                            return false;
                        }
                    }
                    else if (tile == 1) {
                        if (!write_char(index, j, horizontal - 1)) {
                            return false;
                        }
                    }
                    else {
                        if (!write_tile(static_cast<WindowTileType>(tile))) {
                            return false;
                        }
                    }
                }
                if (!w.write('\n', 1)) {
                    return false;
                }
            }
            return true;
        }

        constexpr bool reset_layout(view::rvec layout) {
            if (layout.size() != horizontal * vertical) {
                return false;
            }
            layout_data = layout;
            return true;
        }
    };

    constexpr auto basic_window_layout = *Window::create(view::rvec(basic_window, sizeof(basic_window)), 20, 7);

    namespace test {
        constexpr bool test_window() {
            byte buf[1200]{}, data[] = "Hello, World!";
            binary::writer w{buf};
            basic_window_layout.draw(w, data);
            return true;
        }
    }  // namespace test
}  // namespace futils::console
