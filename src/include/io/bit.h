/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "expandable_writer.h"
#include "expandable_reader.h"

namespace utils {
    namespace io {

        template <class T>
        struct bit_writer {
           private:
            byte bits = 0;
            byte index = 0;
            expand_writer<T> target;
            bool from_lsb = false;

           public:
            constexpr bit_writer(auto&& v)
                : target(std::forward<decltype(v)>(v)) {}

            constexpr bit_writer() = default;

            constexpr io::expand_writer<T>& get_base() {
                return target;
            }

            constexpr void set_direction(bool from_lsb) {
                this->from_lsb = from_lsb;
            }

            constexpr byte current_mask() const {
                if (from_lsb) {
                    return byte(1) << index;
                }
                else {
                    return byte(1) << (7 - index);
                }
            }

            constexpr void push_back(bool b) {
                const auto mask = current_mask();
                if (b) {
                    bits |= mask;
                }
                index++;
                if (index == 8) {
                    target.push_back(bits);
                    bits = 0;
                    index = 0;
                }
            }

            constexpr void push_align() {
                if (index != 0) {
                    target.push_back(bits);
                    bits = 0;
                    index = 0;
                }
            }

            constexpr void reset_bits_index() {
                bits = 0;
                index = 0;
            }

            constexpr byte get_index() const {
                return index;
            }
        };

        template <class T>
        struct bit_reader {
           private:
            byte index = 0;
            io::expand_reader<T> target;
            bool from_lsb = false;

           public:
            constexpr bit_reader(auto&& v)
                : target(std::forward<decltype(v)>(v)) {}

            constexpr bit_reader() = default;

            constexpr io::expand_reader<T>& get_base() {
                return target;
            }

            constexpr bool skip() {
                bool tmp;
                return read(tmp);
            }

            constexpr bool skip_align() {
                while (index != 0) {
                    if (!skip()) {
                        return false;
                    }
                }
                return true;
            }

            // for deflate format
            // if from_lsb is true, |01011011| are read as order (1,1,0,1,1,0,1,0)
            // otherwise |01011011| are read as order (0,1,0,1,1,0,1,1)
            constexpr void set_direction(bool from_lsb) {
                this->from_lsb = from_lsb;
            }

            constexpr byte current_mask() const {
                if (from_lsb) {
                    return byte(1) << index;
                }
                else {
                    return byte(1) << (7 - index);
                }
            }

            constexpr bool top() const {
                return bool(target.top() & current_mask());
            }

            constexpr bool read(bool& fit) {
                const auto mask = current_mask();
                if (target.remain().size() == 0) {
                    if (!target.check_input(1)) {
                        return false;
                    }
                }
                fit = bool(target.top() & mask);
                index++;
                if (index == 8) {
                    target.offset(1);
                    index = 0;
                }
                return true;
            }

            constexpr bool read(auto& data, byte n, bool be = true) {
                if (n > sizeof(data) * 8) {
                    return false;
                }
                data = 0;
                for (auto i = 0; i < n; i++) {
                    bool b = false;
                    if (!read(b)) {
                        return false;
                    }
                    if (b) {
                        data |= std::make_unsigned_t<std::decay_t<decltype(data)>>(1) << (be ? (n - 1 - i) : (i));
                    }
                }
                return true;
            }

            constexpr void reset_index() {
                index = 0;
            }

            constexpr byte get_index() const {
                return index;
            }
        };

    }  // namespace io
}  // namespace utils
