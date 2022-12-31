/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bytelen -ByteLen
#pragma once
#include <cstdint>
#include <type_traits>
#include <bit>
#include "quic/types.h"
#include "../core/byte.h"
#include <utility>
#include "../endian/buf.h"

namespace utils {
    namespace dnet {
        namespace internal {
            template <class From, class To>
            concept convertible = std::is_convertible_v<From, To>;

            template <class T>
            concept has_size = requires(T t) {
                                   { t.size() } -> convertible<size_t>;
                               };

            template <class T>
            concept has_logical_not = requires(T t) {
                                          { !t } -> convertible<bool>;
                                      };

            constexpr auto equal_fn() {
                return [](auto&& a, auto&& b) { return a == b; };
            };
        }  // namespace internal

        template <class Byte>
        struct ByteLenBase {
            Byte* data = nullptr;
            size_t len = 0;

            constexpr bool enough(size_t l) const {
                return data && len >= l;
            }
            constexpr bool valid() const {
                return enough(0);
            }

            constexpr bool adjacent(const ByteLenBase& v) const {
                if (!valid() || !v.valid()) {
                    return false;
                }
                return data + len == v.data;
            }

            template <class T>
            constexpr bool as(T& value, size_t offset = 0, bool big_endian = true) const {
                if (!enough((offset + 1) * sizeof(T))) {
                    return false;
                }
                auto ofs = offset * sizeof(T);
                endian::read_from(value, data, big_endian, ofs);
                return true;
            }

            constexpr bool packet_number(std::uint32_t& pn, size_t len) {
                if (len == 0 || len > 4) {
                    return false;
                }
                if (!enough(len)) {
                    return false;
                }
                if (len == 1) {
                    pn = data[0];
                }
                else if (len == 2) {
                    pn = std::uint32_t(data[0]) << 8 | data[1];
                }
                else if (len == 3) {
                    pn = std::uint32_t(data[0]) << 16 | std::uint32_t(data[1]) << 8 | data[2];
                }
                else {
                    pn = std::uint32_t(data[0]) << 24 | std::uint32_t(data[1]) << 16 |
                         std::uint32_t(data[2]) << 8 | data[3];
                }
                return true;
            }

            template <class T>
            constexpr T as(size_t offset = 0, bool big_endian = true) const {
                T val{};
                as(val, offset, big_endian);
                return val;
            }

            constexpr size_t qvarintlen() const {
                if (!enough(1)) {
                    return 0;
                }
                auto mask = data[0] & 0xC0;
                if (mask == 0x00) {
                    return 1;
                }
                if (mask == 0x40) {
                    return 2;
                }
                if (mask == 0x80) {
                    return 4;
                }
                if (mask == 0xC0) {
                    return 8;
                }
                return 0;
            }

            constexpr bool qvarint(size_t& value) const {
                auto len = qvarintlen();
                if (len == 0) {
                    return false;
                }
                if (len == 1) {
                    value = data[0];
                    return true;
                }
                if (!enough(len)) {
                    return false;
                }
                if (len == 2) {
                    std::uint16_t val = 0;
                    if (!as(val)) {
                        return false;
                    }
                    val &= ~0xC000;
                    value = val;
                    return true;
                }
                if (len == 4) {
                    std::uint32_t val = 0;
                    if (!as(val)) {
                        return false;
                    }
                    val &= ~0xC0000000;
                    value = val;
                    return true;
                }
                if (len == 8) {
                    std::uint64_t val = 0;
                    if (!as(val)) {
                        return false;
                    }
                    val &= ~0xC000000000000000;
                    value = val;
                    return true;
                }
                return false;
            }

            constexpr size_t qvarint() const {
                size_t value = 0;
                qvarint(value);
                return value;
            }

            constexpr bool forward(ByteLenBase& blen, size_t offset) const {
                if (!enough(offset)) {
                    return false;
                }
                blen = ByteLenBase{data + offset, len - offset};
                return true;
            }

            constexpr ByteLenBase forward(size_t offset) const {
                ByteLenBase blen{};
                forward(blen, offset);
                return blen;
            }

