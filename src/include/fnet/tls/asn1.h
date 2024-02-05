/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// asn1 - asn1 subset for x509 certificate parser
#pragma once
#include "../../binary/number.h"

namespace futils {
    namespace fnet::asn1 {
        enum class Tag : byte {
            RESERVED = 0x0,
            BOOLEAN = 0x01,
            INTEGER = 0x02,
            BIT_STRING = 0x03,
            OCTET_STRING = 0x04,
            NULL_ = 0x05,
            OBJECT_IDENTIFIER = 0x06,
            UTF8String = 0x0C,
            SEQUENCE = 0x30,
            SET = 0x31,
            PrintableString = 0x13,
            IA5String = 0x16,
            UTCTime = 0x17,
            GeneralizedTime = 0x18,
        };

        enum class TagClass : byte {
            universal = 0b00,
            application = 0b01,
            context_specific = 0b10,
            private_ = 0b11,
        };

        constexpr TagClass tag_class(Tag tag) noexcept {
            return TagClass((byte(tag) >> 6) & 0x3);
        }

        constexpr bool is_structured(Tag tag) noexcept {
            return byte(tag) & 0x20;
        }

        constexpr Tag ctx_spec(byte n, bool structured = false) noexcept {
            return Tag((byte(TagClass::context_specific) << 6) | (structured ? 0x20 : 0x00) | (n & 0x1f));
        }

        struct Len {
           private:
            std::uint64_t len = 0;

           public:
            constexpr Len() = default;

            constexpr operator std::uint64_t() const {
                return len;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                if (r.empty()) {
                    return false;
                }
                if (r.top() < 0x80) {
                    len = r.top();
                    r.offset(1);
                    return true;
                }
                const auto len_len = (r.top() & 0x7F);
                if (len_len > 8) {
                    // this means, r has more data than it can count
                    // but such things would not occur
                    // use other system !
                    return false;
                }
                r.offset(1);
                auto [data, ok] = r.read(len_len);
                if (!ok) {
                    return false;
                }
                len = 0;
                for (size_t i = 0; i < len_len; i++) {
                    len |= (data[i] << ((len_len - i - 1) * 8));
                }
                return true;
            }
        };

        struct TLV {
            Tag tag{};
            Len len;
            view::rvec data;

            constexpr bool parse(binary::reader& r) {
                return binary::read_num(r, tag) &&
                       len.parse(r) &&
                       r.read(data, len);
            }

            constexpr bool iterate(size_t least, auto&& cb) const {
                binary::reader r{data};
                size_t count = 0;
                while (!r.empty()) {
                    TLV tlv;
                    if (!tlv.parse(r)) {
                        return false;
                    }
                    if (!cb(tlv, count, r.empty())) {
                        return false;
                    }
                    count++;
                }
                return count < least ? false : true;
            }

            constexpr std::pair<TLV, bool> explicit_unstruct() const {
                if (!is_structured(tag)) {
                    return {{}, false};
                }
                TLV tlv;
                binary::reader r{data};
                if (!tlv.parse(r) || !r.empty()) {
                    return {{}, false};  // not single strucutured
                }
                return {tlv, true};
            }
        };

        struct Integer {
           private:
            view::rvec data_;

           public:
            constexpr Integer() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::INTEGER) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr bool sign() const {
                return data_.size() ? data_.front() & 0x80 : false;
            }

            constexpr std::pair<std::uint64_t, bool> as_uint64() const {
                if (data_.empty() || data_.size() > 9 || sign()) {
                    return {0, false};
                }
                if (data_.size() == 9) {
                    if (data_.front() != 0) {
                        return {0, false};
                    }
                }
                std::uint64_t result = 0;
                size_t i = 0;
                if (data_.size() == 9) {
                    i = 1;
                }
                for (; i < data_.size(); i++) {
                    result |= std::uint64_t(data_[i] & (i == 0 ? 0x7F : 0xFF)) << ((data_.size() - i - 1) * 8);
                }
                return {result, true};
            }

