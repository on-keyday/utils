/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "../../binary/number.h"
#include "deflate.h"

namespace futils::file::gzip {
    enum GZIPHeaderFlag : byte {
        FNONE = 0x0,
        FTEXT = 0x1,
        FHCRC = 0x2,
        FEXTRA = 0x4,
        FNAME = 0x8,
        FCOMMENT = 0x10,
        FRESERVED = 0xE0,
    };

    enum class CompressionMethod : byte {
        reserved_0 = 0,
        deflate = 8,
    };

    enum class OS : byte {
        fat = 0,
        amiga = 1,
        vms = 2,
        unix_ = 3,
        vm_cms = 4,
        atari = 5,
        hpfs = 6,
        machintosh = 7,
        z_system = 8,
        cp_m = 9,
        tops_20 = 10,
        ntfs = 11,
        qdos = 12,
        acorn_risc_os = 13,
        vfat = 14,
        mvs = 15,
        beos = 16,
    };

    GZIPHeaderFlag& operator|=(GZIPHeaderFlag& a, GZIPHeaderFlag b) {
        a = GZIPHeaderFlag(a | b);
        return a;
    }

    constexpr byte valid_id_impl[2] = {0x1f, 0x8b};

    constexpr auto valid_id = view::rvec(valid_id_impl, 2);

    struct GZipHeader {
        byte id[2]{};
        CompressionMethod cm = CompressionMethod::reserved_0;
        GZIPHeaderFlag flag = FNONE;
        std::uint32_t mtime = 0;
        byte xfl = 0;
        OS os = OS::fat;
        byte xlen = 0;
        view::rvec extra;
        view::rvec fname;
        view::rvec fcomment;
        std::uint16_t crc16 = 0;
        view::rvec content;
        std::uint32_t crc32 = 0;
        std::uint32_t isize = 0;

        constexpr bool parse_optional_fields(binary::reader& r) noexcept {
            if (flag & FEXTRA) {
                xlen = r.top();
                r.offset(1);
                if (!r.read(extra, xlen)) {
                    return false;
                }
            }
            if (flag & FNAME) {
                auto peek = r.clone();
                while (peek.top() != 0) {
                    peek.offset(1);
                }
                if (peek.empty()) {
                    return false;
                }
                if (!r.read(fname, peek.offset() - r.offset())) {
                    return false;
                }
                r.offset(1);  // ignore zero
            }
            if (flag & FCOMMENT) {
                auto peek = r.clone();
                while (peek.top() != 0) {
                    peek.offset(1);
                }
                if (peek.empty()) {
                    return false;
                }
                if (!r.read(fcomment, peek.offset() - r.offset())) {
                    return false;
                }
                r.offset(1);  // ignore zero
            }
            if (flag & FHCRC) {
                if (!binary::read_num(r, crc16, false)) {
                    return false;
                }
            }
            if (flag & FRESERVED) {
                return false;
            }
            return true;
        }

        constexpr bool parse_header(binary::reader& r) noexcept {
            return r.read(id) &&
                   id == valid_id &&
                   binary::read_num(r, cm) &&
                   binary::read_num(r, flag) &&
                   binary::read_num(r, mtime, false) &&
                   r.read(view::wvec(&xfl, 1)) &&
                   binary::read_num(r, os) &&
                   parse_optional_fields(r);
        }

        constexpr bool parse_trailer(binary::reader& r) noexcept {
            return binary::read_num(r, crc32, false) &&
                   binary::read_num(r, isize, false);
        }

        constexpr bool parse(binary::reader& r) noexcept {
            return parse_header(r) &&
                   r.read(content, r.remain().size() - 8) &&
                   parse_trailer(r);
        }

        constexpr bool render_optional_fields(binary::writer& w) const {
            if (flag & FEXTRA) {
                if (extra.size() > 0xff) {
                    return false;
                }
                if (!w.write(byte(extra.size()), 1) ||
                    !w.write(extra)) {
                    return false;
                }
            }
            auto has_zero = [](view::rvec r) {
                for (auto c : r) {
                    if (c == 0) {
                        return true;
                    }
                }
                return false;
            };
            if (flag & FNAME) {
                if (has_zero(fname)) {
                    return false;
                }
                if (!w.write(fname) || !w.write(0, 1)) {
                    return false;
                }
            }
            if (flag & FCOMMENT) {
                if (has_zero(fcomment)) {
                    return false;
                }
                if (!w.write(fcomment) || !w.write(0, 1)) {
                    return false;
                }
            }
            if (flag & FHCRC) {
                if (!binary::write_num(w, crc16, false)) {
                    return false;
                }
            }
            return true;
        }

        constexpr bool render_in_place(binary::writer& w, auto&& render_contnet) const {
            auto ok = w.write(valid_id) &&
                      w.write(byte(cm), 1) &&
                      w.write(byte(flag & ~FRESERVED), 1) &&
                      binary::write_num(w, mtime, false) &&
                      w.write(xfl, 1) &&
                      w.write(byte(os), 1) &&
                      render_optional_fields(w);
            if (!ok) {
                return false;
            }
            const auto cur = w.offset();
            if (!render_contnet(w)) {
                return false;
            }
            const auto fin = w.offset();
            if (cur > fin) {
                return false;
            }
            return binary::write_num(w, crc32) &&
                   binary::write_num(w, isize, false);
        }

        constexpr bool valid() const {
            return id == valid_id &&
                   !(flag & FRESERVED);
        }
    };

    template <class T, class Out>
    deflate::DeflateError decode_gzip(Out& out, GZipHeader& head, binary::bit_reader<T>& r) {
        binary::expand_reader<T>& base = r.get_base();
        if (!base.check_input(20)) {
            return deflate::DeflateError::input_length;
        }
        auto br = base.reader().clone();
        while (!head.parse_header(br)) {
            if (!head.valid()) {
                return deflate::DeflateError::invalid_header;
            }
            if (!base.check_input(10)) {
                return deflate::DeflateError::input_length;
            }
        }
        if (head.cm != CompressionMethod::deflate) {
            return deflate::DeflateError::invalid_header;
        }
        base.offset(br.offset() - base.offset());
        if (auto err = deflate::decode_deflate(out, r); err != deflate::DeflateError::none) {
            return err;
        }
        if (!r.skip_align()) {
            return deflate::DeflateError::input_length;
        }
        if (!base.check_input(8)) {
            return deflate::DeflateError::input_length;
        }
        br = base.reader().clone();
        if (!head.parse_trailer(br)) {
            return deflate::DeflateError::input_length;
        }
        base.offset(br.offset() - base.offset());
        if (out.size() % (~std::uint32_t(0)) != head.isize) {
            return deflate::DeflateError::broken_data;
        }
        return deflate::DeflateError::none;
    }
}  // namespace futils::file::gzip