            constexpr bool resized(ByteLenBase& blen, size_t resize) const {
                if (!enough(resize)) {
                    return false;
                }
                blen = ByteLenBase{data, resize};
                return true;
            }

            constexpr ByteLenBase resized(size_t resize) const {
                ByteLenBase blen{};
                resized(blen, resize);
                return blen;
            }

            constexpr bool fwdenough(size_t offset, size_t expect) {
                *this = this->forward(offset);
                return enough(expect);
            }

            constexpr bool fwdresize(ByteLenBase& to, size_t offset, size_t expect) {
                if (!fwdenough(offset, expect)) {
                    return false;
                }
                to = resized(expect);
                return true;
            }

            constexpr size_t qvarintfwd(size_t& to) {
                auto len = qvarintlen();
                if (!qvarint(to)) {
                    return 0;
                }
                *this = forward(len);
                return len;
            }

            constexpr bool qvarintfwd(ByteLenBase& to) {
                auto len = qvarintlen();
                if (!enough(len)) {
                    return 0;
                }
                to = resized(len);
                *this = forward(len);
                return true;
            }

            constexpr bool is_qvarint_valid() const {
                auto vlen = qvarintlen();
                if (vlen == 0 || vlen > len) {
                    return false;
                }
                return true;
            }

            template <class Eq = decltype(internal::equal_fn())>
            constexpr size_t match_len(auto&& in, size_t size, Eq&& is_equal = internal::equal_fn()) const {
                size_t i = 0;
                for (; i < size; i++) {
                    if (i >= len) {
                        break;
                    }
                    if (!is_equal(data[i], in[i])) {
                        break;
                    }
                }
                return i;
            }

            constexpr static size_t get_size(auto&& in) {
                if constexpr (internal::has_size<decltype(in)>) {
                    return in.size();
                }
                if constexpr (internal::has_logical_not<decltype(in)>) {
                    if (!in) {
                        return 0;
                    }
                }
                // null terminated string
                size_t i = 0;
                while (in[i]) {
                    i++;
                }
                return i;
            }

            template <class Eq = decltype(internal::equal_fn())>
            constexpr size_t match_len(auto&& in, Eq&& is_equal = internal::equal_fn()) const {
                return match_len(in, get_size(in), is_equal);
            }

            template <class In, bool non_zero = true, class Eq = decltype(internal::equal_fn())>
            constexpr bool expect(In&& in, Eq&& eq = internal::equal_fn()) const {
                auto size = get_size(in);
                if (non_zero && size == 0) {
                    return false;
                }
                auto l = match_len(in, size, eq);
                return l == size;
            }

            template <class In, bool non_zero = true, class Eq = decltype(internal::equal_fn())>
            constexpr bool expectfwd(In&& in, Eq&& eq = internal::equal_fn()) const {
                auto size = get_size(in);
                if (non_zero && size == 0) {
                    return false;
                }
                auto l = match_len(in, size, eq);
                if (l == size) {
                    *this = forward(l);
                }
                return l == size;
            }

            constexpr size_t msbvarint_len() const {
                size_t i = 1;
                while (true) {
                    if (!enough(i)) {
                        return 0;
                    }
                    if (data[i - 1] & 0x80) {
                        i++;
                        continue;
                    }
                    return i;
                }
            }

            constexpr bool msbvarint(size_t& value, size_t len_max = 10) const {
                size_t size = msbvarint_len();
                if (size == 0 || size > len_max) {
                    return false;
                }
                size_t result = 0;
                for (auto i = 0; i < size; i++) {
                    byte dat = data[i] & 0x7f;
                    result |= size_t(dat) << (7 * i);
                }
                return true;
            }

            constexpr size_t msbvarint(size_t len_max = 10) const {
                size_t val = 0;
                msbvarint(val, len_max);
                return val;
            }
        };

        using ByteLen = ByteLenBase<byte>;
        using ConstByteLen = ByteLenBase<const byte>;

        struct WPacket {
            ByteLen b;
            size_t offset = 0;
            bool overflow = false;
            size_t counter = 0;

