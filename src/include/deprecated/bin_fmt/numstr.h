/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// numstr - common number and string write
// almost copied from commonlib
#pragma once

#include <cstdint>
#include "../endian/reader.h"
#include "../endian/writer.h"
#include "../utf/view.h"

namespace utils {
    namespace bin_fmt {
        struct CodeLimit {  //number codeing like QUIC
            static constexpr size_t limit8 = 0x3f;
            static constexpr size_t limit16 = 0x3fff;
            static constexpr std::uint16_t mask16 = 0x4000;
            static constexpr std::uint8_t mask16_8 = 0x40;
            static constexpr size_t limit32 = 0x3fffffff;
            static constexpr std::uint32_t mask32 = 0x80000000;
            static constexpr std::uint8_t mask32_8 = 0x80;
            static constexpr size_t limit64 = 0x3fffffffffffffff;
            static constexpr std::uint64_t mask64 = 0xC000000000000000;
            static constexpr std::uint8_t mask64_8 = 0xC0;
        };

        struct BinaryIO {
            template <class Buf>
            static bool read_num(endian::Reader<Buf>& target, size_t& size) {
                std::uint8_t e = target.seq.current();
                std::uint8_t masked = e & CodeLimit::mask64_8;
                auto set_v = [&](auto& v, auto unmask) {
                    if (!target.read_ntoh(v)) {
                        return false;
                    }
                    size = v & ~unmask;
                    return true;
                };
                if (masked == CodeLimit::mask64_8) {
                    std::uint64_t v;
                    return set_v(v, CodeLimit::mask64);
                }
                else if (masked == CodeLimit::mask32_8) {
                    std::uint32_t v;
                    return set_v(v, CodeLimit::mask32);
                }
                else if (masked == CodeLimit::mask16_8) {
                    std::uint16_t v;
                    return set_v(v, CodeLimit::mask16);
                }
                else {
                    std::uint8_t v;
                    return set_v(v, 0);
                }
            }

            template <class Buf>
            static bool write_num(endian::Writer<Buf>& target, size_t size) {
                if (size <= CodeLimit::limit8) {
                    target.write_hton(std::uint8_t(size));
                }
                else if (size <= CodeLimit::limit16) {
                    std::uint16_t v = std::uint16_t(size) | CodeLimit::mask16;
                    target.write_hton(v);
                }
                else if (size <= CodeLimit::limit32) {
                    std::uint32_t v = std::uint32_t(size) | CodeLimit::mask32;
                    target.write_hton(v);
                }
                else if (size <= CodeLimit::limit64) {
                    std::uint64_t v = std::uint64_t(size) | CodeLimit::mask64;
                    target.write_hton(v);
                }
                else {
                    return false;
                }
                return true;
            }

            template <class Buf, class String>
            static bool write_string(endian::Writer<Buf>& target, String& str) {
                utf::U8View<String&> buf(str);
                if (buf.size() == 0) {
                    return false;
                }
                if (!write_num(target, buf.size())) {
                    return false;
                }
                for (size_t i = 0; i < buf.size(); i++) {
                    target.write(buf[i]);
                }
                return true;
            }

            template <class Buf, class String>
            static bool read_string(endian::Reader<Buf>& target, String& str) {
                size_t size = 0;
                if (!read_num(target, size)) {
                    return false;
                }
                return target.read_seq(str, size);
            }
        };

    }  // namespace bin_fmt
}  // namespace utils
