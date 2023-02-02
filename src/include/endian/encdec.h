/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encdec - endian encoder/decoder
#pragma once
#include "buf.h"

namespace utils {
    namespace endian {
        template <class T>
        struct Encoder {
            // out has `push_back` member function
            T out;

            // encode `input` as big endian into `this->out`
            constexpr void write_be(auto input) {
                Buf<decltype(input)> buf;
                buf.write_be(input);
                buf.into(out);
            }

            // encode `input` as little endian into `this->out`
            constexpr void write_le(auto input) {
                Buf<decltype(input)> buf;
                buf.write_le(input);
                buf.into(out);
            }
        };

        template <class T>
        struct Decoder {
            // in has `operator[]` and `size` member function
            T in;
            // index is index of `in`
            size_t index = 0;

            // can_decode report whether remaining is enough to decode `size` byte
            constexpr bool can_read(size_t size) const {
                return in.size() >= size + index;
            }

            // read_be read big endian integer from `in` byte array
            constexpr bool read_be(auto& output) {
                if (!can_read(sizeof(output))) {
                    return false;
                }
                Buf<decltype(output)> buf;
                buf.from(in, index);
                buf.read_be(output);
                index += sizeof(output);
                return true;
            }

            // read_be read little endian integer from `in` byte array
            constexpr bool read_le(auto& output) {
                if (!can_read(sizeof(output))) {
                    return false;
                }
                Buf<decltype(output)> buf;
                buf.from(in, index);
                buf.read_le(output);
                index += sizeof(output);
                return true;
            }
        };

    }  // namespace endian
}  // namespace utils