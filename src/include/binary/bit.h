/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/expandable_reader.h>
#include <binary/expandable_writer.h>

namespace futils {
    namespace binary {

        template <class T, class C>
        struct basic_bit_writer {
           private:
            byte bits = 0;
            byte index = 0;
            basic_expand_writer<T, C> target;
            bool from_lsb = false;

           public:
            constexpr basic_bit_writer(auto&& v)
                : target(std::forward<decltype(v)>(v)) {}

            constexpr basic_bit_writer() = default;

            constexpr binary::basic_expand_writer<T, C>& get_base() {
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

            constexpr bool is_aligned() const {
                return index % 8 == 0;
            }
        };

        template <class T, class C>
        struct basic_bit_reader {
           private:
            byte index = 0;
            binary::basic_expand_reader<T, C> target;
            bool from_lsb = false;

           public:
            constexpr basic_bit_reader(auto&& v)
                : target(std::forward<decltype(v)>(v)) {}

            constexpr basic_bit_reader() = default;

            constexpr binary::basic_expand_reader<T, C>& get_base() {
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

            constexpr bool is_aligned() const {
                return index % 8 == 0;
            }
        };

        template <class T>
        using bit_reader = basic_bit_reader<T, byte>;

        template <class T>
        using bit_writer = basic_bit_writer<T, byte>;

    }  // namespace binary
}  // namespace futils