            constexpr void reset_offset() {
                offset = 0;
                overflow = false;
                counter = 0;
            }

            template <class Byte>
            constexpr void append(const Byte* src, size_t len) {
                counter += len;
                static_assert(sizeof(src[0]) == 1, "must be 1 byte sequence");
                for (auto i = 0; i < len; i++) {
                    if (b.len <= offset) {
                        overflow = true;
                        break;
                    }
                    b.data[offset] = src[i];
                    offset++;
                }
            }

            constexpr void add(byte src, size_t len) {
                counter += len;
                for (auto i = 0; i < len; i++) {
                    if (b.len <= offset) {
                        overflow = true;
                        break;
                    }
                    b.data[offset] = src;
                    offset++;
                }
            }

            constexpr ByteLen acquire(size_t len) {
                counter += len;
                if (b.len < offset + len) {
                    return {};
                }
                auto res = ByteLen{b.data + offset, len};
                offset += len;
                return res;
            }

            constexpr ByteLen aligned_aquire(size_t len, size_t align) {
                while (align != 0 && offset % align) {
                    if (b.len < offset + len) {
                        return {};
                    }
                    offset++;
                }
                return acquire(len);
            }

            constexpr ByteLen zeromem(size_t len) {
                auto buf = acquire(len);
                if (!buf.enough(len)) {
                    return {};
                }
                for (auto i = 0; i < len; i++) {
                    buf.data[i] = 0;
                }
                return buf;
            }

            template <class B>
            constexpr ByteLen copy_from(ByteLenBase<B> from) {
                auto to = acquire(from.len);
                if (!to.enough(from.len)) {
                    return {};
                }
                for (auto i = 0; i < from.len; i++) {
                    to.data[i] = from.data[i];
                }
                return to;
            }

            template <class T>
            constexpr ByteLen as(T val, bool big_endian = true) {
                auto buf = acquire(sizeof(T));
                if (!buf.enough(sizeof(T))) {
                    return {};
                }
                std::make_unsigned_t<T> src = val;
                if (big_endian) {
                    for (auto i = 0; i < sizeof(T); i++) {
                        buf.data[i] = (src >> (8 * (sizeof(T) - 1 - i))) & 0xff;
                    }
                }
                else {
                    for (auto i = 0; i < sizeof(T); i++) {
                        buf.data[i] = (src >> (8 * i)) & 0xff;
                    }
                }
                return buf;
            }

            constexpr byte* as_byte(size_t val) {
                if (val > 0xff) {
                    return nullptr;
                }
                return as<byte>(val).data;
            }

            constexpr size_t qvarintlen(size_t val, byte* maskres = nullptr) {
                constexpr auto masks = 0xC000000000000000;
                if (val & masks) {
                    return 0;
                }
                val &= ~masks;
                size_t len = 0;
                byte mask;
                if (val < 0x40) {
                    len = 1;
                    mask = 0;
                }
                else if (val < 0x4000) {
                    len = 2;
                    mask = 0x40;
                }
                else if (val < 0x40000000) {
                    len = 4;
                    mask = 0x80;
                }
                else {
                    len = 8;
                    mask = 0xC0;
                }
                if (maskres) {
                    *maskres = mask;
                }
                return len;
            }

            constexpr ByteLen qvarint(size_t val) {
                byte mask;
                size_t len = qvarintlen(val, &mask);
                if (len == 0) {
                    return {};
                }
                auto buf = acquire(len);
                if (!buf.enough(len)) {
                    return {};
                }
                if (len == 1) {
                    buf.data[0] = byte(val);
                    return buf;
                }
                for (auto i = 0; i < len; i++) {
                    buf.data[i] = (val >> (8 * (len - 1 - i))) & 0xff;
                    if (i == 0) {
                        buf.data[i] |= mask;
                    }
                }
                return buf;
            }

            std::pair<quic::FrameFlags, ByteLen> frame_type(quic::FrameType ty) {
                auto val = qvarint(size_t(ty));
                if (!val.valid()) {
                    return {};
                }
                return {quic::FrameFlags{size_t(ty)}, val};
            }

