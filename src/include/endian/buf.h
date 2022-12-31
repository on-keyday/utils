/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encdec - encode/decode methods
#pragma once
#include <type_traits>
#include "../core/byte.h"
#include <bit>

namespace utils {
    namespace endian {

        template <class T, class Data = byte[sizeof(T)]>
        struct Buf {
            Data data;

            static constexpr auto bit_per_byte = 8;

            constexpr byte& operator[](size_t i) {
                return data[i];
            }

            constexpr const byte& operator[](size_t i) const {
                return data[i];
            }

            constexpr size_t size() const {
                return sizeof(T);
            }

            // encode `input` as big endian into `this->data`
            constexpr Buf& write_be(T input) {
                std::make_unsigned_t<decltype(input)> in = input;
                for (size_t i = 0; i < sizeof(input); i++) {
                    data[i] = byte((in >> ((sizeof(input) - 1 - i) * bit_per_byte)) & 0xff);
                }
                return *this;
            }

            // encode `input` as little endian into `this->data`
            constexpr Buf& write_li(T input) {
                std::make_unsigned_t<decltype(input)> in = input;
                for (size_t i = 0; i < sizeof(input); i++) {
                    data[i] = byte((in >> (i * bit_per_byte)) & 0xff);
                }
                return *this;
            }

            // read_be read big endian integer from `data`
            constexpr const Buf& read_be(T& output) const {
                using Out = std::make_unsigned_t<T>;
                Out out = 0;
                for (auto i = 0; i < sizeof(out); i++) {
                    out |= Out(data[i]) << ((sizeof(out) - 1 - i) * bit_per_byte);
                }
                output = out;
                return *this;
            }

            // read_be read big endian integer from `data`
            constexpr T read_be() const {
                T v;
                read_be(v);
                return v;
            }

            // read_be read little endian integer from `data`
            constexpr const Buf& read_le(T& output) const {
                using Out = std::make_unsigned_t<T>;
                Out out = 0;
                for (auto i = 0; i < sizeof(out); i++) {
                    out |= Out(data[i]) << (i * bit_per_byte);
                }
                output = out;
                return *this;
            }

            // read_be read little endian integer from `data`
            constexpr T read_le() const {
                T v;
                read_le(v);
                return v;
            }

            // into call output.push_back for each `data` element
            constexpr const Buf& into(auto&& output) const {
                for (auto i = 0; i < size(); i++) {
                    output.push_back(data[i]);
                }
                return *this;
            }

            // from call input[offset+i] for each `data` element
            constexpr Buf& from(auto&& input, size_t offset = 0) {
                for (auto i = 0; i < size(); i++) {
                    data[i] = input[offset + i];
                }
                return *this;
            }

            // into_array call output[offset+i] for each data element
            constexpr const Buf& into_array(auto&& output, size_t offset = 0) const {
                for (auto i = 0; i < size(); i++) {
                    output[offset + i] = data[i];
                }
                return *this;
            }

            constexpr Buf& reverse() {
                for (auto i = 0; i < sizeof(T) / 2; i++) {
                    auto tmp = data[i];
                    data[i] = data[sizeof(T) - 1 - i];
                    data[sizeof(T) - 1 - i] = tmp;
                }
                return *this;
            }
        };

        template <class T>
        constexpr T bswap(T input) {
            using Out = std::make_unsigned_t<T>;
            Out in = input, out = 0;
            for (auto i = 0; i < sizeof(T); i++) {
                out |= Out(byte((in >> (i * 8)) & 0xff)) << ((sizeof(T) - 1 - i) * 8);
            }
            return out;
        }

        // using direct memory access
        template <class T>
        inline T bswap_direct(T input) {
            T hold;
            auto direct0 = (byte*)&hold;
            auto direct1 = (byte*)&input;
            for (auto i = 0; i < sizeof(T); i++) {
                direct0[i] = direct1[sizeof(T) - 1 - i];
            }
            return hold;
        }

        template <class T>
        constexpr void write_into(auto&& output, T input, bool be) {
            Buf<T, decltype(output)> buf{output};
            if (be) {
                buf.write_be(input);
            }
            else {
                buf.write_li(input);
            }
        }

        template <class T>
        constexpr void read_from(T& output, auto&& input, bool be, size_t offset = 0) {
            Buf<T, decltype(input)> buf{input};
            if (be) {
                buf.read_be(output);
            }
            else {
                buf.read_le(output);
            }
        }

        template <class T>
        constexpr T read_from(auto&& input, bool be, size_t offset = 0) {
            T output;
            read_from(output, input, be, offset);
            return output;
        }

        // is_little judges whether this program runs on little endian
        inline bool is_little() {
            const int test = 0x00000001;
            auto test_vec = reinterpret_cast<const byte*>(&test);
            return test_vec[0] == 0x01;
        }

        template <class T>
        constexpr T bswap_net(T input) {
            if constexpr (std::endian::native == std::endian::little) {
                return bswap(input);
            }
            else if constexpr (std::endian::native == std::endian::big) {
                return input;
            }
            else {  // bi-endian?
                if (is_little()) {
                    return bswap(input);
                }
                else {
                    return input;
                }
            }
        }

    }  // namespace endian
}  // namespace utils