            constexpr std::pair<std::int64_t, bool> as_int64() const {
                if (data_.empty() || data_.size() > 8) {
                    return {0, false};
                }
                std::uint64_t result = 0;
                for (size_t i = 0; i < data_.size(); i++) {
                    result |= std::uint64_t(data_[i] & (i == 0 ? 0x7F : 0xFF)) << ((data_.size() - i - 1) * 8);
                }
                std::int64_t out = result;
                if (sign()) {
                    out = -out;
                }
                return {out, true};
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct Bool {
            bool value = false;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::BOOLEAN) {
                if (tlv.tag != expect) {
                    return false;
                }
                if (tlv.data.size() != 1) {
                    return false;
                }
                value = tlv.data[0] != 0;
                return true;
            }
        };

        struct OID {
           private:
            view::rvec data_;

           public:
            constexpr OID() = default;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::OBJECT_IDENTIFIER) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr bool iterate(auto&& cb) {
                for (size_t i = 0; i < data_.size(); i++) {
                    if (i == 0) {
                        cb(data_[0] / 40, false);
                        cb(data_[0] % 40, i + 1 == data_.size());
                    }
                    else {
                        std::uint64_t val = data_[i] & 0x7F;
                        for (; i <= data_.size(); i++) {
                            if (i == data_.size()) {
                                return false;
                            }
                            if (data_[i] & 0x80) {
                                val <<= 7;  // val *= 128
                                val |= data_[i] & 0x7f;
                                continue;
                            }
                            break;
                        }
                        cb(val, i + 1 == data_.size());
                    }
                }
                return true;
            }

            constexpr bool to_array(auto&& to) {
                return iterate([&](auto num, bool last) {
                    to.push_back(num);
                });
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct BitString {
           private:
            view::rvec data_;

           public:
            constexpr BitString() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::BIT_STRING) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr byte unused_bits() const {
                return data_.empty() ? 0 : data_[0];
            }

            constexpr bool iterate(auto&& cb) const {
                if (data_.empty()) {
                    return false;
                }
                auto unused = unused_bits();
                if (unused > 7) {
                    return false;
                }
                if (data_.size() > 3) {
                    for (size_t i = 0; i < data_.size() - 2; i++) {
                        cb(bool(data_[i] & 0x80));
                        cb(bool(data_[i] & 0x40));
                        cb(bool(data_[i] & 0x20));
                        cb(bool(data_[i] & 0x10));
                        cb(bool(data_[i] & 0x08));
                        cb(bool(data_[i] & 0x04));
                        cb(bool(data_[i] & 0x02));
                        cb(bool(data_[i] & 0x01));
                    }
                }
                const auto i = data_.size() - 1;
                cb(bool(data_[i] & 0x80));
                if (unused == 7) return true;
                cb(bool(data_[i] & 0x40));
                if (unused == 6) return true;
                cb(bool(data_[i] & 0x20));
                if (unused == 5) return true;
                cb(bool(data_[i] & 0x10));
                if (unused == 4) return true;
                cb(bool(data_[i] & 0x08));
                if (unused == 3) return true;
                cb(bool(data_[i] & 0x04));
                if (unused == 2) return true;
                cb(bool(data_[i] & 0x02));
                if (unused == 1) return true;
                cb(bool(data_[i] & 0x01));
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct OctectString {
           private:
            view::rvec data_;

           public:
            constexpr OctectString() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::OCTET_STRING) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct UTCTime {
           private:
            view::rvec data_;

           public:
            constexpr UTCTime() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::UTCTime) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() const {
                return data_;
            }
        };

        struct GeneralizedTime {
           private:
            view::rvec data_;

           public:
            constexpr GeneralizedTime() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::GeneralizedTime) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() const {
                return data_;
            }
        };

        struct Time {
           private:
            TLV tlv_;

           public:
            constexpr Time() = default;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::UTCTime && tlv.tag != Tag::GeneralizedTime) {
                    return false;
                }
                tlv_ = tlv;
                return true;
            }

            std::pair<UTCTime, bool> as_UTCTime() const {
                UTCTime t;
                if (!t.parse(tlv_)) {
                    return {{}, false};
                }
                return {t, true};
            }

            std::pair<GeneralizedTime, bool> as_GeneralizedTime() const {
                GeneralizedTime t;
                if (!t.parse(tlv_)) {
                    return {{}, false};
                }
                return {t, true};
            }
        };
    }  // namespace fnet::asn1
}  // namespace futils