            std::pair<quic::PacketFlags, ByteLen> packet_flags(byte raw) {
                auto len = acquire(1);
                if (!len.enough(1)) {
                    return {};
                }
                *len.data = raw;
                return {quic::PacketFlags{*len.data}, len};
            }

            constexpr ByteLen packet_number(std::uint32_t value, byte len) {
                if (len == 0 || len > 0x4) {
                    return {};
                }
                auto num = acquire(len);
                if (!num.enough(len)) {
                    return {};
                }
                if (len == 1) {
                    num.data[0] = value & 0xff;
                }
                else if (len == 2) {
                    num.data[0] = (value >> 8) & 0xff;
                    num.data[1] = value & 0xff;
                }
                else if (len == 3) {
                    num.data[0] = (value >> 16) & 0xff;
                    num.data[1] = (value >> 8) & 0xff;
                    num.data[2] = value & 0xff;
                }
                else {
                    num.data[0] = (value >> 24) & 0xff;
                    num.data[1] = (value >> 16) & 0xff;
                    num.data[2] = (value >> 8) & 0xff;
                    num.data[3] = value & 0xff;
                }
                return num;
            }
        };

        ByteLen range_aquire(WPacket& wp, auto&& cb) {
            auto begin_offset = wp.offset;
            if (!cb(wp)) {
                return {};
            }
            auto end_offset = wp.offset;
            if (begin_offset > end_offset) {
                return {};
            }
            wp.offset = begin_offset;
            return wp.acquire(end_offset - begin_offset);
        }

        template <class T>
        constexpr T byteswap(T t) {
            byte d[sizeof(T)];
            WPacket w{{d, sizeof(T)}};
            w.as(t);
            return w.b.as<T>(0, false);
        }

        template <class T>
        constexpr T netorder(T t) {
            return std::endian::native == std::endian::big ? t : byteswap(t);
        }

        struct QVarInt {
            std::uint64_t value = 0;
            size_t len = 0;
            static constexpr size_t lenmask = (size_t(0xC0) << (7 * 8));
            constexpr QVarInt() = default;
            constexpr QVarInt(size_t v)
                : value(v & ~lenmask) {}

            constexpr operator size_t() const {
                return value;
            }

            constexpr bool parse(ByteLen& b) {
                if (!b.qvarint(value)) {
                    return false;
                }
                len = b.qvarintlen();
                b = b.forward(len);
                return true;
            }

            constexpr bool render(WPacket& w) const {
                if (value & lenmask) {
                    return false;
                }
                w.qvarint(value);
                return true;
            }

            static size_t minimum_len(size_t value) {
                if (value & lenmask) {
                    return 0;
                }
                if (value < 0x40) {
                    return 1;
                }
                if (value < 0x4000) {
                    return 2;
                }
                if (value < 0x40000000) {
                    return 4;
                }
                return 8;
            }

            constexpr size_t minimum_len() const {
                return minimum_len(value);
            }

            constexpr bool is_minimum() const {
                auto minlen = minimum_len();
                if (minlen == 0) {
                    return false;
                }
                return minlen == len;
            }
        };

        // QPacketNumber is packet number representation on wire
        // value of this is truncated to len
        struct QPacketNumber {
            std::uint32_t value = 0;
            std::uint32_t len = 0;

            constexpr QPacketNumber() = default;
            constexpr QPacketNumber(std::uint32_t v)
                : value(v) {}

            constexpr QPacketNumber(std::uint32_t v, std::uint32_t l)
                : value(v), len(l) {}
            constexpr bool parse(ByteLen& b, byte l) {
                if (!b.packet_number(value, l)) {
                    return false;
                }
                b = b.forward(l);
                len = l;
                return true;
            }

            constexpr bool render(WPacket& w, byte l) const {
                if (l < 1 || 4 < l) {
                    return false;
                }
                w.packet_number(value, l);
                return true;
            }

            constexpr operator std::uint32_t() {
                return value;
            }
        };

    }  // namespace dnet
}  // namespace utils
